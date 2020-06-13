
#include "DfrRenderer.hpp"

#include <Primitives.hpp>
#include <util/files.hpp>

namespace blaze
{

DfrRenderer::DfrRenderer(GLFWwindow* window, bool enableValidationLayers) noexcept
	: ARenderer(window, enableValidationLayers)
{
	// Depthbuffer
	depthBuffer = createDepthBuffer();
	// Renderpass
	renderPass = createRenderpass();

	// XXX: G-buffer rendering
	// Pipeline layouts
	mrtShader = createMRTShader();
	// Pipeline
	mrtPipeline = createMRTPipeline();

	// XXX: Lightpass
	// Pipeline layouts
	lightingShader = createLightingShader();
	// Pipeline
	lightingPipeline = createLightingPipeline();

	// All uniform buffer stuff
	cameraSets = createCameraSets();
	cameraUBOs = createCameraUBOs();
	settingsUBOs = createSettingsUBOs();

	// Skybox mesh
	// Deferred Quad
	screenQuad = getUVRect(context.get());

	// Lights
	lightCaster = std::make_unique<DfrLightCaster>();

	// G-buffer
	mrtAttachment = createMRTAttachment();
	inputAttachmentSet = createInputAttachmentSet();

	framebuffers = createFramebuffers();
	// Framebuffers
	isComplete = true;
}

void DfrRenderer::recreateSwapchainDependents()
{
	// Depthbuffer
	depthBuffer = createDepthBuffer();
	// Renderpass
	renderPass = createRenderpass();

	// All uniform buffer stuff
	cameraSets = createCameraSets();
	cameraUBOs = createCameraUBOs();
	settingsUBOs = createSettingsUBOs();

	// Lights
	lightCaster->recreate();

	// G-buffer
	mrtAttachment = createMRTAttachment();
	inputAttachmentSet = createInputAttachmentSet();
	framebuffers = createFramebuffers();
	// Framebuffers
}

spirv::RenderPass DfrRenderer::createRenderpass()
{
	assert(depthBuffer.valid());
	std::vector<spirv::AttachmentFormat> attachments;

	spirv::AttachmentFormat* attachment;

	std::vector<VkAttachmentReference> colorRefs;
	VkAttachmentReference swapchainColorRef = {};
	VkAttachmentReference depthRef = {};

	std::vector<VkAttachmentReference> inputRefs;

	uint32_t attachmentRefIdx = 0;

	// Swapchain attachment
	attachment = &attachments.emplace_back();
	attachment->format = swapchain->get_format();
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainColorRef = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// G-buffer attachments
	// POSITION
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// NORMAL
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// ALBEDO
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R8G8B8A8_UNORM;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// OCCLUSION METAL ROUGH
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R8G8B8A8_UNORM;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// EMISSION
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R8G8B8A8_UNORM;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// Depth Attachment
	attachment = &attachments.emplace_back();
	attachment->format = depthBuffer.get_format();
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthRef = {attachmentRefIdx++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	// Input Attachments
	inputRefs = colorRefs;
	for (auto& ref : inputRefs)
	{
		ref.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	std::vector<VkSubpassDescription> subpassDescs = {};
	{
		VkSubpassDescription* subpassDesc;
		subpassDesc = &subpassDescs.emplace_back();
		subpassDesc->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc->colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
		subpassDesc->pColorAttachments = colorRefs.data();
		subpassDesc->pDepthStencilAttachment = &depthRef;

		subpassDesc = &subpassDescs.emplace_back();
		subpassDesc->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc->inputAttachmentCount = static_cast<uint32_t>(inputRefs.size());
		subpassDesc->pInputAttachments = inputRefs.data();
		subpassDesc->colorAttachmentCount = 1;
		subpassDesc->pColorAttachments = &swapchainColorRef;
		subpassDesc->pDepthStencilAttachment = &depthRef;
	}

	spirv::LoadStoreConfig loadStore = {};
	loadStore.colorLoad = spirv::LoadStoreConfig::LoadAction::CLEAR;
	loadStore.colorStore = spirv::LoadStoreConfig::StoreAction::CONTINUE;
	loadStore.depthLoad = spirv::LoadStoreConfig::LoadAction::CLEAR;
	loadStore.depthStore = spirv::LoadStoreConfig::StoreAction::CONTINUE;

	std::vector<VkSubpassDependency> deps;
	{
		VkSubpassDependency* dependency;

		dependency = &deps.emplace_back();
		dependency->srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency->dstSubpass = 0;
		dependency->srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependency->dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency->srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependency->dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependency = &deps.emplace_back();
		dependency->srcSubpass = 0;
		dependency->dstSubpass = 1;
		dependency->srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency->dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependency->srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependency = &deps.emplace_back();
		dependency->srcSubpass = 0;
		dependency->dstSubpass = VK_SUBPASS_EXTERNAL;
		dependency->srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency->dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependency->srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency->dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	}

	return context->get_pipelineFactory()->createRenderPass(attachments, loadStore, subpassDescs, deps);
}

Texture2D DfrRenderer::createDepthBuffer() const
{
	assert(swapchain);
	VkFormat format =
		util::findSupportedFormat(context->get_physicalDevice(),
								  {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
								  VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	ImageData2D imageData = {};
	imageData.format = format;
	imageData.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	imageData.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageData.height = swapchain->get_extent().height;
	imageData.width = swapchain->get_extent().width;
	imageData.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	imageData.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageData.numChannels = 1;
	imageData.size = swapchain->get_extent().width * swapchain->get_extent().height;

	return Texture2D(context.get(), imageData);
}

void DfrRenderer::update(uint32_t frame)
{
	cameraUBOs[frame].write(camera->getUbo());
	settingsUBOs[frame].write(settings);
}

void DfrRenderer::recordCommands(uint32_t frame)
{
	VkRenderPassBeginInfo renderpassBeginInfo = {};
	renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpassBeginInfo.renderPass = renderPass.renderPass.get();
	renderpassBeginInfo.framebuffer = framebuffers[frame];
	renderpassBeginInfo.renderArea.offset = {0, 0};
	renderpassBeginInfo.renderArea.extent = swapchain->get_extent();

	std::array<VkClearValue, 7> clearColor;
	for (auto& clCol : clearColor)
	{
		clCol.color = {0.0f, 0.0f, 0.0f, 0.0f};
	}
	clearColor.back().depthStencil = {1.0f, 0};
	renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
	renderpassBeginInfo.pClearValues = clearColor.data();

	auto& extent = swapchain->get_extent();

	VkViewport viewport = {};
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0;
	viewport.y = static_cast<float>(extent.height);
	viewport.width = static_cast<float>(extent.width);
	viewport.height = -static_cast<float>(extent.height);

	VkRect2D scissor = {};
	scissor.extent = extent;
	scissor.offset = {0, 0};

	vkCmdBeginRenderPass(commandBuffers[frame], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);
	vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

	// Do the systemic thing
	mrtPipeline.bind(commandBuffers[frame]);
	vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, mrtShader.pipelineLayout.get(),
							cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
	for (Drawable* drawable : drawables)
	{
		drawable->drawOpaque(commandBuffers[frame], mrtShader.pipelineLayout.get());
	}

	vkCmdNextSubpass(commandBuffers[frame], VK_SUBPASS_CONTENTS_INLINE);
	lightingPipeline.bind(commandBuffers[frame]);
	vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, lightingShader.pipelineLayout.get(),
							cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, lightingShader.pipelineLayout.get(),
							inputAttachmentSet.setIdx, 1, &inputAttachmentSet.get(), 0, nullptr);
	screenQuad.bind(commandBuffers[frame]);
	vkCmdDrawIndexed(commandBuffers[frame], screenQuad.get_indexCount(), 1, 0, 0, 0);
	vkCmdEndRenderPass(commandBuffers[frame]);
}

void DfrRenderer::setEnvironment(const Bindable* env)
{
}

const spirv::Shader* DfrRenderer::get_shader() const
{
	return &mrtShader;
}

void DfrRenderer::drawSettings()
{
	if (ImGui::Begin("Deferred Settings"))
	{
		settings.viewRT =
			ImGui::RadioButton("Full Render", settings.viewRT == settings.RENDER) ? settings.RENDER : settings.viewRT;
		settings.viewRT = ImGui::RadioButton("MRT Position", settings.viewRT == settings.POSITION) ? settings.POSITION
																								   : settings.viewRT;
		settings.viewRT =
			ImGui::RadioButton("MRT Normal", settings.viewRT == settings.NORMAL) ? settings.NORMAL : settings.viewRT;
		settings.viewRT =
			ImGui::RadioButton("MRT Albedo", settings.viewRT == settings.ALBEDO) ? settings.ALBEDO : settings.viewRT;
		settings.viewRT =
			ImGui::RadioButton("MRT OMR", settings.viewRT == settings.OMR) ? settings.OMR : settings.viewRT;
		settings.viewRT = ImGui::RadioButton("MRT EMISSION", settings.viewRT == settings.EMISSION) ? settings.EMISSION
																								   : settings.viewRT;
	}
	ImGui::End();
}

ALightCaster* DfrRenderer::get_lightCaster()
{
	return lightCaster.get();
}

spirv::Shader DfrRenderer::createMRTShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vMRTShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fMRTShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline DfrRenderer::createMRTPipeline()
{
	assert(mrtShader.valid());

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
	colorblendAttachment.colorWriteMask = 0;
	colorblendAttachment.blendEnable = VK_FALSE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorblendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	std::vector<VkPipelineColorBlendAttachmentState> colorblendAttachments(MRTAttachment::attachmentCount,
																		   colorblendAttachment);

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = static_cast<uint32_t>(colorblendAttachments.size());
	info.colorblendCreateInfo.pAttachments = colorblendAttachments.data();

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_TRUE;
	info.depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
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

	return context->get_pipelineFactory()->createGraphicsPipeline(mrtShader, renderPass, info);
}

DfrRenderer::MRTAttachment DfrRenderer::createMRTAttachment()
{
	MRTAttachment attachment;

	auto extent = swapchain->get_extent();

	ImageData2D id2d = {};
	id2d.height = extent.height;
	id2d.width = extent.width;
	id2d.numChannels = 3;
	id2d.anisotropy = VK_FALSE;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	id2d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	id2d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	id2d.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	id2d.format = VK_FORMAT_R8G8B8A8_UNORM;
	attachment.albedo = Texture2D(context.get(), id2d, false);
	attachment.emission = Texture2D(context.get(), id2d, false);
	attachment.omr = Texture2D(context.get(), id2d, false);

	id2d.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachment.position = Texture2D(context.get(), id2d, false);
	attachment.normal = Texture2D(context.get(), id2d, false);

	return attachment;
}

spirv::SetSingleton DfrRenderer::createInputAttachmentSet()
{
	assert(mrtAttachment.valid());
	assert(lightingShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*lightingShader.getSetWithUniform("I_POSITION"));

	std::vector<const spirv::UniformInfo*> unifs = {
		set.getUniform("I_POSITION"), set.getUniform("I_NORMAL"),	set.getUniform("I_ALBEDO"),
		set.getUniform("I_OMR"),	  set.getUniform("I_EMISSION"),
	};
	std::vector<VkDescriptorImageInfo> infos = {
		mrtAttachment.position.get_imageInfo(), mrtAttachment.normal.get_imageInfo(),
		mrtAttachment.albedo.get_imageInfo(),	mrtAttachment.omr.get_imageInfo(),
		mrtAttachment.emission.get_imageInfo(),
	};
	std::vector<VkWriteDescriptorSet> writes(5);

	for (int i = 0; i < 5; ++i)
	{
		const spirv::UniformInfo* unif = unifs[i];
		VkDescriptorImageInfo* info = &infos[i];
		info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Read
		info->sampler = VK_NULL_HANDLE;								  // No sampler for input attachments

		VkWriteDescriptorSet* write = &writes[i];
		write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write->descriptorType = unif->type;
		write->descriptorCount = unif->arrayLength;
		write->dstSet = set.get();
		write->dstBinding = unif->binding;
		write->dstArrayElement = 0;
		write->pImageInfo = info;
	}

	vkUpdateDescriptorSets(context->get_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	return std::move(set);
}

spirv::Shader DfrRenderer::createLightingShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vLightingShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fLightingShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline DfrRenderer::createLightingPipeline()
{
	assert(lightingShader.valid());

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
	colorblendAttachment.colorWriteMask = 0;
	colorblendAttachment.blendEnable = VK_FALSE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorblendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 1;
	info.colorblendCreateInfo.pAttachments = &colorblendAttachment;

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;
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

	info.subpass = 1;

	return context->get_pipelineFactory()->createGraphicsPipeline(lightingShader, renderPass, info);
}

vkw::FramebufferVector DfrRenderer::createFramebuffers()
{
	using namespace std;
	assert(depthBuffer.valid());
	assert(mrtAttachment.valid());

	vector<VkFramebuffer> frameBuffers(maxFrameInFlight);
	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		vector<VkImageView> attachments = {
			swapchain->get_imageView(i),		  mrtAttachment.position.get_imageView(),
			mrtAttachment.normal.get_imageView(), mrtAttachment.albedo.get_imageView(),
			mrtAttachment.omr.get_imageView(),	  mrtAttachment.emission.get_imageView(),
			depthBuffer.get_imageView(),
		};

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass.get();
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.width = swapchain->get_extent().width;
		createInfo.height = swapchain->get_extent().height;
		createInfo.layers = 1;

		auto result = vkCreateFramebuffer(context->get_device(), &createInfo, nullptr, &frameBuffers[i]);
		if (result != VK_SUCCESS)
		{
			for (size_t j = 0; j < i; j++)
			{
				vkDestroyFramebuffer(context->get_device(), frameBuffers[i], nullptr);
			}
			throw runtime_error("Framebuffer creation failed with " + std::to_string(result));
		}
	}
	return vkw::FramebufferVector(std::move(frameBuffers), context->get_device());
}

spirv::SetVector DfrRenderer::createCameraSets()
{
	return context->get_pipelineFactory()->createSets(*mrtShader.getSetWithUniform("camera"), maxFrameInFlight);
}

DfrRenderer::CameraUBOV DfrRenderer::createCameraUBOs()
{
	auto unif = mrtShader.getUniform("camera");

	auto ubos = CameraUBOV(context.get(), {}, maxFrameInFlight);

	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		VkDescriptorBufferInfo info = ubos[i].get_descriptorInfo();

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = unif->type;
		write.descriptorCount = unif->arrayLength;
		write.dstSet = cameraSets[i];
		write.dstBinding = unif->binding;
		write.dstArrayElement = 0;
		write.pBufferInfo = &info;

		vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);
	}

	return ubos;
}

DfrRenderer::SettingsUBOV DfrRenderer::createSettingsUBOs()
{
	assert(lightingShader.valid());
	auto unif = lightingShader.getUniform("settings");

	auto ubos = SettingsUBOV(context.get(), {}, maxFrameInFlight);

	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		VkDescriptorBufferInfo info = ubos[i].get_descriptorInfo();

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = unif->type;
		write.descriptorCount = unif->arrayLength;
		write.dstSet = cameraSets[i];
		write.dstBinding = unif->binding;
		write.dstArrayElement = 0;
		write.pBufferInfo = &info;

		vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);
	}

	return ubos;
}
} // namespace blaze
