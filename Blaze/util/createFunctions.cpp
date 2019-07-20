
#include "createFunctions.hpp"

#include <stdexcept>
#include <string>

namespace blaze::util
{
	VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& shaderCode)
	{
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderCode.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		auto result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
		if (result == VK_SUCCESS)
		{
			return shaderModule;
		}
		throw std::runtime_error("Shader Module creation failed with " + std::to_string(result));
	}

	VkSemaphore createSemaphore(VkDevice device)
	{
		VkSemaphore semaphore = VK_NULL_HANDLE;
		VkSemaphoreCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		auto result = vkCreateSemaphore(device, &createInfo, nullptr, &semaphore);
		if (result == VK_SUCCESS)
		{
			return semaphore;
		}
		throw std::runtime_error("Semaphore creation failed with " + std::to_string(result));
	}

	VkFence createFence(VkDevice device)
	{
		VkFence fence = VK_NULL_HANDLE;
		VkFenceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		auto result = vkCreateFence(device, &createInfo, nullptr, &fence);
		if (result == VK_SUCCESS)
		{
			return fence;
		}
		throw std::runtime_error("Fence creation failed with " + std::to_string(result));
	}
}
