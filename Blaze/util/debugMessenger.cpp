
#include "debugMessenger.hpp"

#include <fstream>
#include <cstring>

namespace blaze::util
{

struct DebugCounts
{
	inline static uint32_t errors{0};
	inline static uint32_t warnings{0};
	inline static uint32_t verbose{0};
	inline static std::ofstream fileStream;

private:
	inline static bool _init = false;

public:
	static void init()
	{
		if (!_init)
		{
			_init = true;
			fileStream.open("logfile.txt", std::ofstream::out);
			atexit(exit);
		}
	}

	static void exit()
	{
		std::cout << errors << " error(s), " << warnings << " warning(s), " << verbose << " verbose message(s)."
				  << std::endl;
		fileStream.close();
	}
};

/// @cond PRIVATE
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
											 VkDebugUtilsMessageTypeFlagsEXT messageType,
											 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		if (pCallbackData->pMessageIdName != nullptr)
		{
			std::cerr << "VALIDATION ERR [" << pCallbackData->pMessageIdName << "]: " << pCallbackData->pMessage << '\n';
		}
		else
		{
			std::cerr << "VALIDATION ERR: " << pCallbackData->pMessage << '\n';
		}
	}

	if (pCallbackData->pMessageIdName && strcmp(pCallbackData->pMessageIdName, "UNASSIGNED-CoreValidation-DrawState-InvalidImageLayout"))
	{
		return VK_FALSE;
	}

	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		DebugCounts::fileStream << "ERROR ";
		DebugCounts::errors++;
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		DebugCounts::fileStream << "WARNING ";
		DebugCounts::warnings++;
	}
	else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
	{
		DebugCounts::fileStream << "VERBOSE ";
		DebugCounts::verbose++;
	}

	if (pCallbackData->pMessageIdName != nullptr)
	{
		DebugCounts::fileStream << "[" << pCallbackData->pMessageIdName << "]: " << pCallbackData->pMessage << '\n';
	}
	else
	{
		DebugCounts::fileStream << ": " << pCallbackData->pMessage << '\n';
	}

	return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT createDebugMessengerCreateInfo()
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
								 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
								 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
							 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
							 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr;

	DebugCounts::init();

	return createInfo;
}

VkResult createDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
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

void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
								   const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}
/// @endcond
} // namespace blaze::util
