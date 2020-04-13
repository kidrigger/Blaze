
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/Context.hpp>
#include <util/Managed.hpp>
#include <vector>

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
	util::Managed<VkSwapchainKHR> swapchain;
	util::Unmanaged<VkFormat> format;
	util::Unmanaged<VkExtent2D> extent;

	util::UnmanagedVector<VkImage> images;
	util::ManagedVector<VkImageView> imageViews;

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
		return format.get();
	}
	const VkExtent2D& get_extent() const
	{
		return extent.get();
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
	 * @fn Swapchain(const Context& context)
	 *
	 * @brief Main constructor
	 *
	 * @param context Context reference.
	 */
	Swapchain(const Context& context) noexcept;

	/**
	 * @fn recreate(const Context& context)
	 *
	 * @brief Recreates the swapchain due to changes in screen size etc.
	 */
	void recreate(const Context& context) noexcept;

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
	std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> createSwapchain(const Context& context) const;
	std::vector<VkImage> getImages(const Context& context) const;
	std::vector<VkImageView> createImageViews(const Context& context) const;
};
} // namespace blaze