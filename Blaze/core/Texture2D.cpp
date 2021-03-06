
#include "Texture2D.hpp"

namespace blaze
{

[[nodiscard]] Texture2D loadImage(const Context* context, const std::string& name)
{
	ImageData2D image;
	int width, height, numChannels;
	image.data = stbi_load(name.c_str(), &width, &height, &numChannels, STBI_rgb_alpha);
	image.width = width;
	image.height = height;
	image.numChannels = numChannels;
	image.size = 4 * width * height;

	if (!image.data)
	{
		throw std::runtime_error("Image " + name + " could not be loaded.");
	}

	auto&& ti = Texture2D(context, image);
	stbi_image_free(image.data);

	return std::move(ti);
}

Texture2D::Texture2D(const Context* context, const ImageData2D& image_data, bool mipmapped)
	: width(image_data.width), height(image_data.height), format(image_data.format), layout(image_data.layout),
	  usage(image_data.usage), access(image_data.access), aspect(image_data.aspect), tiling(image_data.tiling),
	  layerCount(image_data.layerCount), anisotropy(image_data.anisotropy), is_valid(false)
{
	using namespace util;
	using std::max;

	VmaAllocator allocator = context->get_allocator();

	if (mipmapped)
	{
		miplevels = static_cast<uint32_t>(floor(log2(max(width, height)))) + 1;
	}

	if (!image_data.data)
	{
		image = context->createImage(width, height, miplevels, layerCount, format, tiling, usage,
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
		barrier.subresourceRange.layerCount = layerCount;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;

		vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		context->flushCommandBuffer(commandBuffer);

		allViews =
			vkw::ImageView(createImageView(context->get_device(), get_image(),
										   (layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D),
										   format, aspect, miplevels, layerCount),
						   context->get_device());
		std::vector<VkImageView> views(layerCount);
		uint32_t index = 0;
		for (auto& view : views)
		{
			view = createImageView(context->get_device(), get_image(), VK_IMAGE_VIEW_TYPE_2D, format, aspect, miplevels,
								   1, index);
			index++;
		}
		imageViews = vkw::ImageViewVector(std::move(views), context->get_device());
		imageSampler =
			vkw::Sampler(createSampler(context->get_device(), miplevels, image_data.samplerAddressMode, anisotropy),
						 context->get_device());

		imageInfo.imageView = allViews.get();
		imageInfo.sampler = imageSampler.get();
		imageInfo.imageLayout = layout;

		is_valid = true;
		return;
	}

	auto stagingBuffer = context->createBuffer(image_data.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	void* bufferdata;
	vmaMapMemory(allocator, stagingBuffer.allocation, &bufferdata);
	memcpy(bufferdata, image_data.data, image_data.size);
	vmaUnmapMemory(allocator, stagingBuffer.allocation);

	image = context->createImage(width, height, miplevels, layerCount, format, VK_IMAGE_TILING_OPTIMAL, usage,
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
		barrier.subresourceRange.layerCount = layerCount;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = 0;

		vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = aspect;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = layerCount;

		region.imageOffset = {0, 0};
		region.imageExtent = {width, height, 1};

		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.handle, image.get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
							   &region);

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

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
								 nullptr, 0, nullptr, 1, &barrier);

			VkImageBlit blit = {};
			blit.srcOffsets[0] = {0, 0, 0};
			blit.srcOffsets[1] = {mipwidth, mipheight, 1};
			blit.srcSubresource.aspectMask = aspect;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = {0, 0, 0};
			blit.dstOffsets[1] = {mipwidth > 1 ? mipwidth / 2 : 1, mipheight > 1 ? mipheight / 2 : 1, 1};
			blit.dstSubresource.aspectMask = aspect;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = layerCount;

			vkCmdBlitImage(commandBuffer, image.get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.get(),
						   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = layout;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = access;

			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								 0, 0, nullptr, 0, nullptr, 1, &barrier);

			mipwidth = max(mipwidth / 2, 1);
			mipheight = max(mipheight / 2, 1);
		}

		barrier.subresourceRange.baseMipLevel = miplevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = layout;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = access;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
							 nullptr, 0, nullptr, 1, &barrier);

		context->flushCommandBuffer(commandBuffer);
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	allViews = vkw::ImageView(createImageView(context->get_device(), get_image(),
											  (layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D),
											  format, aspect, miplevels, layerCount),
							  context->get_device());
	std::vector<VkImageView> views(layerCount);
	uint32_t index = 0;
	for (auto& view : views)
	{
		view = createImageView(context->get_device(), get_image(), VK_IMAGE_VIEW_TYPE_2D, format, aspect, miplevels, 1,
							   index);
		index++;
	}
	imageViews = vkw::ImageViewVector(std::move(views), context->get_device());
	imageSampler =
		vkw::Sampler(createSampler(context->get_device(), miplevels, image_data.samplerAddressMode, anisotropy),
					 context->get_device());

	imageInfo.imageView = allViews.get();
	imageInfo.sampler = imageSampler.get();
	imageInfo.imageLayout = layout;

	is_valid = true;
}

Texture2D::Texture2D(Texture2D&& other) noexcept
	: image(std::move(other.image)), imageViews(std::move(other.imageViews)), allViews(std::move(other.allViews)),
	  imageSampler(std::move(other.imageSampler)), imageInfo(std::move(other.imageInfo)), width(other.width),
	  height(other.height), format(other.format), layout(other.layout), usage(other.usage), access(other.access),
	  aspect(other.aspect), tiling(other.tiling), miplevels(other.miplevels), layerCount(other.layerCount),
	  anisotropy(other.anisotropy), is_valid(other.is_valid)
{
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	image = std::move(other.image);
	imageViews = std::move(other.imageViews);
	allViews = std::move(other.allViews);
	imageSampler = std::move(other.imageSampler);
	imageInfo = std::move(other.imageInfo);
	width = other.width;
	height = other.height;
	format = other.format;
	layout = other.layout;
	usage = other.usage;
	access = other.access;
	aspect = other.aspect;
	tiling = other.tiling;
	miplevels = other.miplevels;
	layerCount = other.layerCount;
	anisotropy = other.anisotropy;
	is_valid = other.is_valid;
	return *this;
}

void Texture2D::transferLayout(VkCommandBuffer cmdbuffer, VkImageLayout newImageLayout, VkAccessFlags dstAccess,
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
	barrier.subresourceRange.layerCount = layerCount;
	barrier.srcAccessMask = access;
	barrier.dstAccessMask = dstAccess;

	vkCmdPipelineBarrier(cmdbuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	access = dstAccess;
	layout = newImageLayout;
	imageInfo.imageLayout = newImageLayout;
}

void Texture2D::implicitTransferLayout(VkImageLayout newImageLayout, VkAccessFlags dstAccess)
{
	layout = newImageLayout;
	imageInfo.imageLayout = newImageLayout;
	access = dstAccess;
}
} // namespace blaze
