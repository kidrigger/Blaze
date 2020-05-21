
#pragma once

#include <algorithm>
#include <cmath>
#include <core/Context.hpp>
#include <cstring>
#include <thirdparty/stbi/stb_image.h>
#include <util/createFunctions.hpp>
#include <util/Managed.hpp>

namespace blaze
{
/**
 * @struct ImageDataCube
 *
 * @brief Data for constructing a TextureCube
 */
struct ImageDataCube
{
	uint8_t* data[6]{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
	uint32_t width{0};
	uint32_t height{0};
	uint32_t numChannels{0};
	uint32_t layerSize{0};
	uint32_t size{0};
	VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
	VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
							VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
	VkImageLayout layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	VkAccessFlags access{VK_ACCESS_SHADER_READ_BIT};
	VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};
};

/**
 * @class TextureCube
 *
 * @brief A wrapper over a vulkan texture that contains all the required data.
 *
 * A Cube texture that contains the image, memory, view, sampler and metadata.
 */
class TextureCube
{
private:
	util::Managed<ImageObject> image;
	util::Managed<VkImageView> imageView;
	util::Managed<VkSampler> imageSampler;
	uint32_t width{0};
	uint32_t height{0};
	VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
	VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
							VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
	VkImageLayout layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	VkAccessFlags access{VK_ACCESS_SHADER_READ_BIT};
	VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};
	VkDescriptorImageInfo imageInfo{};
	uint32_t miplevels{1};
	bool is_valid{false};

public:
	/**
	 * @fn TextureCube()
	 *
	 * @brief Default Constructor.
	 */
	TextureCube() noexcept
	{
	}

	/**
	 * @fn TextureCube(const Context* context, const ImageDataCube& image_data, bool mipmapped = true)
	 *
	 * @brief Main constructor for TextureCube.
	 *
	 * @param context The current Vulkan Context.
	 * @param image_data The ImageDataCube stuct containing the initialization information.
	 * @param mipmapped Enabling mipmapping.
	 */
	TextureCube(const Context* context, const ImageDataCube& image_data, bool mipmapped = true);

	/**
	 * @name Move Constructors.
	 *
	 * @brief Move only, Copy deleted.
	 *
	 * @{
	 */
	TextureCube(TextureCube&& other) noexcept;
	TextureCube& operator=(TextureCube&& other) noexcept;
	TextureCube(const TextureCube& other) = delete;
	TextureCube& operator=(const TextureCube& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @fn valid()
	 *
	 * @brief Checks if a texture is valid and usable.
	 *
	 * A texture is considered valid if it is constructed using
	 * the main constructor and is initialized successfully.
	 *
	 * @returns \a true If is properly constructed.
	 * @returns \a false If is default constructed or had an error.
	 */
	bool valid() const
	{
		return is_valid;
	}

	/**
	 * @name Getters.
	 *
	 * @brief Getters for private variables.
	 *
	 * @{
	 */
	const VkImage& get_image() const
	{
		return image.get().image;
	}
	const VkImageView& get_imageView() const
	{
		return imageView.get();
	}
	const VkSampler& get_imageSampler() const
	{
		return imageSampler.get();
	}
	const VkDescriptorImageInfo& get_imageInfo() const
	{
		return imageInfo;
	}
	const VkFormat& get_format() const
	{
		return format;
	}
	const VkImageUsageFlags& get_usage() const
	{
		return usage;
	}
	const VkImageLayout& get_layout() const
	{
		return layout;
	}
	const VkAccessFlags& get_access() const
	{
		return access;
	}
	const VkImageAspectFlags& get_aspect() const
	{
		return aspect;
	}
	uint32_t get_miplevels() const
	{
		return miplevels;
	}

	uint32_t get_width() const
	{
		return width;
	}

	uint32_t get_height() const
	{
		return height;
	}
	/**
	 * @}
	 */

	/**
	 * @brief Transfers layout using a pipeline barrier.
	 *
	 * Transfer layouts use a pipeline barrier to change the layouts between the two specified stages.
	 *
	 * @param cmdbuffer The command buffer to execute the transfer on.
	 * @param newImageLayout The final image layout for the texture.
	 * @param dstAccess The access flag of the final image.
	 * @param srcStageMask The stages after which the texture can start transfer.
	 * @param dstStageMask The stages by which the texture needs to finish transfer.
	 */
	void transferLayout(VkCommandBuffer cmdbuffer, VkImageLayout newImageLayout, VkAccessFlags dstAccess = 0,
						VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

	/**
	 * @brief Changes the image layout and access to note the change occuring due to a renderpass.
	 *
	 * @param newImageLayout The image layout to change to.
	 * @param dstAccess The access flag to set to.
	 */
	void implicitTransferLayout(VkImageLayout newImageLayout, VkAccessFlags dstAccess);

private:
	VkSampler createSampler(VkDevice device, uint32_t miplevels) const;
};

[[nodiscard]] TextureCube loadImageCube(const Context* context, const std::vector<std::string>& names_fbudrl,
										bool mipmapped = true);
[[nodiscard]] TextureCube loadImageCube(const Context* context, const std::string& name, bool mipmapped = true);
} // namespace blaze
