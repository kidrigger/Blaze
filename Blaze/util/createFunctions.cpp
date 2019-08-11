
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

	std::tuple<VkBuffer, VkDeviceMemory> createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, size_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	{
		VkBufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createInfo.size = size;
		createInfo.usage = usage;
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		
		VkBuffer buffer;
		auto result = vkCreateBuffer(device, &createInfo, nullptr, &buffer);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Vertex buffer creation failed with " + std::to_string(result));
		}

		VkMemoryRequirements memReq;
		vkGetBufferMemoryRequirements(device, buffer, &memReq);

		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

		uint32_t memoryTypeIndex = -1;
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
		{
			if ((memReq.memoryTypeBits & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties))
			{
				memoryTypeIndex = i;
				break;
			}
		}

		if (memoryTypeIndex < 0)
		{
			throw std::runtime_error("Memory required by vertex buffer not found.");
		}

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReq.size;
		allocInfo.memoryTypeIndex = memoryTypeIndex;

		VkDeviceMemory memory;
		result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Memory allocation failed with " + std::to_string(result));
		}

		vkBindBufferMemory(device, buffer, memory, 0);

		return { buffer, memory };
	}

	VkImageView createImageView(VkDevice device, VkImage image, VkFormat format)
	{
		VkImageView view;
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		auto result = vkCreateImageView(device, &createInfo, nullptr, &view);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image view with " + std::to_string(result));
		}
		return view;
	}
}
