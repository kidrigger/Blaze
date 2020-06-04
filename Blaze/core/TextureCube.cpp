
#include "TextureCube.hpp"
#include "Primitives.hpp"
#include "Texture2D.hpp"
#include "util/files.hpp"
#include "util/processing.hpp"

namespace blaze
{

[[nodiscard]] TextureCube loadImageCube(const Context* context, const std::vector<std::string>& names_lrudfb,
										bool mipmapped)
{
	ImageDataCube image;
	int width, height, numChannels;

	int layer = 0;
	for (const auto& name : names_lrudfb)
	{
		image.data[layer] = stbi_load(name.c_str(), &width, &height, &numChannels, STBI_rgb_alpha);

		if (!image.data[layer])
		{
			throw std::runtime_error("Image " + name + " could not be loaded.");
		}

		layer++;
	}
	image.width = width;
	image.height = height;
	image.numChannels = numChannels;
	image.layerSize = 4 * width * height;
	image.size = 6 * image.layerSize;

	auto&& ti = TextureCube(context, image, mipmapped);

	for (int i = 0; i < 6; i++)
	{
		stbi_image_free(image.data[i]);
	}

	return std::move(ti);
}

[[nodiscard]] TextureCube loadImageCube(const Context* context, const std::string& name, bool mipmapped)
{
	auto ext = name.substr(name.find_last_of('.'));
	if (ext != ".hdr")
	{
		throw std::runtime_error("Can't load " + ext + " files.");
	}

	int width, height, numChannels;
	float* data = stbi_loadf(name.c_str(), &width, &height, &numChannels, 0);

	float* data_rgba = new float[size_t(width) * size_t(height) * 4];
	if (numChannels == 3)
	{
		for (size_t i = 0; i < size_t(width) * size_t(height); i++)
		{
			for (size_t c = 0; c < 3; c++)
			{
				data_rgba[4 * i + c] = data[3 * i + c];
			}
			data_rgba[4 * i + 3] = 1.0f;
		}
	}
	else
	{
		memcpy(data_rgba, data, size_t(width) * size_t(height) * 4 * 4);
	}

	stbi_image_free(data);

	ImageData2D eqvData = {};
	eqvData.data = reinterpret_cast<uint8_t*>(data_rgba);
	eqvData.width = width;
	eqvData.height = height;
	eqvData.numChannels = 4;
	eqvData.size = width * height * 4 * sizeof(float);
	eqvData.format = VK_FORMAT_R32G32B32A32_SFLOAT;

	Texture2D equirect(context, eqvData, false);
	delete[] data_rgba;

	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;
	std::vector<VkDescriptorPoolSize> poolSizes = {poolSize};
	auto dsPool =
		vkw::DescriptorPool(util::createDescriptorPool(context->get_device(), poolSizes, 2), context->get_device());

	std::vector<VkDescriptorSetLayoutBinding> cubemapLayoutBindings = {
		{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
	auto dsLayout = vkw::DescriptorSetLayout(
		util::createDescriptorSetLayout(context->get_device(), cubemapLayoutBindings), context->get_device());

	auto createDescriptorSet = [device = context->get_device()](VkDescriptorSetLayout layout, VkDescriptorPool pool,
															   const Texture2D& texture, uint32_t binding) {
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout;

		VkDescriptorSet descriptorSet;
		auto result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
		}

		VkDescriptorImageInfo info = {};
		info.imageView = texture.get_imageView();
		info.sampler = texture.get_imageSampler();
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.dstSet = descriptorSet;
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.pImageInfo = &info;

		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

		return descriptorSet;
	};

	auto ds = vkw::DescriptorSet(createDescriptorSet(dsLayout.get(), dsPool.get(), equirect, 1));

	util::Texture2CubemapInfo<util::Ignore> convertInfo{"shaders/env/vEqvrect2Cube.vert.spv",
														"shaders/env/fEqvrect2Cube.frag.spv", ds.get(), dsLayout.get(),
														static_cast<uint32_t>(height)};

	return util::Process<decltype(convertInfo.pcb)>::convertDescriptorToCubemap(context, convertInfo);
}

TextureCube::TextureCube(const Context* context, const ImageDataCube& image_data, bool mipmapped)
	: width(image_data.width), height(image_data.height), format(image_data.format), layout(image_data.layout),
	  usage(image_data.usage), access(image_data.access), aspect(image_data.aspect), is_valid(false)
{

	using namespace util;
	using std::max;

	VmaAllocator allocator = context->get_allocator();

	if (mipmapped)
	{
		miplevels = static_cast<uint32_t>(floor(log2(max(width, height)))) + 1;
	}

	if (!image_data.data[0] || !image_data.data[1] || !image_data.data[2] || !image_data.data[3] ||
		!image_data.data[4] || !image_data.data[5])
	{
		image = context->createImageCube(width, height, miplevels, format, VK_IMAGE_TILING_OPTIMAL, usage,
										 VMA_MEMORY_USAGE_GPU_ONLY);

		VkCommandBuffer commandBuffer = context->startCommandBufferRecord();

		VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = layout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.image = image.get();
		barrier.subresourceRange.aspectMask = aspect;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = miplevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 6;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;

		vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		context->flushCommandBuffer(commandBuffer);

		imageView = vkw::ImageView(
			createImageView(context->get_device(), get_image(), VK_IMAGE_VIEW_TYPE_CUBE, format, aspect, miplevels, 6),
			context->get_device());
		imageSampler = vkw::Sampler(createSampler(context->get_device(), miplevels, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE),
							   context->get_device());

		imageInfo.imageView = imageView.get();
		imageInfo.sampler = imageSampler.get();
		imageInfo.imageLayout = layout;
		return;
	}

	auto stagingBuffer = context->createBuffer(image_data.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* bufferdata;
	vmaMapMemory(allocator, stagingBuffer.allocation, &bufferdata);
	{
		uint8_t* dest = static_cast<uint8_t*>(bufferdata);
		for (int i = 0; i < 6; i++)
		{
			memcpy(dest + image_data.layerSize * i, image_data.data[i], image_data.layerSize);
		}
	}
	vmaUnmapMemory(allocator, stagingBuffer.allocation);

	image = context->createImageCube(width, height, miplevels, format, VK_IMAGE_TILING_OPTIMAL, usage,
									 VMA_MEMORY_USAGE_GPU_ONLY);

	try
	{
		VkCommandBuffer commandBuffer = context->startCommandBufferRecord();

		VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		barrier.image = image.get();
		barrier.subresourceRange.aspectMask = aspect;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = miplevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 6;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;

		vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		barrier.subresourceRange.layerCount = 1;

		for (int face = 0; face < 6; face++)
		{
			VkBufferImageCopy region = {};
			region.bufferOffset = face * image_data.layerSize;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;
			region.imageSubresource.aspectMask = aspect;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = face;
			region.imageSubresource.layerCount = 1;

			region.imageOffset = {0, 0};
			region.imageExtent = {width, height, 1};

			vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.handle, image.get(),
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = 0;
			barrier.subresourceRange.baseArrayLayer = face;
			barrier.subresourceRange.layerCount = 1;
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

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
									 0, nullptr, 0, nullptr, 1, &barrier);

				VkImageBlit blit = {};
				blit.srcOffsets[0] = {0, 0, 0};
				blit.srcOffsets[1] = {mipwidth, mipheight, 1};
				blit.srcSubresource.aspectMask = aspect;
				blit.srcSubresource.mipLevel = i - 1;
				blit.srcSubresource.baseArrayLayer = face;
				blit.srcSubresource.layerCount = 1;
				blit.dstOffsets[0] = {0, 0, 0};
				blit.dstOffsets[1] = {mipwidth > 1 ? mipwidth / 2 : 1, mipheight > 1 ? mipheight / 2 : 1, 1};
				blit.dstSubresource.aspectMask = aspect;
				blit.dstSubresource.mipLevel = i;
				blit.dstSubresource.baseArrayLayer = face;
				blit.dstSubresource.layerCount = 1;

				vkCmdBlitImage(commandBuffer, image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							   image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

				barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barrier.newLayout = layout;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barrier.dstAccessMask = access;

				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
									 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

				mipwidth = max(mipwidth / 2, 1);
				mipheight = max(mipheight / 2, 1);
			}

			barrier.subresourceRange.baseMipLevel = miplevels - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = layout;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = access;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								 0, 0, nullptr, 0, nullptr, 1, &barrier);
		}

		context->flushCommandBuffer(commandBuffer);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	imageView = vkw::ImageView(
		createImageView(context->get_device(), get_image(), VK_IMAGE_VIEW_TYPE_CUBE, format, aspect, miplevels, 6),
		context->get_device());
	imageSampler =
		vkw::Sampler(createSampler(context->get_device(), miplevels, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE),
					 context->get_device());

	imageInfo.imageView = imageView.get();
	imageInfo.sampler = imageSampler.get();
	imageInfo.imageLayout = layout;

	is_valid = true;
}

TextureCube::TextureCube(TextureCube&& other) noexcept
	: image(std::move(other.image)), imageView(std::move(other.imageView)), imageSampler(std::move(other.imageSampler)),
	  imageInfo(std::move(other.imageInfo)), width(other.width), height(other.height), format(other.format),
	  layout(other.layout), usage(other.usage), access(other.access), aspect(other.aspect), miplevels(other.miplevels),
	  is_valid(other.is_valid)
{
}

TextureCube& TextureCube::operator=(TextureCube&& other) noexcept
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
	format = other.format;
	layout = other.layout;
	usage = other.usage;
	access = other.access;
	aspect = other.aspect;
	miplevels = other.miplevels;
	is_valid = other.is_valid;
	return *this;
}

void TextureCube::transferLayout(VkCommandBuffer cmdbuffer, VkImageLayout newImageLayout, VkAccessFlags dstAccess,
								 VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	auto currentLayout = layout;

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = currentLayout;
	barrier.newLayout = newImageLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image.get();
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = miplevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 6;
	barrier.srcAccessMask = access;
	barrier.dstAccessMask = dstAccess;

	vkCmdPipelineBarrier(cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	access = dstAccess;
	layout = newImageLayout;
	imageInfo.imageLayout = newImageLayout;
}

void TextureCube::implicitTransferLayout(VkImageLayout newImageLayout, VkAccessFlags dstAccess)
{
	layout = newImageLayout;
	imageInfo.imageLayout = newImageLayout;
	access = dstAccess;
}
} // namespace blaze
