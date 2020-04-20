
#include "Swapchain.hpp"

#include "util/createFunctions.hpp"
#include <string>
#include <algorithm>

namespace blaze
{
void Swapchain::createSwapchain(const Context* context)
{
	util::SwapchainSupportDetails swapchainSupport =
		util::getSwapchainSupport(context->get_physicalDevice(), context->get_surface());

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
	format = surfaceFormat.format;

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

	auto capabilities = swapchainSupport.capabilities;
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		extent = capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetWindowSize(context->get_window(), &width, &height);
		extent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width,
									  capabilities.maxImageExtent.width);
		extent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height,
									   capabilities.maxImageExtent.height);
	}

	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0)
	{
		imageCount = std::min(imageCount, capabilities.maxImageCount);
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = context->get_surface();
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = swapchain.get();

	auto queueIndices = context->get_queueFamilyIndices();
	uint32_t queueFamilyIndices[] = {queueIndices.graphicsIndex.value(), queueIndices.presentIndex.value()};
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

	VkSwapchainKHR schain;
	auto result = vkCreateSwapchainKHR(context->get_device(), &createInfo, nullptr, &schain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Swapchain creation failed with " + std::to_string(result));
	}
	swapchain = vkw::SwapchainKHR(schain, context->get_device());
}

std::vector<VkImage> Swapchain::getImages(const Context* context) const
{
	uint32_t swapchainImageCount = 0;
	std::vector<VkImage> swapchainImages;
	vkGetSwapchainImagesKHR(context->get_device(), swapchain.get(), &swapchainImageCount, nullptr);
	swapchainImages.resize(swapchainImageCount);
	vkGetSwapchainImagesKHR(context->get_device(), swapchain.get(), &swapchainImageCount, swapchainImages.data());
	return swapchainImages;
}

vkw::ImageViewVector Swapchain::createImageViews(const Context* context) const
{
	std::vector<VkImageView> swapchainImageViews;
	swapchainImageViews.reserve(images.size());
	for (size_t i = 0; i < images.size(); i++)
	{
		swapchainImageViews.push_back(util::createImageView(context->get_device(), images[i], VK_IMAGE_VIEW_TYPE_2D,
															format, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1));
	}
	return vkw::ImageViewVector(std::move(swapchainImageViews), context->get_device());
}

Swapchain::Swapchain(const Context* context) noexcept
{
	using namespace util;
	
	createSwapchain(context);

	images = getImages(context);
	imageViews = createImageViews(context);

	count = static_cast<uint32_t>(images.size());
}

void Swapchain::recreate(const Context* context) noexcept
{
	using namespace util;
	createSwapchain(context);

	images = getImages(context);
	imageViews = createImageViews(context);

	count = static_cast<uint32_t>(images.size());
}

Swapchain::Swapchain(Swapchain&& other) noexcept
	: swapchain(std::move(other.swapchain)), format(std::move(other.format)), extent(std::move(other.extent)),
	  images(std::move(other.images)), imageViews(std::move(other.imageViews)), count(other.count)
{
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept
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
} // namespace blaze
