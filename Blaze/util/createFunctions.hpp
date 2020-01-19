
#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>
#include <string>
#include <Datatypes.hpp>

namespace blaze::util
{
    /**
     * @brief Creates a shader module from the code.
     *
     * @param device The logical device used.
     * @param shaderCode The code as a vector<char>
     */
	VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& shaderCode);

    /**
     * @brief Creates a semaphore on the device.
     *
     * @param device The logical device used.
     */
	VkSemaphore createSemaphore(VkDevice device);

    /**
     * @brief Creates a fence on the device.
     *
     * @param device The logical device used.
     */
	VkFence createFence(VkDevice device);

    /**
     * @brief Creates an image view.
     *
     * @param device The logical device used.
     * @param image The image to create the view for.
     * @param viewType The type of the imageView.
     * @param format The format of the image.
     * @param aspect The aspect to use the image as.
     * @param miplevels The number of levels of mipmapping in the image.
     */
	VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType viewType, VkFormat format, VkImageAspectFlags aspect, uint32_t miplevels);

    /**
     * @brief Creates a new descriptor pool as per the poolsizes.
     *
     * @param device The logical device used.
     * @param poolSizes The vector of pool sizes to support.
     * @param maxSets The maximum number of descriptor sets to support.
     */
	VkDescriptorPool createDescriptorPool(VkDevice device, std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);

    /**
     * @brief Creates a descriptor set layout as per the bindings.
     *
     * @param device The logical device used.
     * @param layoutBindings The vector of layout bindings in the descriptor set.
     */
	VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device, std::vector<VkDescriptorSetLayoutBinding>& layoutBindings);

    /**
     * @brief Creates a renderpass as per the configuration.
     *
     * @param device The logical device used.
     * @param colorAttachmentFormat The format used in the color attachment.
     * @param depthAttachmentFormat The format used for the depth attachment
     * (VK_FORMAT_UNDEFINED if no depth attachment)
     * @param finalLayout The layout the color attachment should end in.
     * @param initialLayout The initial layout of color attachments attached.
     * @param colorLoadOp The load operation for color attachment.
     */
	VkRenderPass createRenderPass(VkDevice device, VkFormat colorAttachmentFormat, VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED,
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR);

    /**
     * @brief Creates a renderpass for shadow as per the configuration.
     *
     * @param device The logical device used.
     * @param depthAttachmentFormat The format used for the depth attachment
     * @param finalLayout The layout the depth attachment should end in.
     */
	VkRenderPass createShadowRenderPass(VkDevice device, VkFormat depthAttachmentFormat = VK_FORMAT_D32_SFLOAT, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
	
    /**
     * @brief Creates a renderpass for multiview framebuffer as per the configuration.
     *
     * @param device The logical device used.
     * @param viewMask The mask defining the multiview indices to write to.
     * @param colorAttachmentFormat The format used in the color attachment.
     * @param depthAttachmentFormat The format used for the depth attachment
     * (VK_FORMAT_UNDEFINED if no depth attachment)
     * @param finalLayout The layout the color attachment should end in.
     * @param initialLayout The initial layout of color attachments attached.
     * @param colorLoadOp The load operation for color attachment.
     */
	VkRenderPass createRenderPassMultiView(VkDevice device, uint32_t viewMask, VkFormat colorAttachmentFormat, VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED,
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		VkAttachmentLoadOp colorLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR);

    /**
     * @brief Creates a pipeline layout.
     *
     * @param device The logical device in use.
     * @param descriptorSetLayouts All the descriptorSetLayouts to support.
     * @param pushConstantRanges The push constant ranges to use.
     */
	VkPipelineLayout createPipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout> descriptorSetLayouts, const std::vector<VkPushConstantRange>& pushConstantRanges);

    /**
     * @brief Create the graphics pipeline.
     *
     * @param device The logical device in use.
     * @param pipelineLayout The layout to use for the pipeline.
     * @param renderPass The renderpass to be used.
     * @param viewPortSize The extent2D for the viewport.
     * @param vShader The file location of the vertex shader.
     * @param fShader The file location of the fragment shader.
     * @param dynamicStates The vector of dynamic states to support.
     * @param cullMode The pipeline cull mode.
     * @param depthTest Is depth test enabled?
     * @param depthWrite Should write to depth map?
     * @param depthCompareOp How to compare depth?
     * @param vertBindingDescription The binding description for the vertices.
     * @param vertAttributeDescription The attribute description for the vertices.
     */
	VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
		VkExtent2D viewPortSize, const std::string& vShader, const std::string& fShader,
		const std::vector<VkDynamicState>& dynamicStates = {},
		VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, VkBool32 depthTest = VK_TRUE,
		VkBool32 depthWrite = VK_TRUE, VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS,
		VkVertexInputBindingDescription vertBindingDescription = Vertex::getBindingDescription(),
		const std::vector<VkVertexInputAttributeDescription>& vertAttributeDescription = Vertex::getAttributeDescriptions());
}
