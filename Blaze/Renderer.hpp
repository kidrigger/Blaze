
#pragma once

#include "util/Managed.hpp"
#include "Context.hpp"

#include <map>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
	class Renderer
	{
	private:
		std::pair<int, int> dimensions;

		util::Managed<VkSwapchainKHR> swapchain;
		util::Unmanaged<VkFormat> swapchainFormat;
		util::Unmanaged<VkExtent2D> swapchainExtent;

	public:
		Renderer() noexcept
		{
		}

		Renderer(GLFWwindow* window, const Context& context) noexcept
		{
			using namespace std;
			using namespace util;
			glfwGetWindowSize(window, &dimensions.first, &dimensions.second);
			auto [swapc, swapcFormat, swapcExtent] = createSwapchain(context);
			swapchain = Managed(swapc, [dev = context.get_device()](VkSwapchainKHR& swpc){ vkDestroySwapchainKHR(dev, swpc, nullptr); });
			swapchainFormat = swapcFormat;
			swapchainExtent = swapcExtent;
		}

		Renderer(Renderer&& other) noexcept
			: dimensions(std::move(other.dimensions)),
			swapchain(std::move(other.swapchain)),
			swapchainFormat(std::move(other.swapchainFormat)),
			swapchainExtent(std::move(other.swapchainExtent))
		{
		}

		Renderer& operator=(Renderer&& other) noexcept
		{
			dimensions = std::move(other.dimensions);
			swapchain = std::move(other.swapchain);
			swapchainFormat = std::move(other.swapchainFormat);
			swapchainExtent = std::move(other.swapchainExtent);
			return *this;
		}

		Renderer(const Renderer& other) = delete;
		Renderer& operator=(const Renderer& other) = delete;

		std::pair<int, int> get_dimensions() const { return dimensions; }
		VkSwapchainKHR get_swapchain() const { return swapchain.get(); }
		VkFormat get_swapchainFormat() const { return swapchainFormat.get(); }
		VkExtent2D get_swapchainExtent() const { return swapchainExtent.get(); }

	private:
		std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> Renderer::createSwapchain(const Context& context) const;
	};
}
