
#include "Context.hpp"
#include <vector>
#include <iostream>
#include <string>
#include <set>
#include "util/DeviceSelection.hpp"
#include "util/createFunctions.hpp"

namespace blaze
{
	VkInstance Context::createInstance(const std::vector<const char*>& requiredExtensions) const
	{
		VkInstance instance = VK_NULL_HANDLE;
		auto debugMessengerCreateInfo = util::createDebugMessengerCreateInfo();

		// Vulkan Setup
		VkApplicationInfo applicationInfo = {};
		applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		applicationInfo.pApplicationName = "Hello Vulkan";
		applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		applicationInfo.pEngineName = "No Engine";
		applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		applicationInfo.apiVersion = VK_API_VERSION_1_1;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &applicationInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
		createInfo.ppEnabledExtensionNames = requiredExtensions.data();
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
			createInfo.pNext = &debugMessengerCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		auto result = vkCreateInstance(&createInfo, nullptr, &instance);

		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create VK instance.");
		}
		else
		{
			std::cout << "VK Instance Created." << std::endl;
		}
		return instance;
	}

	std::vector<const char*> Context::getRequiredInstanceExtensions() const
	{
		using namespace std;
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = nullptr;

		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers)
		{
			requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}
		return requiredExtensions;
	}

	void Context::setupDebugMessenger() {
		if (enableValidationLayers)
		{
			VkDebugUtilsMessengerCreateInfoEXT createInfo = util::createDebugMessengerCreateInfo();

			VkDebugUtilsMessengerEXT dm;
			auto result = util::createDebugUtilsMessengerEXT(instance.get(), &createInfo, nullptr, &dm);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Debug messenger creation failed with " + std::to_string(result));
			}
			debugMessenger = util::Managed(dm, [inst = instance.get()](VkDebugUtilsMessengerEXT& dm) { util::destroyDebugUtilsMessengerEXT(inst, dm, nullptr); });
		}
	}

	VkSurfaceKHR Context::createSurface(GLFWwindow* window) const
	{
		VkSurfaceKHR surface;
		auto result = glfwCreateWindowSurface(instance.get(), window, nullptr, &surface);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Surface creation failed with " + std::to_string(result));
		}
		return surface;
	}

	VkPhysicalDevice Context::getPhysicalDevice() const
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance.get(), &deviceCount, nullptr);

		if (deviceCount <= 0)
		{
			throw std::runtime_error("Device Count < 0.");
		}

		std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
		vkEnumeratePhysicalDevices(instance.get(), &deviceCount, physicalDevices.data());

		for (const auto& physicalDevice : physicalDevices)
		{
			if (util::isDeviceSuitable(physicalDevice, surface.get(), deviceExtensions))
			{
				return physicalDevice;
			}
		}

		throw std::runtime_error("Suitable Device Not Found");
	}

	VkDevice Context::createLogicalDevice() const
	{
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { queueFamilyIndices.graphicsIndex.value(), queueFamilyIndices.presentIndex.value() };

		float queuePriority = 1.0f;

		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			createInfo.queueFamilyIndex = queueFamily;
			createInfo.queueCount = 1;
			createInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(createInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkDevice device;
		auto result = vkCreateDevice(physicalDevice.get(), &createInfo, nullptr, &device);
		if (result == VK_SUCCESS)
		{
			return device;
		}
		throw std::runtime_error("Device Creation failed with " + std::to_string(result));
	}

	VkQueue Context::getQueue(uint32_t index) const
	{
		VkQueue queue;
		vkGetDeviceQueue(device.get(), index, 0, &queue);
		return queue;
	}

	VkCommandPool Context::createCommandPool(uint32_t queueIndex) const 
	{
		VkCommandPool commandPool;
		VkCommandPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.queueFamilyIndex = queueIndex;
		createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		auto result = vkCreateCommandPool(device.get(), &createInfo, nullptr, &commandPool);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("CommandPool creation failed with " + std::to_string(result));
		}
		return commandPool;
	}

	VmaAllocator Context::createAllocator() const
	{
		VmaAllocatorCreateInfo createInfo = {};
		createInfo.physicalDevice = physicalDevice.get();
		createInfo.device = device.get();

		VmaAllocator alloc;
		vmaCreateAllocator(&createInfo, &alloc);

		return alloc;
	};

	VkCommandBuffer Context::startTransferCommands() const
	{
		VkCommandBuffer commandBuffer;
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = graphicsCommandPool.get();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		auto result = vkAllocateCommandBuffers(device.get(), &allocInfo, &commandBuffer);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Command buffer alloc failed with " + std::to_string(result));
		}

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Begin Command Buffer failed with " + std::to_string(result));
		}

		return commandBuffer;
	}

	void Context::endTransferCommands(VkCommandBuffer commandBuffer) const
	{
		auto result = vkEndCommandBuffer(commandBuffer);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("End Command Buffer failed with " + std::to_string(result));
		}

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VkFence fence = util::createFence(device.get());
		vkResetFences(device.get(), 1, &fence);

		result = vkQueueSubmit(get_transferQueue(), 1, &submitInfo, fence);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Submit Command Buffer failed with " + std::to_string(result));
		}

		vkWaitForFences(device.get(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkDestroyFence(device.get(), fence, nullptr);
		vkFreeCommandBuffers(device.get(), get_transferCommandPool(), 1, &commandBuffer);
	}
}
