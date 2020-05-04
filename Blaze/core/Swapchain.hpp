
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/Context.hpp>
#include <vector>
#include <vkwrap/VkWrap.hpp>

namespace blaze
{
/**
 * @class Swapchain
 *
 * @brief Wrapper for all swapchain related objects.
 *
 * Contains the swapchain, the images, views, format and extent.
 */
class Swapchain
{
private:
	vkw::SwapchainKHR swapchain;
	VkFormat format;
	VkExtent2D extent;

	std::vector<VkImage> images;
	vkw::ImageViewVector imageViews;

	uint32_t count{0};

public:
	/**
	 * @name Getters
	 *
	 * @brief Getters for private members.
	 * @{
	 */
	const VkSwapchainKHR& get_swapchain() const
	{
		return swapchain.get();
	}
	const VkFormat& get_format() const
	{
		return format;
	}
	const VkExtent2D& get_extent() const
	{
		return extent;
	}
	const std::vector<VkImageView>& get_imageViews() const
	{
		return imageViews.get();
	}
	const VkImageView& get_imageView(uint32_t index) const
	{
		return imageViews[index];
	}
	const uint32_t& get_imageCount() const
	{
		return count;
	}
	/**
	 * @}
	 */

	/**
	 * @fn Swapchain()
	 *
	 * @brief Default Constructor
	 */
	Swapchain() noexcept
	{
	}

	/**
	 * @brief Main constructor
	 *
	 * @param context Pointer to the Vulkan Context in use.
     */
	Swapchain(const Context* context) noexcept;

	/**
	 * @brief Recreates the swapchain due to changes in screen size etc.
	 *
	 * @param context Pointer to the Vulkan Context in use.
     */
	void recreate(const Context* context) noexcept;

	/**
	 * @name Move Constructors
	 *
	 * @brief All the move constructors, copy is deleted.
	 *
	 * @{
	 */
	Swapchain(Swapchain&& other) noexcept;
	Swapchain& operator=(Swapchain&& other) noexcept;
	Swapchain(const Swapchain& other) = delete;
	Swapchain& operator=(const Swapchain& other) = delete;
	/**
	 * @}
	 */

private:
	void createSwapchain(const Context* context);
	std::vector<VkImage> getImages(const Context* context) const;
	vkw::ImageViewVector createImageViews(const Context* context) const;
};
} // namespace blaze
