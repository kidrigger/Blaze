
#pragma once

#include "util/Managed.hpp"
#include "util/debug.hpp"
#include "util/DeviceSelection.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <exception>

namespace blaze
{
	class Context
	{
	private:
		bool enableValidationLayers{ true };
		int WIDTH{ 800 };
		int HEIGHT{ 600 };
		bool isComplete{ false };

		const std::vector<const char*> validationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		const std::vector<const char*> deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		util::Managed<VkInstance> instance;
		util::Managed<VkDebugUtilsMessengerEXT> debugMessenger;
		util::Managed<VkSurfaceKHR> surface;
		util::Unmanaged<VkPhysicalDevice> physicalDevice;
		util::Managed<VkDevice> device;

		util::QueueFamilyIndices queueFamilyIndices;
		util::Unmanaged<VkQueue> graphicsQueue;
		util::Unmanaged<VkQueue> presentQueue;
		util::Managed<VkCommandPool> commandPool;
	public:
		Context() noexcept
			: enableValidationLayers(true) {}

		Context(GLFWwindow* window, bool enableValidationLayers = true) noexcept
			: enableValidationLayers(enableValidationLayers)
		{
			auto requiredExtensions = getRequiredInstanceExtensions();
			glfwGetWindowSize(window, &WIDTH, &HEIGHT);
			try
			{
				if (!util::checkValidationLayerSupport(validationLayers))
				{
					throw std::runtime_error("Validation layers not supported.");
				}
				instance = util::Managed(createInstance(requiredExtensions), [](VkInstance& inst) { vkDestroyInstance(inst, nullptr); });
				setupDebugMessenger();
				surface = util::Managed(createSurface(window), [inst = instance.get()](VkSurfaceKHR& surface) { vkDestroySurfaceKHR(inst, surface, nullptr); });
				physicalDevice = getPhysicalDevice();
				queueFamilyIndices = util::getQueueFamilies(physicalDevice.get(), surface.get());
				device = util::Managed(createLogicalDevice(), [](VkDevice& device) { vkDestroyDevice(device, nullptr); });
				graphicsQueue = getQueue(queueFamilyIndices.graphicsIndex.value());
				presentQueue = getQueue(queueFamilyIndices.presentIndex.value());
				commandPool = util::Managed(createCommandPool(), [dev = device.get()](VkCommandPool& commandPool){ vkDestroyCommandPool(dev, commandPool, nullptr); });

				// Command Pool

				isComplete = true;
			}
			catch (std::exception& e)
			{
				std::cerr << "CONTEXT_CREATION_FAILED: " << e.what() << std::endl;
				isComplete = false;
			}
		}

		Context(Context&& other) noexcept
			: enableValidationLayers(other.enableValidationLayers),
			WIDTH(other.WIDTH),
			HEIGHT(other.HEIGHT),
			isComplete(other.isComplete),
			instance(std::move(other.instance)),
			debugMessenger(std::move(other.debugMessenger)),
			surface(std::move(other.surface)),
			physicalDevice(std::move(other.physicalDevice)),
			queueFamilyIndices(std::move(other.queueFamilyIndices)),
			device(std::move(other.device)),
			graphicsQueue(std::move(other.graphicsQueue)),
			presentQueue(std::move(other.presentQueue)),
			commandPool(std::move(other.commandPool))
		{
		}

		Context& operator=(Context&& other) noexcept
		{
			if (this == &other) return *this;
			enableValidationLayers = other.enableValidationLayers;
			WIDTH = other.WIDTH;
			HEIGHT = other.HEIGHT;
			isComplete = other.isComplete;
			instance = std::move(other.instance);
			debugMessenger = std::move(other.debugMessenger);
			surface = std::move(other.surface);
			physicalDevice = std::move(other.physicalDevice);
			queueFamilyIndices = std::move(other.queueFamilyIndices);
			device = std::move(other.device);
			graphicsQueue = std::move(other.graphicsQueue);
			presentQueue = std::move(other.presentQueue);
			commandPool = std::move(other.commandPool);
			return *this;
		}

		Context(const Context& other) = delete;
		Context& operator=(const Context& other) = delete;

		VkInstance get_instance() { return instance.get(); }
		VkSurfaceKHR get_surface() { return surface.get(); }
		VkPhysicalDevice get_physicalDevice() { return physicalDevice.get(); }
		VkDevice get_device() { return device.get(); }
		VkQueue get_graphicsQueue() { return graphicsQueue.get(); }
		VkQueue get_presentQueue() { return presentQueue.get(); }
		VkCommandPool get_commandPool() { return commandPool.get(); }

		bool complete() const { return isComplete; }

	private:

		VkInstance createInstance(const std::vector<const char*>& requiredExtensions) const;
		std::vector<const char*> getRequiredInstanceExtensions() const;
		void setupDebugMessenger();
		VkSurfaceKHR createSurface(GLFWwindow* window) const;
		VkPhysicalDevice getPhysicalDevice() const;
		VkDevice createLogicalDevice() const;
		VkQueue getQueue(uint32_t index) const;
		VkCommandPool createCommandPool() const;
	};
}
