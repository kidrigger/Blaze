
#pragma once

#include <stb_image.h>
#include "Context.hpp"
#include "util/createFunctions.hpp"
#include <algorithm>
#include <cmath>

namespace blaze
{
	struct ImageData2D
	{
		uint8_t* data{ nullptr };
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		uint32_t numChannels{ 0 };
		uint32_t size{ 0 };
	};

	class Texture2D
	{
	private:
		util::Managed<ImageObject> image;
		util::Managed<VkImageView> imageView;
		util::Managed<VkSampler> imageSampler;
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		VkDescriptorImageInfo imageInfo{};
		uint32_t miplevels{ 1 };
		bool is_valid{ false };
	public:
		Texture2D() noexcept
		{
		}

		Texture2D(const Context& context, const ImageData2D& image_data, bool mipmapped = false)
			: width(image_data.width),
			height(image_data.height),
			is_valid(false)
		{
			if (!image_data.data) return;

			using namespace util;
			using std::max;

			VmaAllocator allocator = context.get_allocator();

			if (mipmapped)
			{
				miplevels = static_cast<uint32_t>(floor(log2(max(width, height)))) + 1;
			}

			auto [stagingBuffer, stagingAlloc] = context.createBuffer(image_data.size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

			auto stagingBufferRAII = Managed<BufferObject>({ stagingBuffer, stagingAlloc }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingAlloc, &bufferdata);
			memcpy(bufferdata, image_data.data, image_data.size);
			vmaUnmapMemory(allocator, stagingAlloc);

			image = Managed(context.createImage(width, height, miplevels, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_GPU_ONLY), [allocator](ImageObject& bo) { vmaDestroyImage(allocator, bo.image, bo.allocation); });

			try
			{
				VkCommandBuffer commandBuffer = context.startTransferCommands();

				VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

				VkImageMemoryBarrier barrier = {};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

				barrier.image = image.get().image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = miplevels;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = 0;

				vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				VkBufferImageCopy region = {};
				region.bufferOffset = 0;
				region.bufferRowLength = 0;
				region.bufferImageHeight = 0;
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = 0;
				region.imageSubresource.baseArrayLayer = 0;
				region.imageSubresource.layerCount = 1;

				region.imageOffset = { 0,0 };
				region.imageExtent = { width, height, 1 };

				vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, image.get().image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

				barrier.oldLayout = barrier.newLayout;
				barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				srcStage = dstStage;
				dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

				vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				// Mipmapping
				int32_t mipwidth = static_cast<int32_t>(width);
				int32_t mipheight = static_cast<int32_t>(height);
				barrier.subresourceRange.levelCount = 1;

				for (uint32_t i = 1; i < miplevels; i++)
				{
					barrier.subresourceRange.baseMipLevel = i - 1;
					barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

					vkCmdPipelineBarrier(commandBuffer,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
						0, nullptr,
						0, nullptr,
						1, &barrier);

					VkImageBlit blit = {};
					blit.srcOffsets[0] = { 0, 0, 0 };
					blit.srcOffsets[1] = { mipwidth, mipheight, 1 };
					blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.srcSubresource.mipLevel = i - 1;
					blit.srcSubresource.baseArrayLayer = 0;
					blit.srcSubresource.layerCount = 1;
					blit.dstOffsets[0] = { 0, 0, 0 };
					blit.dstOffsets[1] = { mipwidth > 1 ? mipwidth / 2 : 1, mipheight > 1 ? mipheight / 2 : 1, 1 };
					blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					blit.dstSubresource.mipLevel = i;
					blit.dstSubresource.baseArrayLayer = 0;
					blit.dstSubresource.layerCount = 1;

					vkCmdBlitImage(commandBuffer,
						image.get().image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						image.get().image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1, &blit,
						VK_FILTER_LINEAR);

					barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
					barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

					vkCmdPipelineBarrier(commandBuffer,
						VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
						0, nullptr,
						0, nullptr,
						1, &barrier);
					
					mipwidth = max(mipwidth / 2, 1);
					mipheight = max(mipheight / 2, 1);
				}

				barrier.subresourceRange.baseMipLevel = miplevels - 1;
				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(commandBuffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
					0, nullptr,
					0, nullptr,
					1, &barrier);

				context.endTransferCommands(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}

			imageView = Managed(createImageView(context.get_device(), get_image(), VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, miplevels), [dev = context.get_device()](VkImageView& iv) { vkDestroyImageView(dev, iv, nullptr); });
			imageSampler = Managed(createSampler(context.get_device(), miplevels), [dev = context.get_device()](VkSampler& sampler) { vkDestroySampler(dev, sampler, nullptr); });

			imageInfo.imageView = imageView.get();
			imageInfo.sampler = imageSampler.get();
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			is_valid = true;
		}

		Texture2D(Texture2D&& other) noexcept
			: image(std::move(other.image)),
			imageView(std::move(other.imageView)),
			imageSampler(std::move(other.imageSampler)),
			imageInfo(std::move(other.imageInfo)),
			width(other.width),
			height(other.height),
			is_valid(other.is_valid)
		{
		}

		Texture2D& operator=(Texture2D&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			image = std::move(other.image);
			imageView = std::move(other.imageView);
			imageSampler = std::move(other.imageSampler);
			imageInfo = std::move(other.imageInfo);
			width = other.width;
			height = other.height;
			is_valid = other.is_valid;
			return *this;
		}

		Texture2D(const Texture2D& other) = delete;
		Texture2D& operator=(const Texture2D& other) = delete;

		bool valid() const { return is_valid; }

		VkImage get_image() const { return image.get().image; }
		VkImageView get_imageView() const { return imageView.get(); }
		VkSampler get_imageSampler() const { return imageSampler.get(); }
		const VkDescriptorImageInfo& get_imageInfo() const { return imageInfo; }

	private:
		VkSampler createSampler(VkDevice device, uint32_t miplevels) const
		{
			VkSamplerCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			createInfo.magFilter = VK_FILTER_LINEAR;
			createInfo.minFilter = VK_FILTER_LINEAR;
			createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			createInfo.anisotropyEnable = VK_TRUE;
			createInfo.maxAnisotropy = 16;
			createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			createInfo.unnormalizedCoordinates = VK_FALSE;
			createInfo.compareEnable = VK_FALSE;
			createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			createInfo.mipLodBias = 0.0f;
			createInfo.minLod = 0.0f;
			createInfo.maxLod = static_cast<float>(miplevels);

			VkSampler sampler;
			auto result = vkCreateSampler(device, &createInfo, nullptr, &sampler);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Sampler creation failed with " + std::to_string(result));
			}

			return sampler;
		}
	};

	[[nodiscard]] Texture2D loadImage(const Context& context, const std::string& name);
}
