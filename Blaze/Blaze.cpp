// Blaze.cpp : Defines the entry point for the application.
//

#include "Blaze.hpp"

#include <optional>
#include <vector>
#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#define VALIDATION_LAYERS_ENABLED

#ifdef NDEBUG
#ifdef VALIDATION_LAYERS_ENABLED
#undef VALIDATION_LAYERS_ENABLED
#endif
#endif

// Constants
const int WIDTH = 800;
const int HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef VALIDATION_LAYERS_ENABLED
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

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

// Callbacks

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

VkResult createDebugUtilsMessengerEXT(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator,
	VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void destroyDebugUtilsMessengerEXT(
	VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator) 
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) 
	{
		func(instance, debugMessenger, pAllocator);
	}
}

VkDebugUtilsMessengerCreateInfoEXT createDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr;
	return createInfo;
};

// Vulkan Creation Functions

VkInstance createInstance(const std::vector<const char*>& requiredExtensions) 
{

	VkInstance instance = nullptr;
	auto debugMessengerCreateInfo = createDebugMessengerCreateInfo();

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

std::vector<const char*> getRequiredInstanceExtensions()
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

bool checkValidationLayerSupport()
{
	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}
	return true;
}

QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsIndex = i;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (queueFamily.queueCount > 0 && presentSupport)
		{
			indices.presentIndex = i;
		}

		i++;
	}

	return indices;
}

SwapchainSupportDetails getSwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
	details.formats.resize(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());

	uint32_t presentModesCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, nullptr);
	details.presentModes.resize(presentModesCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModesCount, details.presentModes.data());

	return details;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties(device, &properties);
	if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		return false;
	}

	QueueFamilyIndices queueFamilyIndices = getQueueFamilies(device, surface);

	bool extensionsSupported = checkDeviceExtensionSupport(device);
	bool swapchainAdequate = false;
	if (extensionsSupported)
	{
		auto swapchainSupport = getSwapchainSupport(device, surface);
		swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
	}
	
	return queueFamilyIndices.complete() && extensionsSupported && swapchainAdequate;
}

VkPhysicalDevice getPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	
	if (deviceCount <= 0)
	{
		return VK_NULL_HANDLE;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	for (const auto& device : devices)
	{
		if (isDeviceSuitable(device, surface))
		{
			return device;
		}
	}

	return VK_NULL_HANDLE;
}

VkDevice getLogicalDevice(VkPhysicalDevice physicalDevice, const QueueFamilyIndices& queueIndices)
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { queueIndices.graphicsIndex.value(), queueIndices.presentIndex.value() };

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
	if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) == VK_SUCCESS)
	{
		return device;
	}
	else
	{
		return VK_NULL_HANDLE;
	}
}

std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
{
	SwapchainSupportDetails swapchainSupport = getSwapchainSupport(physicalDevice, surface);

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
	createInfo.surface = surface;
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

	auto queueIndices = getQueueFamilies(physicalDevice, surface);
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
	assert(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) == VK_SUCCESS);
	return std::make_tuple(swapchain, surfaceFormat.format, swapExtent);
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& shaderCode)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = shaderCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
	VkShaderModule shaderModule = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) == VK_SUCCESS)
	{
		return shaderModule;
	}
	return VK_NULL_HANDLE;
}

std::vector<char> loadBinaryFile(const std::string& filename)
{
	using namespace std;
	ifstream file(filename, ios::ate | ios::binary);
	if (file.is_open())
	{
		size_t filesize = file.tellg();
		vector<char> filedata(filesize);
		file.seekg(0);
		file.read(filedata.data(), filesize);
		file.close();
		return filedata;
	}
	cout << "File (" << filename << ") could not be opened." << endl;
	return vector<char>();
}

VkRenderPass createRenderpass(VkDevice device, VkFormat swapchainFormat)
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = 1;
	createInfo.pAttachments = &colorAttachment;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpassDesc;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;

	VkRenderPass renderpass = VK_NULL_HANDLE;
	if (vkCreateRenderPass(device, &createInfo, nullptr, &renderpass) == VK_SUCCESS)
	{
		return renderpass;
	}
	return VK_NULL_HANDLE;
};

std::tuple<VkPipelineLayout, VkPipeline> createGraphicsPipeline(VkDevice device, VkRenderPass renderpass, VkExtent2D swapchainExtent)
{
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;

	auto vertexShaderCode = loadBinaryFile("shaders/vShader.vert.spv");
	auto fragmentShaderCode = loadBinaryFile("shaders/fShader.frag.spv");

	VkShaderModule vertexShaderModule = createShaderModule(device, vertexShaderCode);
	assert(vertexShaderModule != VK_NULL_HANDLE);
	VkShaderModule fragmentShaderModule = createShaderModule(device, fragmentShaderCode);
	assert(fragmentShaderModule != VK_NULL_HANDLE);

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStagesCreateInfo[] = {
		vertexShaderStageCreateInfo,
		fragmentShaderStageCreateInfo
	};

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.lineWidth = 1.0f;
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblendAttachment = {};
	colorblendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorblendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorblendCreateInfo = {};
	colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorblendCreateInfo.logicOpEnable = VK_FALSE;
	colorblendCreateInfo.attachmentCount = 1;
	colorblendCreateInfo.pAttachments = &colorblendAttachment;

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = 2;
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	if (vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		pipelineLayout = VK_NULL_HANDLE;

		vkDestroyShaderModule(device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(device, fragmentShaderModule, nullptr);

		return std::make_tuple(pipelineLayout, graphicsPipeline);
	}

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStagesCreateInfo;
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
	pipelineCreateInfo.pDepthStencilState = nullptr;
	pipelineCreateInfo.pColorBlendState = &colorblendCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderpass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
	{
		graphicsPipeline = VK_NULL_HANDLE;
	}

	vkDestroyShaderModule(device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(device, fragmentShaderModule, nullptr);

	return std::make_tuple(pipelineLayout, graphicsPipeline);
};

int main()
{
	// Usings
	using namespace std;
	using std::byte;

	// Variables
	GLFWwindow* window = nullptr;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkFormat swapchainFormat;
	VkExtent2D swapchainExtent;
	vector<VkImage> swapchainImages;
	vector<VkImageView> swapchainImageViews;
	VkRenderPass renderpass = VK_NULL_HANDLE;
	VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	vector<VkFramebuffer> swapchainFramebuffers;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	vector<VkCommandBuffer> commandBuffers;
	vector<VkSemaphore> imageAvailableSem(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	vector<VkSemaphore> renderFinishedSem(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
	vector<VkFence> inFlightFences(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);

	// GLFW Setup
	assert(glfwInit());
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(WIDTH, HEIGHT, "Hello, Vulkan", nullptr, nullptr);
	assert(window != nullptr);

	assert(checkValidationLayerSupport());

	// Instance
	auto requiredExtensions = getRequiredInstanceExtensions();
	instance = createInstance(requiredExtensions);

	if (enableValidationLayers) 
	{
		auto createInfo = createDebugMessengerCreateInfo();
		if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
		{
			throw runtime_error("Failed to setup Debug messenger");
		}
	}

	// Surface
	assert(glfwCreateWindowSurface(instance, window, nullptr, &surface) == VK_SUCCESS);

	// PhysicalDevice
	physicalDevice = getPhysicalDevice(instance, surface);
	assert(physicalDevice != VK_NULL_HANDLE);

	QueueFamilyIndices queueIndices = getQueueFamilies(physicalDevice, surface);

	// LogicalDevice
	device = getLogicalDevice(physicalDevice, queueIndices);
	assert(device != VK_NULL_HANDLE);

	// DeviceQueues
	vkGetDeviceQueue(device, queueIndices.graphicsIndex.value(), 0, &graphicsQueue);	// Will always succeed.
	vkGetDeviceQueue(device, queueIndices.presentIndex.value(), 0, &presentQueue);

	// Swapchain
	tie(swapchain, swapchainFormat, swapchainExtent) = createSwapchain(device, physicalDevice, surface);

	// SwapchainImages
	{
		uint32_t swapchainImageCount = 0;
		vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
		swapchainImages.resize(swapchainImageCount);
		vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
	}

	swapchainImageViews.resize(swapchainImages.size());
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapchainFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		assert(vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]) == VK_SUCCESS);
	}

	renderpass = createRenderpass(device, swapchainFormat);
	assert(renderpass != VK_NULL_HANDLE);

	tie(graphicsPipelineLayout, graphicsPipeline) = createGraphicsPipeline(device, renderpass, swapchainExtent);

	// Framebuffers
	swapchainFramebuffers.resize(swapchainImageViews.size());
	for (size_t i = 0; i < swapchainImageViews.size(); i++)
	{
		vector<VkImageView> attachments = {
			swapchainImageViews[i]
		};

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderpass;
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.width = swapchainExtent.width;
		createInfo.height = swapchainExtent.height;
		createInfo.layers = 1;

		assert(vkCreateFramebuffer(device, &createInfo, nullptr, &swapchainFramebuffers[i]) == VK_SUCCESS);
	}

	// Command Pool
	{
		VkCommandPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.queueFamilyIndex = queueIndices.graphicsIndex.value();
		assert(vkCreateCommandPool(device, &createInfo, nullptr, &commandPool) == VK_SUCCESS);
	}

	{
		commandBuffers.resize(swapchainFramebuffers.size());
		
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
		assert(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) == VK_SUCCESS);
	}

	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		assert(vkBeginCommandBuffer(commandBuffers[i], &commandBufferBeginInfo) == VK_SUCCESS);

		VkRenderPassBeginInfo renderpassBeginInfo = {};
		renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpassBeginInfo.renderPass = renderpass;
		renderpassBeginInfo.framebuffer = swapchainFramebuffers[i];
		renderpassBeginInfo.renderArea.offset = {0, 0};
		renderpassBeginInfo.renderArea.extent = swapchainExtent;
		
		VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
		renderpassBeginInfo.clearValueCount = 1;
		renderpassBeginInfo.pClearValues = &clearColor;
		vkCmdBeginRenderPass(commandBuffers[i], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);

		assert(vkEndCommandBuffer(commandBuffers[i]) == VK_SUCCESS);
	}

	auto createSemaphore = [](VkDevice device) -> VkSemaphore
	{
		VkSemaphore semaphore = VK_NULL_HANDLE;
		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(device, &createInfo, nullptr, &semaphore) == VK_SUCCESS)
		{
			return semaphore;
		}
		return VK_NULL_HANDLE;
	};

	auto createFence = [](VkDevice device) -> VkFence
	{
		VkFence fence = VK_NULL_HANDLE;
		VkFenceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		if (vkCreateFence(device, &createInfo, nullptr, &fence) == VK_SUCCESS)
		{
			return fence;
		}
		return VK_NULL_HANDLE;
	};

	for (auto& sem : imageAvailableSem)
	{
		sem = createSemaphore(device);
		assert(sem != VK_NULL_HANDLE);
	}

	for (auto& sem : renderFinishedSem)
	{
		sem = createSemaphore(device);
		assert(sem != VK_NULL_HANDLE);
	}

	for (auto& fence : inFlightFences)
	{
		fence = createFence(device);
		assert(fence != VK_NULL_HANDLE);
	}

	// Run
	size_t currentFrame = 0;
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, numeric_limits<uint64_t>::max());
		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, swapchain, numeric_limits<uint64_t>::max(), imageAvailableSem[currentFrame], VK_NULL_HANDLE, &imageIndex);
		
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSem[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1u;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { renderFinishedSem[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		assert(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) == VK_SUCCESS);

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapchains[] = { swapchain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;

		assert(vkQueuePresentKHR(presentQueue, &presentInfo) == VK_SUCCESS);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	// Wait for all commands to finish
	vkDeviceWaitIdle(device);
	
	// Cleanup

	for (auto& sem : imageAvailableSem)
	{
		vkDestroySemaphore(device, sem, nullptr);
	}
	for (auto& sem : renderFinishedSem)
	{
		vkDestroySemaphore(device, sem, nullptr);
	}
	for (auto& fence : inFlightFences)
	{
		vkDestroyFence(device, fence, nullptr);
	}
	vkDestroyCommandPool(device, commandPool, nullptr);

	for (auto framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	
	for (auto imageview : swapchainImageViews)
	{
		vkDestroyImageView(device, imageview, nullptr);
	}

	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderpass, nullptr);

	vkDestroySwapchainKHR(device, swapchain, nullptr);
	vkDestroyDevice(device, nullptr);
	destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
