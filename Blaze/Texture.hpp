
#pragma once

#include <stb_image.h>
#include "Context.hpp"
#include "util/createFunctions.hpp"

namespace blaze
{
	struct ImageData
	{
		uint8_t* data{ nullptr };
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		uint32_t numChannels{ 0 };
		uint32_t size{ 0 };
	};

	class TextureImage
	{
	private:
		util::Managed<ImageObject> image;
		util::Managed<VkImageView> imageView;
		util::Managed<VkSampler> imageSampler;
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		VkDescriptorImageInfo imageInfo{};
		bool is_valid{ false };
	public:
		TextureImage() noexcept
		{
		}

		TextureImage(const Context& context, const ImageData& image_data)
			: width(image_data.width),
			height(image_data.height),
			is_valid(false)
		{
			if (!image_data.data) return;

			using namespace util;
			VmaAllocator allocator = context.get_allocator();

			auto [stagingBuffer, stagingAlloc] = context.createBuffer(image_data.size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

			auto stagingBufferRAII = Managed<BufferObject>({ stagingBuffer, stagingAlloc }, [allocator](BufferObject& bo) { vmaDestroyBuffer(allocator, bo.buffer, bo.allocation); });

			void* bufferdata;
			vmaMapMemory(allocator, stagingAlloc, &bufferdata);
			memcpy(bufferdata, image_data.data, image_data.size);
			vmaUnmapMemory(allocator, stagingAlloc);

			image = Managed(context.createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY), [allocator](ImageObject& bo) { vmaDestroyImage(allocator, bo.image, bo.allocation); });

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
				barrier.subresourceRange.levelCount = 1;
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
				barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				srcStage = dstStage;
				dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

				vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				context.endTransferCommands(commandBuffer);
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}

			imageView = Managed(createImageView(context.get_device(), get_image(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT), [dev = context.get_device()](VkImageView& iv) { vkDestroyImageView(dev, iv, nullptr); });
			imageSampler = Managed(createSampler(context.get_device()), [dev = context.get_device()](VkSampler& sampler) { vkDestroySampler(dev, sampler, nullptr); });

			imageInfo.imageView = imageView.get();
			imageInfo.sampler = imageSampler.get();
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			is_valid = true;
		}

		TextureImage(TextureImage&& other) noexcept
			: image(std::move(other.image)),
			imageView(std::move(other.imageView)),
			imageSampler(std::move(other.imageSampler)),
			imageInfo(std::move(other.imageInfo)),
			width(other.width),
			height(other.height),
			is_valid(other.is_valid)
		{
		}

		TextureImage& operator=(TextureImage&& other) noexcept
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

		TextureImage(const TextureImage& other) = delete;
		TextureImage& operator=(const TextureImage& other) = delete;

		bool valid() const { return is_valid; }

		VkImage get_image() const { return image.get().image; }
		VkImageView get_imageView() const { return imageView.get(); }
		VkSampler get_imageSampler() const { return imageSampler.get(); }
		const VkDescriptorImageInfo& get_imageInfo() const { return imageInfo; }

	private:
		VkSampler createSampler(VkDevice device) const
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
			createInfo.maxLod = 0.0f;

			VkSampler sampler;
			auto result = vkCreateSampler(device, &createInfo, nullptr, &sampler);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Sampler creation failed with " + std::to_string(result));
			}

			return sampler;
		}
	};

	[[nodiscard]] TextureImage loadImage(const Context& context, const std::string& name);
}
