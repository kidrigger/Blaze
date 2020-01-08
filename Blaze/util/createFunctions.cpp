#include "createFunctions.hpp"

#include <stdexcept>
#include <optional>
#include <string>

#include "Managed.hpp"
#include "files.hpp"
#include <Datatypes.hpp>

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
			throw new std::runtime_error("DescriptorSet layout creation failed with " + std::to_string(result));
		}
		return descriptorSetLayout;
	}
	
	VkRenderPass createRenderPassMultiView(VkDevice device, uint32_t viewMask, VkFormat colorAttachmentFormat, VkFormat depthAttachmentFormat, VkImageLayout finalLayout, VkImageLayout initialLayout, VkAttachmentLoadOp colorLoadOp)
	{
		std::vector<VkAttachmentDescription> attachments;
		VkAttachmentReference colorAttachmentRef;
		uint32_t idx = 0;
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = colorAttachmentFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = colorLoadOp;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = initialLayout;
		colorAttachment.finalLayout = finalLayout;
		attachments.push_back(colorAttachment);

		colorAttachmentRef = { idx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		std::optional<VkAttachmentReference> depthAttachmentRef;
		if (depthAttachmentFormat != VK_FORMAT_UNDEFINED)
		{
			VkAttachmentDescription attachment = {};
			attachment.format = depthAttachmentFormat;
			attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments.push_back(attachment);

			depthAttachmentRef = { idx++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		}

		VkSubpassDescription subpassDesc = {};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = 1;
		subpassDesc.pColorAttachments = &colorAttachmentRef;
		subpassDesc.pDepthStencilAttachment = depthAttachmentRef.has_value() ? &depthAttachmentRef.value() : nullptr;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassMultiviewCreateInfo renderPassMultiviewCI{};
		renderPassMultiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
		renderPassMultiviewCI.subpassCount = 1;
		renderPassMultiviewCI.pViewMasks = &viewMask;
		renderPassMultiviewCI.correlationMaskCount = 0;
		renderPassMultiviewCI.pCorrelationMasks = nullptr;

		VkRenderPassCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpassDesc;
		createInfo.dependencyCount = 1;
		createInfo.pDependencies = &dependency;
		createInfo.pNext = &renderPassMultiviewCI;

		VkRenderPass renderpass = VK_NULL_HANDLE;
		auto result = vkCreateRenderPass(device, &createInfo, nullptr, &renderpass);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("RenderPass creation failed with " + std::to_string(result));
		}
		return renderpass;
	}

	VkRenderPass createRenderPass(VkDevice device, VkFormat colorAttachmentFormat, VkFormat depthAttachmentFormat, VkImageLayout finalLayout, VkImageLayout initialLayout, VkAttachmentLoadOp colorLoadOp)
	{
		std::vector<VkAttachmentDescription> attachments;
		VkAttachmentReference colorAttachmentRef;
		uint32_t idx = 0;
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = colorAttachmentFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = colorLoadOp;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = initialLayout;
		colorAttachment.finalLayout = finalLayout;
		attachments.push_back(colorAttachment);

		colorAttachmentRef = { idx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		std::optional<VkAttachmentReference> depthAttachmentRef;
		if (depthAttachmentFormat != VK_FORMAT_UNDEFINED)
		{
			VkAttachmentDescription attachment = {};
			attachment.format = depthAttachmentFormat;
			attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments.push_back(attachment);

			depthAttachmentRef = { idx++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		}

		VkSubpassDescription subpassDesc = {};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = 1;
		subpassDesc.pColorAttachments = &colorAttachmentRef;
		subpassDesc.pDepthStencilAttachment = depthAttachmentRef.has_value() ? &depthAttachmentRef.value() : nullptr;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpassDesc;
		createInfo.dependencyCount = 1;
		createInfo.pDependencies = &dependency;

		VkRenderPass renderpass = VK_NULL_HANDLE;
		auto result = vkCreateRenderPass(device, &createInfo, nullptr, &renderpass);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("RenderPass creation failed with " + std::to_string(result));
		}
		return renderpass;
	}

	VkRenderPass createShadowRenderPass(VkDevice device, VkFormat depthAttachmentFormat, VkImageLayout finalLayout)
	{
		std::vector<VkAttachmentDescription> attachments;

		uint32_t idx = 0;

		VkAttachmentReference depthAttachmentRef;
		VkAttachmentDescription attachment = {};
		attachment.format = depthAttachmentFormat;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = finalLayout;
		attachments.push_back(attachment);

		depthAttachmentRef = { idx++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDesc = {};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = 0;
		subpassDesc.pColorAttachments = nullptr;
		subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;

		std::vector<VkSubpassDependency> dependencies = {};
		{
			VkSubpassDependency dependency = {};

			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			dependencies.push_back(dependency);

			dependency.srcSubpass = 0;
			dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
			dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
			dependencies.push_back(dependency);
		}

		VkRenderPassCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpassDesc;
		createInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
		createInfo.pDependencies = dependencies.data();

		VkRenderPass renderpass = VK_NULL_HANDLE;
		auto result = vkCreateRenderPass(device, &createInfo, nullptr, &renderpass);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("RenderPass creation failed with " + std::to_string(result));
		}
		return renderpass;
	}

	VkPipelineLayout createPipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout> descriptorSetLayouts, const std::vector<VkPushConstantRange>& pushConstantRanges)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		auto result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Pipeline Layout Creation Failed with " + result);
		}
		return pipelineLayout;
	}

	VkPipeline createGraphicsPipeline(VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
		VkExtent2D viewPortSize, const std::string& vShader, const std::string& fShader, const std::vector<VkDynamicState>& dynamicStates,
		VkCullModeFlags cullMode, VkBool32 depthTest, VkBool32 depthWrite, VkCompareOp depthCompareOp,
		VkVertexInputBindingDescription vertBindingDescription, const std::vector<VkVertexInputAttributeDescription>& vertAttributeDescription)
	{
		VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
		VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
		
		auto vertexShaderCode = util::loadBinaryFile(vShader);
		auto fragmentShaderCode = util::loadBinaryFile(fShader);

		auto vertexShaderModule = util::Managed(util::createShaderModule(device, vertexShaderCode), [device](VkShaderModule& sm) { vkDestroyShaderModule(device, sm, nullptr); });
		auto fragmentShaderModule = util::Managed(util::createShaderModule(device, fragmentShaderCode), [device](VkShaderModule& sm) { vkDestroyShaderModule(device, sm, nullptr); });

		vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexShaderStageCreateInfo.module = vertexShaderModule.get();
		vertexShaderStageCreateInfo.pName = "main";

		fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentShaderStageCreateInfo.module = fragmentShaderModule.get();
		fragmentShaderStageCreateInfo.pName = "main";

		std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfo = {
			vertexShaderStageCreateInfo,
			fragmentShaderStageCreateInfo
		};

		auto bindingDescriptions = vertBindingDescription;
		auto attributeDescriptions = vertAttributeDescription;

		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescriptions;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(viewPortSize.height);
		viewport.width = (float)viewPortSize.width;
		viewport.height = -(float)viewPortSize.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = viewPortSize;

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
		rasterizerCreateInfo.cullMode = cullMode;
		rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizerCreateInfo.depthBiasEnable = VK_TRUE;

		VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
		multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
		multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorblendAttachment = {};
		colorblendAttachment.blendEnable = VK_TRUE;
		colorblendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorblendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorblendCreateInfo = {};
		colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorblendCreateInfo.logicOpEnable = VK_FALSE;
		colorblendCreateInfo.attachmentCount = 1;
		colorblendCreateInfo.pAttachments = &colorblendAttachment;

		VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
		depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilStateCreateInfo.depthTestEnable = depthTest;
		depthStencilStateCreateInfo.depthWriteEnable = depthWrite;
		depthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
		depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
		depthStencilStateCreateInfo.maxDepthBounds = 0.0f;	// Don't care
		depthStencilStateCreateInfo.minDepthBounds = 1.0f;	// Don't care
		depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
		depthStencilStateCreateInfo.front = {}; // Don't Care
		depthStencilStateCreateInfo.back = {}; // Don't Care

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStagesCreateInfo.size());
		pipelineCreateInfo.pStages = shaderStagesCreateInfo.data();
		pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
		pipelineCreateInfo.pColorBlendState = &colorblendCreateInfo;
		pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		VkPipeline graphicsPipeline = VK_NULL_HANDLE;
		auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Graphics Pipeline creation failed with " + std::to_string(result));
		}
		return graphicsPipeline;
	}
}
