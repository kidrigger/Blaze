
#pragma once

#include <algorithm>
#include <cmath>
#include <core/Context.hpp>
#include <cstring>
#include <thirdparty/stbi/stb_image.h>
#include <util/createFunctions.hpp>

namespace blaze
{
/**
 * @struct ImageData2D
 *
 * @brief Data for constructing a Texture2D
 */
struct ImageData2D
{
	/// @brief The loaded data for the texture (null if no data)
	uint8_t* data{nullptr};

	/// @brief Width of the texture.
	uint32_t width{0};

	/// @brief Height of the texture.
	uint32_t height{0};

	/// @brief Number of color channels in the texture.
	uint32_t numChannels{0};

	/// @brief Size of the data.
	uint32_t size{0};

	/// @brief The format of the texture.
	VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};

	/// @brief Usage flags for the image.
	VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
							VK_BUFFER_USAGE_TRANSFER_SRC_BIT};

	/// @brief The initial layout of the texture.
	VkImageLayout layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

	/// @brief The access flag for the image.
	VkAccessFlags access{VK_ACCESS_SHADER_READ_BIT};

	/// @brief The aspect the image is used as.
	VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};

	/// @brief The tiling of the image.
	VkImageTiling tiling{VK_IMAGE_TILING_OPTIMAL};

	/// @brief Address mode for the sampler.
	VkSamplerAddressMode samplerAddressMode{VK_SAMPLER_ADDRESS_MODE_REPEAT};

	/// @brief Number of layers in the image.
	uint32_t layerCount{1};

	/// @brief Activate Anisotropy
	VkBool32 anisotropy{VK_TRUE};
};

/**
 * @class Texture2D
 *
 * @brief A wrapper over a vulkan texture that contains all the required data.
 *
 * A 2D texture that contains the image, memory, view, sampler and metadata.
 */
class Texture2D
{
private:
	vkw::Image image;
	vkw::ImageView allViews;
	vkw::ImageViewVector imageViews;
	vkw::Sampler imageSampler;
	uint32_t width{0};
	uint32_t height{0};
	VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
	VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
							VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
	VkImageLayout layout{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
	VkAccessFlags access{VK_ACCESS_SHADER_READ_BIT};
	VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};
	VkImageTiling tiling{VK_IMAGE_TILING_OPTIMAL};
	VkDescriptorImageInfo imageInfo{};
	uint32_t miplevels{1};
	uint32_t layerCount{1};
	VkBool32 anisotropy{VK_TRUE};
	bool is_valid{false};

public:
	/**
	 * @fn Texture2D()
	 *
	 * @brief Default Constructor.
	 */
	Texture2D() noexcept
	{
	}

	/**
	 * @fn Texture2D(const Context* context, const ImageData2D& image_data, bool mipmapped = false)
	 *
	 * @brief Constructor for the texture.
	 *
	 * @param context The current Vulkan Context.
	 * @param image_data The ImageData2D struct containing the initialization information.
	 * @param mipmapped Enabling mipmapping.
	 */
	Texture2D(const Context* context, const ImageData2D& image_data, bool mipmapped = false);

	/**
	 * @name Move Constuctors.
	 *
	 * @brief Move only, Copy deleted.
	 *
	 * @{
	 */
	Texture2D(Texture2D&& other) noexcept;
	Texture2D& operator=(Texture2D&& other) noexcept;
	Texture2D(const Texture2D& other) = delete;
	Texture2D& operator=(const Texture2D& other) = delete;
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
	 * @brief Getters for private members.
	 *
	 * @{
	 */
	const VkImage& get_image() const
	{
		return image.get();
	}
	const VkImageView& get_allImageViews() const
	{
		return allViews.get();
	}
	const VkImageView& get_imageView(uint32_t index = 0) const
	{
		return imageViews[index];
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
	uint32_t get_layerCount() const
	{
		return layerCount;
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
	 * @param dstAccess The access flag of the final image.
	 */
	void implicitTransferLayout(VkImageLayout newImageLayout, VkAccessFlags dstAccess);
};

[[nodiscard]] Texture2D loadImage(const Context* context, const std::string& name);
} // namespace blaze
