
#include "Context.hpp"

#include <iostream>
#include <set>
#include <string>
#include <util/DeviceSelection.hpp>
#include <util/createFunctions.hpp>
#include <vector>

#undef max
#undef min

namespace blaze
{
BufferObject Context::createBuffer(size_t size, VkBufferUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const
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
		throw std::runtime_error("Buffer could not be allocated with " + std::to_string(result));
	}

	return BufferObject{buffer, allocation};
}

ImageObject Context::createImage(uint32_t width, uint32_t height, uint32_t miplevels, VkFormat format,
								 VkImageTiling tiling, VkImageUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const
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
		throw std::runtime_error("Image could not be allocated with " + std::to_string(result));
	}

	return ImageObject{image, allocation, format};
}

ImageObject Context::createImage(uint32_t width, uint32_t height, uint32_t miplevels, uint32_t layerCount,
								 VkFormat format, VkImageTiling tiling, VkImageUsageFlags vulkanUsage,
								 VmaMemoryUsage vmaUsage) const
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = miplevels;
	imageInfo.arrayLayers = layerCount;
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
		throw std::runtime_error("Image could not be allocated with " + std::to_string(result));
	}

	return ImageObject{image, allocation, format};
}

ImageObject Context::createImageCube(uint32_t width, uint32_t height, uint32_t miplevels, VkFormat format,
									 VkImageTiling tiling, VkImageUsageFlags vulkanUsage, VmaMemoryUsage vmaUsage) const
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
		throw std::runtime_error("ImageCube could not be allocated with " + std::to_string(result));
	}

	return ImageObject{image, allocation, format};
}

vkw::Instance Context::createInstance(const std::vector<const char*>& requiredExtensions) const
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
	return vkw::Instance(instance);
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

void Context::setupDebugMessenger()
{
	if (enableValidationLayers)
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo = util::createDebugMessengerCreateInfo();

		VkDebugUtilsMessengerEXT dm;
		auto result = util::createDebugUtilsMessengerEXT(instance.get(), &createInfo, nullptr, &dm);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Debug messenger creation failed with " + std::to_string(result));
		}
		debugMessenger = vkw::DebugUtilsMessengerEXT(dm, instance.get());
	}
}

vkw::SurfaceKHR Context::createSurface(GLFWwindow* window) const
{
	VkSurfaceKHR surface;
	auto result = glfwCreateWindowSurface(instance.get(), window, nullptr, &surface);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Surface creation failed with " + std::to_string(result));
	}
	return vkw::SurfaceKHR(surface, instance.get());
}

vkw::PhysicalDevice Context::getPhysicalDevice() const
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
			return vkw::PhysicalDevice(physicalDevice);
		}
	}

	throw std::runtime_error("Suitable Device Not Found");
}

vkw::Device Context::createLogicalDevice() const
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = {queueFamilyIndices.graphicsIndex.value(),
											  queueFamilyIndices.presentIndex.value()};

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
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Device Creation failed with " + std::to_string(result));
	}
	return vkw::Device(device);
}

vkw::Queue Context::getQueue(uint32_t index) const
{
	VkQueue queue;
	vkGetDeviceQueue(device.get(), index, 0, &queue);
	return vkw::Queue(queue);
}

vkw::CommandPool Context::createCommandPool(uint32_t queueIndex) const
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
	return vkw::CommandPool(commandPool, device.get());
}

vkw::MemAllocator Context::createAllocator() const
{
	VmaAllocatorCreateInfo createInfo = {};
	createInfo.physicalDevice = physicalDevice.get();
	createInfo.device = device.get();

	VmaAllocator alloc;
	vmaCreateAllocator(&createInfo, &alloc);

	return vkw::MemAllocator(alloc);
};

VkCommandBuffer Context::startCommandBufferRecord() const
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

void Context::flushCommandBuffer(VkCommandBuffer commandBuffer) const
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

	result = vkWaitForFences(device.get(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Wait for fences failed with " + std::to_string(result));
	}
	vkDestroyFence(device.get(), fence, nullptr);
	vkFreeCommandBuffers(device.get(), get_transferCommandPool(), 1, &commandBuffer);
}

Context::Context(GLFWwindow* window, bool enableValidationLayers) noexcept
	: window(window), enableValidationLayers(enableValidationLayers)
{
	auto requiredExtensions = getRequiredInstanceExtensions();
	try
	{
		if (!util::checkValidationLayerSupport(validationLayers))
		{
			throw std::runtime_error("Validation layers not supported.");
		}
		instance = createInstance(requiredExtensions);
		setupDebugMessenger();
		surface = createSurface(window);
		physicalDevice = getPhysicalDevice();
		queueFamilyIndices = util::getQueueFamilies(physicalDevice.get(), surface.get());
		device = createLogicalDevice();
		graphicsQueue = getQueue(queueFamilyIndices.graphicsIndex.value());
		presentQueue = getQueue(queueFamilyIndices.presentIndex.value());
		graphicsCommandPool = createCommandPool(queueFamilyIndices.graphicsIndex.value());

		{
			VkPhysicalDeviceProperties props;
			vkGetPhysicalDeviceProperties(physicalDevice.get(), &props);
			std::cout << "Using " << props.deviceName << std::endl;
		}

		allocator = createAllocator();

		isComplete = true;
	}
	catch (std::exception& e)
	{
		std::cerr << "CONTEXT_CREATION_FAILED: " << e.what() << std::endl;
		isComplete = false;
	}
}

Context::Context(Context&& other) noexcept
	: window(other.window), enableValidationLayers(other.enableValidationLayers), isComplete(other.isComplete),
	  instance(std::move(other.instance)), debugMessenger(std::move(other.debugMessenger)),
	  surface(std::move(other.surface)), physicalDevice(std::move(other.physicalDevice)),
	  queueFamilyIndices(std::move(other.queueFamilyIndices)), device(std::move(other.device)),
	  graphicsQueue(std::move(other.graphicsQueue)), presentQueue(std::move(other.presentQueue)),
	  graphicsCommandPool(std::move(other.graphicsCommandPool)), allocator(std::move(other.allocator))
{
}

Context& Context::operator=(Context&& other) noexcept
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
} // namespace blaze
