
#include "Environment.hpp"

#include <util/processing.hpp>
#include <rendering/ARenderer.hpp>
#include <util/files.hpp>
#include <vector>
#include <thirdparty/optick/optick.h>

namespace blaze::util
{
TextureCube Environment::createIrradianceCube(const Context* context, spirv::SetSingleton* environment)
{
	OPTICK_EVENT();
	auto timer = AutoTimer("Irradiance Cube Generation took (us)");

	spirv::RenderPass renderpass;
	spirv::Shader shader;
	spirv::Pipeline pipeline;
	spirv::Framebuffer framebuffer;
	TextureCube irradianceMap;
	spirv::SetSingleton descriptorSet;

	struct PCB
	{
		float deltaPhi{(2.0f * glm::pi<float>()) / 180.0f};
		float deltaTheta{(0.5f * glm::pi<float>()) / 64.0f};
	} pcb = {};

	uint32_t dim = 64u;
	
	ImageDataCube idc{};
	idc.height = dim;
	idc.width = dim;
	idc.numChannels = 4;
	idc.size = 4 * 6 * dim * dim;
	idc.layerSize = 4 * dim * dim;
	idc.usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	idc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	idc.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	idc.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	irradianceMap = TextureCube(context, idc, false);

	std::vector<spirv::ShaderStageData> shaderStages;
	{
		spirv::ShaderStageData* stage = &shaderStages.emplace_back();
		stage->spirv = loadBinaryFile("shaders/env/vIrradiance.vert.spv");
		stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

		stage = &shaderStages.emplace_back();
		stage->spirv = loadBinaryFile("shaders/env/fIrradiance.frag.spv");
		stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	spirv::AttachmentFormat format = {
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		irradianceMap.get_format(),
		VK_SAMPLE_COUNT_1_BIT,
		spirv::LoadStoreConfig(spirv::LoadStoreConfig::LoadAction::DONT_CARE,
							   spirv::LoadStoreConfig::StoreAction::READ),
	};

	VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass = {};
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.inputAttachmentCount = 0;
	subpass.preserveAttachmentCount = 0;
	subpass.pDepthStencilAttachment = nullptr;

	uint32_t viewmask = 0b111111;

	VkRenderPassMultiviewCreateInfo multiview = {};
	multiview.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
	multiview.pNext = nullptr;
	multiview.subpassCount = 1;
	multiview.pViewMasks = &viewmask;
	multiview.correlationMaskCount = 0;
	multiview.pCorrelationMasks = nullptr;
	multiview.dependencyCount = 0;
	multiview.pViewOffsets = nullptr;

	renderpass = context->get_pipelineFactory()->createRenderPass({format}, {subpass}, &multiview);

	VkClearValue clearColor = {};
	clearColor.color = {0.0f, 0.0f, 0.0f, 1.0f};
	renderpass.clearValues = {clearColor};

	shader = context->get_pipelineFactory()->createShader(shaderStages);

	spirv::GraphicsPipelineCreateInfo info = {};

	info.inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.inputAssemblyCreateInfo.flags = 0;
	info.inputAssemblyCreateInfo.pNext = nullptr;
	info.inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	info.inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	info.rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	info.rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	info.rasterizerCreateInfo.lineWidth = 1.0f;
	info.rasterizerCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	info.rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	info.rasterizerCreateInfo.depthBiasEnable = VK_TRUE;
	info.rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	info.rasterizerCreateInfo.pNext = nullptr;
	info.rasterizerCreateInfo.flags = 0;

	info.multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	info.multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblendAttachment = {};
	colorblendAttachment.blendEnable = VK_FALSE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 1;
	info.colorblendCreateInfo.pAttachments = &colorblendAttachment;

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	info.depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.maxDepthBounds = 0.0f; // Don't care
	info.depthStencilCreateInfo.minDepthBounds = 1.0f; // Don't care
	info.depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.front = {}; // Don't Care
	info.depthStencilCreateInfo.back = {};	// Don't Care

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	info.dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	info.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	info.dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	pipeline = context->get_pipelineFactory()->createGraphicsPipeline(shader, renderpass, info);

	framebuffer =
		context->get_pipelineFactory()->createFramebuffer(renderpass, {dim, dim}, {irradianceMap.get_imageView()});

	auto cube = getUVCube(context);

	CubemapUBlock uboData = {
		glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f),
		{
			// POSITIVE_X (Outside in - so NEG_X face)
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// NEGATIVE_X (Outside in - so POS_X face)
			glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// POSITIVE_Y
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
			// NEGATIVE_Y
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			// POSITIVE_Z
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// NEGATIVE_Z
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		}};

	UBO<CubemapUBlock> ubo(context, uboData);

	descriptorSet = context->get_pipelineFactory()->createSet(*shader.getSetWithUniform("pv"));

	{
		VkDescriptorBufferInfo info = ubo.get_descriptorInfo();

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.descriptorCount = 1;
		write.dstSet = descriptorSet.get();
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.pBufferInfo = &info;

		vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);
	}

	glm::mat4 mvppcb{};

	auto cmdBuffer = context->startCommandBufferRecord();

	// RENDERPASSES

	renderpass.begin(cmdBuffer, framebuffer);

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(dim);
	viewport.width = (float)dim;
	viewport.height = -(float)dim;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = {dim, dim};

	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

	pipeline.bind(cmdBuffer);
	vkCmdBindDescriptorSets(cmdBuffer, pipeline.bindPoint, shader.pipelineLayout.get(), 0, 1,
							&descriptorSet.get(), 0, nullptr);
	vkCmdBindDescriptorSets(cmdBuffer, pipeline.bindPoint, shader.pipelineLayout.get(), 1, 1,
							&environment->get(), 0, nullptr);

	mvppcb = glm::mat4(1.0f);
	vkCmdPushConstants(cmdBuffer, shader.pipelineLayout.get(), shader.pushConstant.stage, 0,
						sizeof(glm::mat4), &mvppcb);
	vkCmdPushConstants(cmdBuffer, shader.pipelineLayout.get(), shader.pushConstant.stage,
						sizeof(glm::mat4), sizeof(PCB), &pcb);

	VkDeviceSize offsets = {0};

	cube.bind(cmdBuffer);
	vkCmdDrawIndexed(cmdBuffer, cube.get_indexCount(), 1, 0, 0, 0);

	renderpass.end(cmdBuffer);
	context->flushCommandBuffer(cmdBuffer);

	irradianceMap.implicitTransferLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

	return irradianceMap;
}

TextureCube Environment::createPrefilteredCube(const Context* context, spirv::SetSingleton* environment)
{
	OPTICK_EVENT();

	struct PCB
	{
		float roughness = 0;
		float miplevel = 0;
	} pcb;

	auto timer = AutoTimer("Prefilter generation took (us)");

	const uint32_t dim = 128u;

	spirv::Shader shader;
	spirv::Pipeline pipeline;
	spirv::RenderPass renderpass;
	spirv::Framebuffer framebuffer;

	VkFormat imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	// Setup the TextureCube
	ImageDataCube idc{};
	idc.height = dim;
	idc.width = dim;
	idc.numChannels = 4;
	idc.size = 4 * 6 * dim * dim;
	idc.layerSize = 4 * dim * dim;
	idc.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	idc.format = imageFormat;
	idc.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	TextureCube prefilterMap(context, idc, true);

	ImageData2D id2d{};
	id2d.height = dim;
	id2d.width = dim;
	id2d.numChannels = 4;
	id2d.size = 4 * dim * dim;
	id2d.format = imageFormat;
	id2d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	id2d.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	Texture2D fbColorAttachment(context, id2d, false);
	
	std::vector<spirv::ShaderStageData> shaderStages;
	{
		spirv::ShaderStageData* stage = &shaderStages.emplace_back();
		stage->spirv = loadBinaryFile("shaders/env/vPrefilter.vert.spv");
		stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

		stage = &shaderStages.emplace_back();
		stage->spirv = loadBinaryFile("shaders/env/fPrefilter.frag.spv");
		stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	spirv::AttachmentFormat format = {
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		fbColorAttachment.get_format(),
		VK_SAMPLE_COUNT_1_BIT,
		spirv::LoadStoreConfig(spirv::LoadStoreConfig::LoadAction::DONT_CARE,
							   spirv::LoadStoreConfig::StoreAction::CONTINUE),
	};

	VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass = {};
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.inputAttachmentCount = 0;
	subpass.preserveAttachmentCount = 0;
	subpass.pDepthStencilAttachment = nullptr;

	renderpass = context->get_pipelineFactory()->createRenderPass({format}, {subpass});

	std::vector<VkClearValue> clearColor(1);
	clearColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
	renderpass.clearValues = std::move(clearColor);

	shader = context->get_pipelineFactory()->createShader(shaderStages);

	spirv::GraphicsPipelineCreateInfo info = {};

	info.inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.inputAssemblyCreateInfo.flags = 0;
	info.inputAssemblyCreateInfo.pNext = nullptr;
	info.inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	info.inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	info.rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	info.rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	info.rasterizerCreateInfo.lineWidth = 1.0f;
	info.rasterizerCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	info.rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	info.rasterizerCreateInfo.depthBiasEnable = VK_TRUE;
	info.rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	info.rasterizerCreateInfo.pNext = nullptr;
	info.rasterizerCreateInfo.flags = 0;

	info.multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	info.multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblendAttachment = {};
	colorblendAttachment.blendEnable = VK_FALSE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 1;
	info.colorblendCreateInfo.pAttachments = &colorblendAttachment;

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	info.depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.maxDepthBounds = 0.0f; // Don't care
	info.depthStencilCreateInfo.minDepthBounds = 1.0f; // Don't care
	info.depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.front = {}; // Don't Care
	info.depthStencilCreateInfo.back = {};	// Don't Care

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	info.dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	info.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	info.dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	pipeline = context->get_pipelineFactory()->createGraphicsPipeline(shader, renderpass, info);

	framebuffer =
		context->get_pipelineFactory()->createFramebuffer(renderpass, {dim, dim}, {fbColorAttachment.get_imageView()});

	auto cube = getUVCube(context);

	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f);

	std::vector<glm::mat4> matrices = {
		// POSITIVE_X (Outside in - so NEG_X face)
		glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		// NEGATIVE_X (Outside in - so POS_X face)
		glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		// POSITIVE_Y
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
		// NEGATIVE_Y
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		// POSITIVE_Z
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		// NEGATIVE_Z
		glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
	};

	glm::mat4 mvppcb{};

	uint32_t totalMips = prefilterMap.get_miplevels();
	uint32_t mipsize = dim;
	auto cmdBuffer = context->startCommandBufferRecord();
	for (uint32_t miplevel = 0; miplevel < totalMips; miplevel++)
	{
		// RENDERPASSES
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = static_cast<float>(mipsize);
		viewport.width = static_cast<float>(mipsize);
		viewport.height = -static_cast<float>(mipsize);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = {0, 0};
		scissor.extent = {mipsize, mipsize};
		framebuffer.renderArea = scissor;

		for (int face = 0; face < 6; face++)
		{
			renderpass.begin(cmdBuffer, framebuffer);
			vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
			vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

			pipeline.bind(cmdBuffer);

			vkCmdBindDescriptorSets(cmdBuffer, pipeline.bindPoint, shader.pipelineLayout.get(), 0, 1,
									&environment->get(), 0, nullptr);

			mvppcb = proj * matrices[face];
			pcb.roughness = (float)miplevel / (float)(totalMips - 1);
			pcb.miplevel = static_cast<float>(miplevel);
			vkCmdPushConstants(cmdBuffer, shader.pipelineLayout.get(), shader.pushConstant.stage, 0,
							   sizeof(glm::mat4), &mvppcb);
			vkCmdPushConstants(cmdBuffer, shader.pipelineLayout.get(), shader.pushConstant.stage,
							   sizeof(glm::mat4), sizeof(PCB), &pcb);

			cube.bind(cmdBuffer);
			vkCmdDrawIndexed(cmdBuffer, cube.get_indexCount(), 1, 0, 0, 0);

			renderpass.end(cmdBuffer);
			fbColorAttachment.implicitTransferLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
													 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

			fbColorAttachment.transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
											 VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
											 VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = {0, 0, 0};

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = face;
			copyRegion.dstSubresource.mipLevel = miplevel;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = {0, 0, 0};

			copyRegion.extent.width = static_cast<uint32_t>(mipsize);
			copyRegion.extent.height = static_cast<uint32_t>(mipsize);
			copyRegion.extent.depth = 1;

			vkCmdCopyImage(cmdBuffer, fbColorAttachment.get_image(), fbColorAttachment.get_imageInfo().imageLayout,
						   prefilterMap.get_image(), prefilterMap.get_imageInfo().imageLayout, 1, &copyRegion);
		}

		mipsize /= 2;
	}

	prefilterMap.transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
								 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	context->flushCommandBuffer(cmdBuffer);

	return prefilterMap;
}

Texture2D Environment::createBrdfLut(const Context* context)
{
	OPTICK_EVENT();

	const uint32_t dim = 512;

	spirv::Shader shader;
	spirv::Pipeline pipeline;
	spirv::RenderPass renderpass;
	spirv::Framebuffer framebuffer;

	auto rect = getUVRect(context);

	VkDevice device = context->get_device();

	auto timer = AutoTimer("BRDF LUT generation took (us)");

	ImageData2D id2d{};
	id2d.height = dim;
	id2d.width = dim;
	id2d.numChannels = 4;
	id2d.size = 4 * dim * dim;
	id2d.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	id2d.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	id2d.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	id2d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	Texture2D lut(context, id2d, false);

	std::vector<spirv::ShaderStageData> shaderStages;
	{
		spirv::ShaderStageData* stage = &shaderStages.emplace_back();
		stage->spirv = loadBinaryFile("shaders/env/vBrdfLut.vert.spv");
		stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

		stage = &shaderStages.emplace_back();
		stage->spirv = loadBinaryFile("shaders/env/fBrdfLut.frag.spv");
		stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	spirv::AttachmentFormat format = {
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		lut.get_format(),
		VK_SAMPLE_COUNT_1_BIT,
		spirv::LoadStoreConfig(spirv::LoadStoreConfig::LoadAction::DONT_CARE,
							   spirv::LoadStoreConfig::StoreAction::READ),
	};

	VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpass = {};
	subpass.flags = 0;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.inputAttachmentCount = 0;
	subpass.preserveAttachmentCount = 0;
	subpass.pDepthStencilAttachment = nullptr;

	renderpass = context->get_pipelineFactory()->createRenderPass({format}, {subpass});

	VkClearValue clearColor = {};
	clearColor.color = {0.0f, 0.0f, 0.0f, 1.0f};
	renderpass.clearValues = {clearColor};

	shader = context->get_pipelineFactory()->createShader(shaderStages);

	spirv::GraphicsPipelineCreateInfo info = {};

	info.inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.inputAssemblyCreateInfo.flags = 0;
	info.inputAssemblyCreateInfo.pNext = nullptr;
	info.inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	info.inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	info.rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	info.rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	info.rasterizerCreateInfo.lineWidth = 1.0f;
	info.rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	info.rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	info.rasterizerCreateInfo.depthBiasEnable = VK_TRUE;
	info.rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	info.rasterizerCreateInfo.pNext = nullptr;
	info.rasterizerCreateInfo.flags = 0;

	info.multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	info.multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblendAttachment = {};
	colorblendAttachment.blendEnable = VK_FALSE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 1;
	info.colorblendCreateInfo.pAttachments = &colorblendAttachment;

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	info.depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.maxDepthBounds = 0.0f; // Don't care
	info.depthStencilCreateInfo.minDepthBounds = 1.0f; // Don't care
	info.depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.front = {}; // Don't Care
	info.depthStencilCreateInfo.back = {};	// Don't Care

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	info.dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	info.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	info.dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	pipeline = context->get_pipelineFactory()->createGraphicsPipeline(shader, renderpass, info);

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(dim);
	viewport.width = (float)dim;
	viewport.height = -(float)dim;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = {dim, dim};

	framebuffer = context->get_pipelineFactory()->createFramebuffer(renderpass, scissor.extent, {lut.get_imageView()});

	auto cmdBuffer = context->startCommandBufferRecord();

	// RENDERPASSES
	renderpass.begin(cmdBuffer, framebuffer);

	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

	pipeline.bind(cmdBuffer);

	rect.bind(cmdBuffer);
	vkCmdDrawIndexed(cmdBuffer, rect.get_indexCount(), 1, 0, 0, 0);

	renderpass.end(cmdBuffer);
	lut.implicitTransferLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

	context->flushCommandBuffer(cmdBuffer);

	return lut;
}

Environment::Environment(const Context* context, TextureCube&& skybox, spirv::SetSingleton* environment)
{
	const spirv::UniformInfo* skyboxInfo = nullptr;
	const spirv::UniformInfo* irradianceInfo = nullptr;
	const spirv::UniformInfo* prefilteredInfo = nullptr;
	const spirv::UniformInfo* brdfLutInfo = nullptr;
	for (auto& uniform : environment->info)
	{
		if (uniform.name == "skybox")
		{
			skyboxInfo = &uniform;
		}
		else if (uniform.name == "irradianceMap")
		{
			irradianceInfo = &uniform;
		}
		else if (uniform.name == "prefilteredMap")
		{
			prefilteredInfo = &uniform;
		}
		else if (uniform.name == "brdfLUT")
		{
			brdfLutInfo = &uniform;
		}
		else
		{
			std::cerr << "UNKNOWN UNIFORM IN ENVIRONMENT SET" << std::endl;
		}
	}
	assert(skyboxInfo != nullptr);
	assert(irradianceInfo != nullptr);
	assert(prefilteredInfo != nullptr);
	assert(brdfLutInfo != nullptr);

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = environment->get();
	write.dstArrayElement = 0;
	write.descriptorCount = 1;

	this->skybox = std::move(skybox);

	write.dstBinding = skyboxInfo->binding;
	write.descriptorType = skyboxInfo->type;
	write.pImageInfo = &this->skybox.get_imageInfo();
	vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);

	this->irradianceMap = createIrradianceCube(context, environment);

	write.dstBinding = irradianceInfo->binding;
	write.descriptorType = irradianceInfo->type;
	write.pImageInfo = &this->irradianceMap.get_imageInfo();
	vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);

	this->prefilteredMap = createPrefilteredCube(context, environment);

	write.dstBinding = prefilteredInfo->binding;
	write.descriptorType = prefilteredInfo->type;
	write.pImageInfo = &this->prefilteredMap.get_imageInfo();
	vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);

	this->brdfLut = createBrdfLut(context);

	write.dstBinding = brdfLutInfo->binding;
	write.descriptorType = brdfLutInfo->type;
	write.pImageInfo = &this->brdfLut.get_imageInfo();
	vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);
}
} // namespace blaze::util
