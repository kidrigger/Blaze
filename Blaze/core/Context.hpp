
#pragma once

#include <Datatypes.hpp>
#include <util/DeviceSelection.hpp>
#include <util/debugMessenger.hpp>
#include <vkwrap/VkWrap.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <thirdparty/vma/vk_mem_alloc.h>

#include <exception>

namespace blaze
{
/**
 * @class Context
 *
 * @brief Vulkan context to handle device initialization logic.
 *
 * Handles the required hardware interactions.
 * Sets up the devices, surface, extensions, layers, commandpools, and quieues.
 */
class Context
{
private:
	bool enableValidationLayers{true};
	bool isComplete{false};

	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation",
	};

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MULTIVIEW_EXTENSION_NAME,
	};

	vkw::Instance instance;
	vkw::DebugUtilsMessengerEXT debugMessenger;
	vkw::SurfaceKHR surface;
	vkw::PhysicalDevice physicalDevice;
	vkw::Device device;

	util::QueueFamilyIndices queueFamilyIndices;
	vkw::Queue graphicsQueue;
	vkw::Queue presentQueue;
	vkw::CommandPool graphicsCommandPool;

	vkw::MemAllocator allocator;

	GLFWwindow* window{nullptr};

public:
	/**
	 * @fn Context()
	 *
	 * @brief Default constructor.
	 */
	Context() noexcept : enableValidationLayers(true)
	{
	}

	/**
	 * @fn Context(GLFWwindow* window, bool enableValidationLayers = true)
	 *
	 * @brief Initializes all the member variables appropriately.
	 *
	 * @param window The GLFW Window handle of the current window.
	 * @param enableValidationLayers Do we enable the validation layers?
	 */
	Context(GLFWwindow* window, bool enableValidationLayers = true) noexcept;

	/**
	 * @name Move Constructors
	 *
	 * @brief Moves all the members to the new construction.
	 *
	 * Context is a move only class and must be kept unique.
	 * The copy constructors are deleted.
	 *
	 * @{
	 */
	Context(Context&& other) noexcept;
	Context& operator=(Context&& other) noexcept;

	Context(const Context& other) = delete;
	Context& operator=(const Context& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @fn complete
	 *
	 * @brief Checks if the Context is complete.
	 *
	 * A context is considered complete if and only if all its components
	 * are constructed successfully during the constructor.
	 * In case of any errors, a context would remain incomplete.
	 *
	 * @returns Whether Context constructor ran sucessfully.
	 */
	bool complete() const
	{
		return isComplete;
	}

	/**
	 * @fn createBuffer
	 *
	 * @brief Creates a buffer according to the configured flags.
	 *
	 * @param size The size of the buffer to be allocated.
	 * @param vulkanUsage The VkBufferUsageFlags describing the usage.
	 * @param vmaUsage The VmaMemoryUsage describing where the buffer would be used (GPU, CPU, etc)
	 *
	 * @returns BufferObject of buffer handle and the allocation.
	 */
	BufferObject createBuffer(size_t size, VkBufferUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const;

	/**
	 * @brief Creates a 2D image object accourding to the configured flags.
	 *
	 * @param width The width of the image.
	 * @param height The height of the image.
	 * @param miplevels The number of MIP levels in the image.
	 * @param layerCount The number of layers in the image.
	 * @param format The format of the image storage.
	 * @param tiling The tiling used in the image storage.
	 * @param vulkanUsage The VkBufferUsageFlags describing the usage.
	 * @param vmaUsage The VmaMemoryUsage describing where the buffer would be used (GPU, CPU, etc)
	 *
	 * @returns ImageObject of the image.
	 */
	ImageObject createImage(uint32_t width, uint32_t height, uint32_t miplevels, uint32_t layerCount, VkFormat format,
							VkImageTiling tiling, VkImageUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const;

	/**
	 * @brief Creates a 2D image object accourding to the configured flags.
	 *
	 * @param width The width of the image.
	 * @param height The height of the image.
	 * @param miplevels The number of MIP levels in the image.
	 * @param format The format of the image storage.
	 * @param tiling The tiling used in the image storage.
	 * @param vulkanUsage The VkBufferUsageFlags describing the usage.
	 * @param vmaUsage The VmaMemoryUsage describing where the buffer would be used (GPU, CPU, etc)
	 *
	 * @deprecated Since layers were added.
	 *
	 * @returns ImageObject of the image.
	 */
	[[deprecated]] ImageObject createImage(uint32_t width, uint32_t height, uint32_t miplevels, VkFormat format,
										   VkImageTiling tiling, VkImageUsageFlags vulkanUsage,
										   VmaMemoryUsage vmaUsage) const;

	/**
	 * @fn createImageCube
	 *
	 * @brief Creates a cubemap image object accourding to the configured flags.
	 *
	 * @param width The width of the image.
	 * @param height The height of the image.
	 * @param miplevels The number of MIP levels in the image.
	 * @param format The format of the image storage.
	 * @param tiling The tiling used in the image storage.
	 * @param vulkanUsage The VkBufferUsageFlags describing the usage.
	 * @param vmaUsage The VmaMemoryUsage describing where the buffer would be used (GPU, CPU, etc)
	 *
	 * @returns ImageObject Contains image, allocation and format information.
	 */
	ImageObject createImageCube(uint32_t width, uint32_t height, uint32_t miplevels, VkFormat format,
								VkImageTiling tiling, VkImageUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const;

	/**
	 * @fn startCommandBufferRecord
	 *
	 * @brief Creates a one time use command buffer.
	 *
	 * Creates a single use primary command buffer from the context
	 * and start the buffer recording.
	 *
	 * @returns The commandbuffer that is ready to record commands.
	 */
	VkCommandBuffer startCommandBufferRecord() const;

	/**
	 * @fn flushCommandBuffer
	 *
	 * @brief Ends and submits the command buffer.
	 *
	 * @param commandBuffer One time use command buffer.
	 */
	void flushCommandBuffer(VkCommandBuffer commandBuffer) const;

	/**
	 * @name getters
	 *
	 * @brief Getters for private fields.
	 *
	 * Each getter returns the private field.
	 * The fields are borrowed from Context and must not be deleted.
	 *
	 * @{
	 */
	inline VkInstance get_instance() const
	{
		return instance.get();
	}
	inline VkSurfaceKHR get_surface() const
	{
		return surface.get();
	}
	inline VkPhysicalDevice get_physicalDevice() const
	{
		return physicalDevice.get();
	}
	inline VkDevice get_device() const
	{
		return device.get();
	}
	inline VkQueue get_graphicsQueue() const
	{
		return graphicsQueue.get();
	}
	inline VkQueue get_presentQueue() const
	{
		return presentQueue.get();
	}
	inline VkQueue get_transferQueue() const
	{
		return graphicsQueue.get();
	} // TODO: Explicit
	inline VkCommandPool get_graphicsCommandPool() const
	{
		return graphicsCommandPool.get();
	}
	inline VkCommandPool get_transferCommandPool() const
	{
		return graphicsCommandPool.get();
	}
	inline const util::QueueFamilyIndices& get_queueFamilyIndices() const
	{
		return queueFamilyIndices;
	}
	inline VmaAllocator get_allocator() const
	{
		return allocator.get();
	}

	inline GLFWwindow* get_window() const
	{
		return window;
	}
	/**
	 * @}
	 */

private:
	vkw::Instance createInstance(const std::vector<const char*>& requiredExtensions) const;
	std::vector<const char*> getRequiredInstanceExtensions() const;
	void setupDebugMessenger();
	vkw::SurfaceKHR createSurface(GLFWwindow* window) const;
	vkw::PhysicalDevice getPhysicalDevice() const;
	vkw::Device createLogicalDevice() const;
	vkw::Queue getQueue(uint32_t index) const;
	vkw::CommandPool createCommandPool(uint32_t queueIndex) const;
	vkw::MemAllocator createAllocator() const;
};
} // namespace blaze
