
#pragma once

#include <optional>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze::util
{
	// Structs

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsIndex;
		std::optional<uint32_t> presentIndex;

		bool complete()
		{
			return graphicsIndex.has_value() && presentIndex.has_value();
		}
	};

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

	SwapchainSupportDetails getSwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

	bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions);

	bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions);

	bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);
}
