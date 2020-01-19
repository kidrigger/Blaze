
#pragma once

#include <optional>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze::util
{
	// Structs

    /**
     * @struct QueueFamilyIndices
     *
     * @brief Holds the indices for the queue families to use in the context.
     */
	struct QueueFamilyIndices
	{
        /// Index of the Graphics queue.
		std::optional<uint32_t> graphicsIndex;

        /// Index of the Present queue.
		std::optional<uint32_t> presentIndex;

        /**
         * @fn complete()
         *
         * @brief Checks if the indices are complete.
         *
         * @returns true if both indices are found.
         * @returns false otherwise.
         */
		bool complete()
		{
			return graphicsIndex.has_value() && presentIndex.has_value();
		}
	};

    /**
     * @struct SwapchainSupportDetails
     *
     * @brief Holds supported swapchain features.
     */
	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

    /**
     * @brief Returns the queue family indices of the physical device.
     *
     * @param device The physical device being queried.
     * @param surface The surface used.
     */
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    /**
     * @brief Returns the details of the features supported by the swapchain.
     *
     * @param device The physical device being queried.
     * @param surface The surface used.
     */
	SwapchainSupportDetails getSwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    /**
     * @brief Check if the device supports the list of extensions.
     *
     * @param device The physical device to query.
     * @param deviceExtensions The list of extensions required from the device.
     */
	bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions);

    /**
     * @brief Checks if a device is suitable according to multiple conditions.
     *
     * \arg The device must be a \b Discrete GPU.
     * \arg The device must contain at least one Graphics Queue and one Present Queue.
     * \arg The device must support all the deviceExtensions required.
     * \arg The device must support at least one format and one presentModes
     * \arg The device must support shaderSampledImageArrayDynamicIndexing and samplerAnisotrpy.
     * 
     * @param device The physical device to query.
     * @param surface The surface used.
     * @param deviceExtensions The extensions required to be supported.
     */
	bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions);

    /**
     * @brief Check if all the validation layers are support.
     *
     * @param validationLayers The layers to check support for.
     */
	bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);

    /**
     * @brief Finds a format from the candidates that is supported by the device.
     *
     * @param device The physical device to query.
     * @param candidates The formats to choose from.
     * @param tiling The image tiling to be supported.
     * @param features The features needed to be supported by the format.
     */
	VkFormat findSupportedFormat(VkPhysicalDevice device, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
}
