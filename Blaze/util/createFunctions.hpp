
#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <string>

namespace blaze::util
{
	VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& shaderCode);

	VkSemaphore createSemaphore(VkDevice device);

	VkFence createFence(VkDevice device);

	VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspect, uint32_t miplevels);

	VkDescriptorPool createDescriptorPool(VkDevice device, std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

	VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::vector<VkDescriptorSetLayoutBinding>& layoutBindings);

	VkRenderPass createRenderPass(VkDevice device, VkFormat colorAttachmentFormat, VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkPipelineLayout createPipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout> descriptorSetLayouts, const std::vector<VkPushConstantRange>& pushConstantRanges);

	VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
		VkExtent2D viewPortSize, const std::string& vShader, const std::string& fShader,
		const std::vector<VkDynamicState>& dynamicStates = {},
		VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, VkBool32 depthTest = VK_TRUE,
		VkBool32 depthWrite = VK_TRUE, VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS);
}
