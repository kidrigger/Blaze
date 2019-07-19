
#pragma once

#include "util/Managed.hpp"
#include "Context.hpp"

#include <map>
#include <vector>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
	class Renderer
	{
	private:
		std::pair<int, int> dimensions;
		bool isComplete{ false };

		util::Managed<VkSwapchainKHR> swapchain;
		util::Unmanaged<VkFormat> swapchainFormat;
		util::Unmanaged<VkExtent2D> swapchainExtent;
		util::UnmanagedVector<VkImage> swapchainImages;
		util::ManagedVector<VkImageView> swapchainImageViews;

	public:
		Renderer() noexcept
		{
		}

		Renderer(GLFWwindow* window, const Context& context) noexcept
		{
			using namespace std;
			using namespace util;
			glfwGetWindowSize(window, &dimensions.first, &dimensions.second);
			{
				auto [swapc, swapcFormat, swapcExtent] = createSwapchain(context);
				swapchain = Managed(swapc, [dev = context.get_device()](VkSwapchainKHR& swpc){ vkDestroySwapchainKHR(dev, swpc, nullptr); });
				swapchainFormat = swapcFormat;
				swapchainExtent = swapcExtent;
			}

			try
			{
				swapchainImages = getSwapchainImages(context);
				swapchainImageViews = ManagedVector(createSwapchainImageViews(context), [dev = context.get_device()](VkImageView& view) { vkDestroyImageView(dev, view, nullptr); });
			
				isComplete = true;
			}
			catch (std::exception& e)
			{
				std::cerr << "RENDERER_CREATION_FAILED: " << e.what() << std::endl;
				isComplete = false;
			}
		}

		Renderer(Renderer&& other) noexcept
			: dimensions(std::move(other.dimensions)),
			isComplete(other.isComplete),
			swapchain(std::move(other.swapchain)),
			swapchainFormat(std::move(other.swapchainFormat)),
			swapchainExtent(std::move(other.swapchainExtent)),
			swapchainImages(std::move(other.swapchainImages)),
			swapchainImageViews(std::move(other.swapchainImageViews))
		{
		}

		Renderer& operator=(Renderer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			dimensions = std::move(other.dimensions);
			isComplete = other.isComplete;
			swapchain = std::move(other.swapchain);
			swapchainFormat = std::move(other.swapchainFormat);
			swapchainExtent = std::move(other.swapchainExtent);
			swapchainImages = std::move(other.swapchainImages);
			swapchainImageViews = std::move(other.swapchainImageViews);
			return *this;
		}

		Renderer(const Renderer& other) = delete;
		Renderer& operator=(const Renderer& other) = delete;

		std::pair<int, int> get_dimensions() const { return dimensions; }
		VkSwapchainKHR get_swapchain() const { return swapchain.get(); }
		VkFormat get_swapchainFormat() const { return swapchainFormat.get(); }
		VkExtent2D get_swapchainExtent() const { return swapchainExtent.get(); }
		size_t get_swapchainImageViewsCount() const { return swapchainImageViews.size(); }
		VkImageView get_swapchainImageView(size_t index) const { return swapchainImageViews[index]; }

		bool complete() const { return isComplete; }
	private:
		std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> Renderer::createSwapchain(const Context& context) const;
		std::vector<VkImage> getSwapchainImages(const Context& context) const;
		std::vector<VkImageView> createSwapchainImageViews(const Context& context) const;
		VkRenderPass createRenderPass() const;
	};
}
