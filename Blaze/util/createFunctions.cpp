
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

	VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspect, uint32_t miplevels)
	{
		VkImageView view;
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image;
		createInfo.viewType = viewType;
		createInfo.format = format;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = aspect;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = miplevels;
		createInfo.subresourceRange.baseArrayLayer = 0;
		if (viewType == VK_IMAGE_VIEW_TYPE_CUBE)
		{
			createInfo.subresourceRange.layerCount = 6;
		}
		else
		{
			createInfo.subresourceRange.layerCount = 1;
		}
		auto result = vkCreateImageView(device, &createInfo, nullptr, &view);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image view with " + std::to_string(result));
		}
		return view;
	}

	VkDescriptorPool createDescriptorPool(VkDevice device, std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets)
	{
		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		createInfo.pPoolSizes = poolSizes.data();
		createInfo.maxSets = maxSets;
		createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		VkDescriptorPool pool;
		auto result = vkCreateDescriptorPool(device, &createInfo, nullptr, &pool);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Descriptor pool creation failed with " + std::to_string(result));
		}
		return pool;
	}

	VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::vector<VkDescriptorSetLayoutBinding>& layoutBindings)
	{
		VkDescriptorSetLayout descriptorSetLayout;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
		layoutInfo.pBindings = layoutBindings.data();

		auto result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
		if (result != VK_SUCCESS)
		{
			throw new std::runtime_error("loadImageCube::DescriptorSet layout creation failed with " + std::to_string(result));
		}

		return descriptorSetLayout;
	}
}
