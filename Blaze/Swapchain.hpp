
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "util/Managed.hpp"
#include "Context.hpp"
#include <vector>

namespace blaze
{
	class Swapchain
	{
	private:
		util::Managed<VkSwapchainKHR> swapchain;
		util::Unmanaged<VkFormat> format;
		util::Unmanaged<VkExtent2D> extent;

		util::UnmanagedVector<VkImage> images;
		util::ManagedVector<VkImageView> imageViews;

		uint32_t count{ 0 };

	public:
		const VkSwapchainKHR& get_swapchain() const { return swapchain.get(); }
		const VkFormat& get_format() const { return format.get(); }
		const VkExtent2D& get_extent() const { return extent.get(); }
		const std::vector<VkImageView>& get_imageViews() const { return imageViews.get(); }
		const VkImageView& get_imageView(uint32_t index) const { return imageViews[index]; }
		const uint32_t& get_imageCount() const { return count; }

		Swapchain() noexcept
		{
		}

		Swapchain(const Context& context) noexcept
		{
			using namespace util;
			auto [swapc, swapcFormat, swapcExtent] = createSwapchain(context);
			swapchain = Managed(swapc, [dev = context.get_device()](VkSwapchainKHR& swpc){ vkDestroySwapchainKHR(dev, swpc, nullptr); });
			format = swapcFormat;
			extent = swapcExtent;

			images = getImages(context);
			imageViews = ManagedVector(createImageViews(context), [dev = context.get_device()](VkImageView& view) { vkDestroyImageView(dev, view, nullptr); });

			count = static_cast<uint32_t>(images.size());
		}

		void recreate(const Context& context) noexcept
		{
			using namespace util;
			auto [swapc, swapcFormat, swapcExtent] = createSwapchain(context);
			swapchain = Managed(swapc, [dev = context.get_device()](VkSwapchainKHR& swpc){ vkDestroySwapchainKHR(dev, swpc, nullptr); });
			format = swapcFormat;
			extent = swapcExtent;

			images = getImages(context);
			imageViews = ManagedVector(createImageViews(context), [dev = context.get_device()](VkImageView& view) { vkDestroyImageView(dev, view, nullptr); });

			count = static_cast<uint32_t>(images.size());
		}

		Swapchain(Swapchain&& other) noexcept
			: swapchain(std::move(other.swapchain)),
			format(std::move(other.format)),
			extent(std::move(other.extent)),
			images(std::move(other.images)),
			imageViews(std::move(other.imageViews)),
			count(other.count)
		{
		}

		Swapchain& operator=(Swapchain&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			swapchain = std::move(other.swapchain);
			format = std::move(other.format);
			extent = std::move(other.extent);
			images = std::move(other.images);
			imageViews = std::move(other.imageViews);
			count = other.count;

			return *this;
		}

		Swapchain(const Swapchain& other) = delete;
		Swapchain& operator=(const Swapchain& other) = delete;

	private:

		std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> createSwapchain(const Context& context) const;

		std::vector<VkImage> getImages(const Context& context) const;

		std::vector<VkImageView> createImageViews(const Context& context) const;
	};
}
