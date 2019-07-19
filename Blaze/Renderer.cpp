
#include "Renderer.hpp"
#include "util/DeviceSelection.hpp"

#include <algorithm>
#include <string>
#include <stdexcept>

#undef max
#undef min

namespace blaze
{
	std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> Renderer::createSwapchain(const Context& context) const
	{
		util::SwapchainSupportDetails swapchainSupport = util::getSwapchainSupport(context.get_physicalDevice(), context.get_surface());

		VkSurfaceFormatKHR surfaceFormat = swapchainSupport.formats[0];
		for (const auto& availableFormat : swapchainSupport.formats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				surfaceFormat = availableFormat;
				break;
			}
		}

		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& availablePresentMode : swapchainSupport.presentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = availablePresentMode;
				break;
			}
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				presentMode = availablePresentMode;
			}
		}

		VkExtent2D swapExtent;
		auto capabilities = swapchainSupport.capabilities;
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			swapExtent = capabilities.currentExtent;
		}
		else
		{
			swapExtent.width = std::clamp(swapExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			swapExtent.height = std::clamp(swapExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0)
		{
			imageCount = std::min(imageCount, capabilities.maxImageCount);
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = context.get_surface();
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = swapExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		auto queueIndices = context.get_queueFamilyIndices();
		uint32_t queueFamilyIndices[] = { queueIndices.graphicsIndex.value(), queueIndices.presentIndex.value() };
		if (queueIndices.graphicsIndex != queueIndices.presentIndex)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		VkSwapchainKHR swapchain;
		auto result = vkCreateSwapchainKHR(context.get_device(), &createInfo, nullptr, &swapchain);
		if (result == VK_SUCCESS)
		{
			return std::make_tuple(swapchain, surfaceFormat.format, swapExtent);
		}
		throw std::runtime_error("Swapchain creation failed with " + std::to_string(result));
	}

	std::vector<VkImage> Renderer::getSwapchainImages(const Context& context) const
	{
		uint32_t swapchainImageCount = 0;
		std::vector<VkImage> swapchainImages;
		vkGetSwapchainImagesKHR(context.get_device(), swapchain.get(), &swapchainImageCount, nullptr);
		swapchainImages.resize(swapchainImageCount);
		vkGetSwapchainImagesKHR(context.get_device(), swapchain.get(), &swapchainImageCount, swapchainImages.data());
		return swapchainImages;
	}

	std::vector<VkImageView> Renderer::createSwapchainImageViews(const Context& context) const
	{
		std::vector<VkImageView> swapchainImageViews(swapchainImages.size());
		for (size_t i = 0; i < swapchainImages.size(); i++)
		{
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapchainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapchainFormat.get();
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			auto result = vkCreateImageView(context.get_device(), &createInfo, nullptr, &swapchainImageViews[i]);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create image view with " + std::to_string(result));
			}
		}
		return swapchainImageViews;
	}

	VkRenderPass Renderer::createRenderPass() const { return VK_NULL_HANDLE; }
}
