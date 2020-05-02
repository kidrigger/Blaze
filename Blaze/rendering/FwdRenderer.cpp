
#include "FwdRenderer.hpp"

#include <Primitives.hpp>
#include <util/files.hpp>

namespace blaze
{
FwdRenderer::FwdRenderer(GLFWwindow* window, bool enableValidationLayers) noexcept
	: ARenderer(window, enableValidationLayers)
{
	pipelineFactory = spirv::PipelineFactory(context->get_device());
	// Depthbuffer
	depthBuffer = createDepthBuffer();
	renderPass = createRenderpass();

	// All uniform buffer stuff

	// DescriptorPool

	// Pipeline layouts
	// Pipeline
	shader = createShader();
	pipeline = createPipeline();

	std::vector<Vertex> vs(3);
	vs[0].position = glm::vec3(0.5f, -0.5f, 0.5f);
	vs[1].position = glm::vec3(0.0f, 0.7f, 0.5f);
	vs[2].position = glm::vec3(-0.5f, -0.5f, 0.5f);

	std::vector<uint32_t> is = {0, 1, 2};

	cube = IndexedVertexBuffer<Vertex>(*context, is, vs);

	// Framebuffers
	renderFramebuffers = createFramebuffers();
	isComplete = true;
}

void FwdRenderer::recreateSwapchainDependents()
{
	// Depthbuffer
	depthBuffer = createDepthBuffer();
	renderPass = createRenderpass();

	// All uniform buffer stuff

	// DescriptorPool

	// Pipeline layouts
	// Pipeline
	shader = createShader();
	pipeline = createPipeline();

	// Framebuffers
	renderFramebuffers = createFramebuffers();
}

void FwdRenderer::update()
{
}

void FwdRenderer::recordCommands(uint32_t frame)
{
	VkRenderPassBeginInfo renderpassBeginInfo = {};
	renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpassBeginInfo.renderPass = renderPass.renderPass.get();
	renderpassBeginInfo.framebuffer = renderFramebuffers[frame];
	renderpassBeginInfo.renderArea.offset = {0, 0};
	renderpassBeginInfo.renderArea.extent = swapchain->get_extent();

	std::array<VkClearValue, 2> clearColor;
	clearColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
	clearColor[1].depthStencil = {1.0f, 0};
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
	pipeline.bind(commandBuffers[frame]);
	cube.bind(commandBuffers[frame]);
	vkCmdDrawIndexed(commandBuffers[frame], cube.get_indexCount(), 1, 0, 0, 0);

	vkCmdEndRenderPass(commandBuffers[frame]);
}

// Custom creation functions
spirv::RenderPass FwdRenderer::createRenderpass()
{
	assert(depthBuffer.valid());
	std::vector<spirv::AttachmentFormat> attachments;

	spirv::AttachmentFormat* attachment;

	attachment = &attachments.emplace_back();
	attachment->format = swapchain->get_format();
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkAttachmentReference colorRef = {};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	attachment = &attachments.emplace_back();
	attachment->format = depthBuffer.get_format();
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkAttachmentReference depthRef = {};
	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorRef;
	subpassDesc.pDepthStencilAttachment = &depthRef;

	spirv::LoadStoreConfig loadStore = {};
	loadStore.colorLoad = spirv::LoadStoreConfig::LoadAction::CLEAR;
	loadStore.colorStore = spirv::LoadStoreConfig::StoreAction::CONTINUE;
	loadStore.depthLoad = spirv::LoadStoreConfig::LoadAction::CLEAR;
	loadStore.depthStore = spirv::LoadStoreConfig::StoreAction::DONT_CARE;

	return pipelineFactory.createRenderPass(attachments, {subpassDesc}, loadStore);
}

spirv::Shader FwdRenderer::createShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile("shaders/PBR/vPBR.vert.spv");
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile("shaders/PBR/fPBR.frag.spv");
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return pipelineFactory.createShader(stages);
}

spirv::Pipeline FwdRenderer::createPipeline()
{
	// TODO BUGFIX!!
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
	info.rasterizerCreateInfo.cullMode = VK_CULL_MODE_NONE;	// TODO VK_CULL_MODEL_BACK
	info.rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	info.rasterizerCreateInfo.depthBiasEnable = VK_TRUE;
	info.rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	info.rasterizerCreateInfo.pNext = nullptr;
	info.rasterizerCreateInfo.flags = 0;

	info.multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	info.multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblendAttachment = {};
	colorblendAttachment.blendEnable = VK_TRUE;
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

	return pipelineFactory.createGraphicsPipeline(shader, renderPass, info);
}

vkw::FramebufferVector FwdRenderer::createFramebuffers() const
{
	using namespace std;

	vector<VkFramebuffer> frameBuffers(maxFrameInFlight);
	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		vector<VkImageView> attachments = {swapchain->get_imageView(i), depthBuffer.get_imageView()};

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass.renderPass.get();
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

Texture2D FwdRenderer::createDepthBuffer() const
{
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

	return Texture2D(*context, imageData);
}
} // namespace blaze