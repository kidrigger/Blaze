
#pragma once

#include "util/Managed.hpp"
#include "util/debugMessenger.hpp"
#include "util/DeviceSelection.hpp"
#include "Datatypes.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk_mem_alloc.h>

#include <exception>

namespace blaze
{
	class Context
	{
	private:
		bool enableValidationLayers{ true };
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
		util::Managed<VkCommandPool> graphicsCommandPool;

		util::Managed<VmaAllocator> allocator;

		GLFWwindow* window{ nullptr };

	public:
		Context() noexcept
			: enableValidationLayers(true) {}

		Context(GLFWwindow* window, bool enableValidationLayers = true) noexcept
			: window(window),
			enableValidationLayers(enableValidationLayers)
		{
			auto requiredExtensions = getRequiredInstanceExtensions();
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
				graphicsCommandPool = util::Managed(createCommandPool(queueFamilyIndices.graphicsIndex.value()), [dev = device.get()](VkCommandPool& commandPool){ vkDestroyCommandPool(dev, commandPool, nullptr); });

				{
					VkPhysicalDeviceProperties props;
					vkGetPhysicalDeviceProperties(physicalDevice.get(), &props);
					std::cout << "Using " << props.deviceName << std::endl;
				}

				allocator = util::Managed(createAllocator(), [](VmaAllocator& alloc) { vmaDestroyAllocator(alloc); });

				isComplete = true;
			}
			catch (std::exception& e)
			{
				std::cerr << "CONTEXT_CREATION_FAILED: " << e.what() << std::endl;
				isComplete = false;
			}
		}

		Context(Context&& other) noexcept
			: window(other.window),
			enableValidationLayers(other.enableValidationLayers),
			isComplete(other.isComplete),
			instance(std::move(other.instance)),
			debugMessenger(std::move(other.debugMessenger)),
			surface(std::move(other.surface)),
			physicalDevice(std::move(other.physicalDevice)),
			queueFamilyIndices(std::move(other.queueFamilyIndices)),
			device(std::move(other.device)),
			graphicsQueue(std::move(other.graphicsQueue)),
			presentQueue(std::move(other.presentQueue)),
			graphicsCommandPool(std::move(other.graphicsCommandPool)),
			allocator(std::move(other.allocator))
		{
		}

		Context& operator=(Context&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			window = other.window;
			enableValidationLayers = other.enableValidationLayers;
			isComplete = other.isComplete;
			instance = std::move(other.instance);
			debugMessenger = std::move(other.debugMessenger);
			surface = std::move(other.surface);
			physicalDevice = std::move(other.physicalDevice);
			queueFamilyIndices = std::move(other.queueFamilyIndices);
			device = std::move(other.device);
			graphicsQueue = std::move(other.graphicsQueue);
			presentQueue = std::move(other.presentQueue);
			graphicsCommandPool = std::move(other.graphicsCommandPool);
			allocator = std::move(other.allocator);
			return *this;
		}

		Context(const Context& other) = delete;
		Context& operator=(const Context& other) = delete;

		inline VkInstance get_instance() const { return instance.get(); }
		inline VkSurfaceKHR get_surface() const { return surface.get(); }
		inline VkPhysicalDevice get_physicalDevice() const { return physicalDevice.get(); }
		inline VkDevice get_device() const { return device.get(); }
		inline VkQueue get_graphicsQueue() const { return graphicsQueue.get(); }
		inline VkQueue get_presentQueue() const { return presentQueue.get(); }
		inline VkQueue get_transferQueue() const { return graphicsQueue.get(); }	// TODO: Explicit
		inline VkCommandPool get_graphicsCommandPool() const { return graphicsCommandPool.get(); }
		inline VkCommandPool get_transferCommandPool() const { return graphicsCommandPool.get(); }
		inline const util::QueueFamilyIndices& get_queueFamilyIndices() const { return queueFamilyIndices; }
		inline VmaAllocator get_allocator() const { return allocator.get(); }

		inline GLFWwindow* get_window() const { return window; }

		bool complete() const { return isComplete; }
		std::tuple<VkBuffer, VmaAllocation> createBuffer(size_t size, VkBufferUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const
		{
			VkBufferCreateInfo bufferInfo = {};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = vulkanUsage;

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = vmaUsage;

			VkBuffer buffer;
			VmaAllocation allocation;
			auto result = vmaCreateBuffer(allocator.get(), &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Buffer could not be allocated");
			}

			return std::make_tuple(buffer, allocation);
		}

		ImageObject createImage(uint32_t width, uint32_t height, uint32_t miplevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const
		{
			VkImageCreateInfo imageInfo = {};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = miplevels;
			imageInfo.arrayLayers = 1;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = vulkanUsage;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = vmaUsage;

			VkImage image;
			VmaAllocation allocation;
			auto result = vmaCreateImage(allocator.get(), &imageInfo, &allocInfo, &image, &allocation, nullptr);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Buffer could not be allocated");
			}

			return ImageObject{ image, allocation, format };
		}

		ImageObject createImageCube(uint32_t width, uint32_t height, uint32_t miplevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const
		{
			VkImageCreateInfo imageInfo = {};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.extent.width = width;
			imageInfo.extent.height = height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = miplevels;
			imageInfo.arrayLayers = 6;
			imageInfo.format = format;
			imageInfo.tiling = tiling;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageInfo.usage = vulkanUsage;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = vmaUsage;

			VkImage image;
			VmaAllocation allocation;
			auto result = vmaCreateImage(allocator.get(), &imageInfo, &allocInfo, &image, &allocation, nullptr);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Buffer could not be allocated");
			}

			return ImageObject{ image, allocation, format };
		}

		VkCommandBuffer startCommandBufferRecord() const;
		void flushCommandBuffer(VkCommandBuffer commandBuffer) const;

	private:

		VkInstance createInstance(const std::vector<const char*>& requiredExtensions) const;
		std::vector<const char*> getRequiredInstanceExtensions() const;
		void setupDebugMessenger();
		VkSurfaceKHR createSurface(GLFWwindow* window) const;
		VkPhysicalDevice getPhysicalDevice() const;
		VkDevice createLogicalDevice() const;
		VkQueue getQueue(uint32_t index) const;
		VkCommandPool createCommandPool(uint32_t queueIndex) const;
		VmaAllocator createAllocator() const;
	};
}
