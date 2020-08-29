
#include "DfrRenderer.hpp"

#include <Primitives.hpp>
#include <util/files.hpp>

#include <Version.hpp>
#include <random>

#include <thirdparty/renderdoc/renderdoc.h>

namespace blaze
{

DfrRenderer::DfrRenderer(GLFWwindow* window, bool enableValidationLayers) noexcept
	: ARenderer(window, enableValidationLayers)
{
	glfwSetWindowTitle(window, (std::string(VERSION.FULL_NAME) + " (Deferred)").c_str());

	// Depthbuffer
	depthBuffer = createDepthBuffer();

	// XXX: G-buffer rendering

	mrtAttachment = createMRTAttachment();
	mrtRenderPass = createMRTRenderpass();
	// Pipeline
	mrtShader = createMRTShader();
	mrtPipeline = createMRTPipeline();

	// Attachments
	mrtFramebuffer = createRenderFramebuffer();

	// XXX: SSAO
	// Input

	ssaoNoise = createSSAONoise();
	ssaoKernel = createSSAOKernel();

	ssaoAttachment = createSSAOAttachment();
	ssaoRenderPass = createSSAORenderpass();
	ssaoShader = createSSAOShader();
	ssaoPipeline = createSSAOPipeline();
	ssaoFramebuffer = createSSAOFramebuffer();
	
	ssaoSampleSet = createSSAOSampleSet();
	ssaoDepthSet = createSSAODepthSet();

	ssaoBlurAttachment = createSSAOAttachment();
	ssaoBlurShader = createSSAOBlurShader();
	ssaoBlurPipeline = createSSAOBlurPipeline();
	ssaoBlurFramebuffer = createSSAOBlurFramebuffer();
	ssaoBlurSet = createSSAOBlurSet();

	ssaoFilterRenderPass = createSSAOFilterRenderPass();
	ssaoFilterFramebuffer = createSSAOFilterFramebuffer();
	ssaoFilterShader = createSSAOFilterShader();
	ssaoFilterPipeline = createSSAOFilterPipeline();
	ssaoFilterSet = createSSAOFilterSet();

	// XXX: Lightpass

	// Input

	lightingAttachment = createLightingAttachment();
	lightingRenderPass = createLightingRenderpass();

	// Pipelines
	pointLightShader = createPointLightingShader();
	pointLightPipeline = createPointLightingPipeline();
	dirLightShader = createDirLightingShader();
	dirLightPipeline = createDirLightingPipeline();

	lightVisShader = createLightVisShader();
	lightVisPipeline = createLightVisPipeline();

	lightingFramebuffer = createLightingFramebuffer();
	lightInputSet = createLightingInputSet();

	// XXX: Transparency
	forwardShader = createForwardShader();
	forwardPipeline = createForwardPipeline();

	environmentSet = context->get_pipelineFactory()->createSet(*dirLightShader.getSetWithUniform("skybox"));

	// All uniform buffer stuff
	cameraSets = createCameraSets();
	cameraUBOs = createCameraUBOs();
	settingsUBOs = createSettingsUBOs();

	// Skybox mesh
	// Deferred Quad
	lightVolume = getIcoSphere(context.get());
	lightQuad = getUVRect(context.get());

	// Lights
	lightCaster = std::make_unique<DfrLightCaster>(context.get(), &pointLightShader, maxFrameInFlight);

	// Post process
	postProcessRenderPass = createPostProcessRenderPass();
	postProcessFramebuffers = createPostProcessFramebuffers();

	bloom = Bloom(context.get(), &lightingAttachment);
	hdrTonemap = HDRTonemap(context.get(), &postProcessRenderPass, &lightingAttachment);

	isComplete = true;
}

void DfrRenderer::recreateSwapchainDependents()
{
	// Depthbuffer
	depthBuffer = createDepthBuffer();
	// renderpass same since numSwapchain independent

	// All uniform buffer stuff
	cameraSets = createCameraSets();
	cameraUBOs = createCameraUBOs();
	settingsUBOs = createSettingsUBOs();

	// Lights
	lightCaster->recreate(context.get(), &pointLightShader, maxFrameInFlight);

	// G-buffer
	mrtAttachment = createMRTAttachment();
	lightInputSet = createLightingInputSet();

	// SSAO

	ssaoAttachment = createSSAOAttachment();
	ssaoFramebuffer = createSSAOFramebuffer();

	ssaoSampleSet = createSSAOSampleSet();
	ssaoDepthSet = createSSAODepthSet();

	ssaoBlurAttachment = createSSAOAttachment();
	ssaoBlurFramebuffer = createSSAOBlurFramebuffer();
	ssaoBlurSet = createSSAOBlurSet();

	ssaoFilterFramebuffer = createSSAOFilterFramebuffer();
	ssaoFilterSet = createSSAOFilterSet();

	// Lighting
	lightingAttachment = createLightingAttachment();

	// Framebuffers
	mrtFramebuffer = createRenderFramebuffer();
	lightingFramebuffer = createLightingFramebuffer();

	// Post process
	postProcessRenderPass = createPostProcessRenderPass(); // dep on swapchain
	postProcessFramebuffers = createPostProcessFramebuffers();

	bloom.recreate(context.get(), &lightingAttachment);
	hdrTonemap.recreate(context.get(), &postProcessRenderPass, &lightingAttachment);
}

spirv::RenderPass DfrRenderer::createMRTRenderpass()
{
	assert(depthBuffer.valid());

	using namespace spirv;
	std::vector<AttachmentFormat> attachments;

	AttachmentFormat* attachment;

	std::vector<VkAttachmentReference> colorRefs;
	VkAttachmentReference depthRef = {};

	uint32_t attachmentRefIdx = 0;

	// G-buffer attachments
	// POSITION
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// NORMAL
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// ALBEDO
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R8G8B8A8_UNORM;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// OCCLUSION METAL ROUGH
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R8G8B8A8_UNORM;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// EMISSION
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R8G8B8A8_UNORM;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);
	colorRefs.emplace_back() = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// Depth Attachment
	attachment = &attachments.emplace_back();
	attachment->format = depthBuffer.get_format();
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);
	depthRef = {attachmentRefIdx++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	std::vector<VkSubpassDependency> deps = {};
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
		dependency->dstSubpass = VK_SUBPASS_EXTERNAL;
		dependency->srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency->dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependency->srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency->dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	}

	std::vector<VkSubpassDescription> subpassDescs = {};
	{
		VkSubpassDescription* subpassDesc;
		subpassDesc = &subpassDescs.emplace_back();
		subpassDesc->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc->colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
		subpassDesc->pColorAttachments = colorRefs.data();
		subpassDesc->pDepthStencilAttachment = &depthRef;
	}

	std::vector<VkClearValue> clearColor(6);
	for (auto& clCol : clearColor)
	{
		clCol.color = {0.0f, 0.0f, 0.0f, 0.0f};
	}
	clearColor.back().depthStencil = {1.0f, 0};

	auto rp = context->get_pipelineFactory()->createRenderPass(attachments, subpassDescs, deps);
	rp.clearValues = std::move(clearColor);
	return std::move(rp);
}

spirv::RenderPass DfrRenderer::createLightingRenderpass()
{
	assert(depthBuffer.valid());

	using namespace spirv;
	std::vector<AttachmentFormat> attachments;

	AttachmentFormat* attachment;

	VkAttachmentReference outputColorRef = {};
	VkAttachmentReference depthRef = {};

	std::vector<VkAttachmentReference> inputRefs;

	uint32_t attachmentRefIdx = 0;

	// Color attachment
	attachment = &attachments.emplace_back();
	attachment->format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);
	outputColorRef = {attachmentRefIdx++, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	// Depth Attachment
	attachment = &attachments.emplace_back();
	attachment->format = depthBuffer.get_format();
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::READ, LoadStoreConfig::StoreAction::CONTINUE);
	depthRef = {attachmentRefIdx++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	std::vector<VkSubpassDescription> subpassDescs = {};
	{
		VkSubpassDescription* subpassDesc;
		subpassDesc = &subpassDescs.emplace_back();
		subpassDesc->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc->colorAttachmentCount = 1;
		subpassDesc->pColorAttachments = &outputColorRef;
		subpassDesc->pDepthStencilAttachment = &depthRef;
	}

	std::vector<VkClearValue> lightingClear(2);
	lightingClear[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
	lightingClear[1].depthStencil = {1.0f, 0};
	auto rp = context->get_pipelineFactory()->createRenderPass(attachments, subpassDescs);
	rp.clearValues = std::move(lightingClear);

	return std::move(rp);
}

Texture2D DfrRenderer::createDepthBuffer() const
{
	assert(swapchain);
	VkFormat format =
		util::findSupportedFormat(context->get_physicalDevice(),
								  {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT},
								  VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	ImageData2D imageData = {};
	imageData.format = format;
	imageData.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	imageData.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageData.height = swapchain->get_extent().height;
	imageData.width = swapchain->get_extent().width;
	imageData.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imageData.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageData.numChannels = 1;
	imageData.size = swapchain->get_extent().width * swapchain->get_extent().height;

	return Texture2D(context.get(), imageData);
}

Texture2D DfrRenderer::createLightingAttachment() const
{
	assert(swapchain);

	ImageData2D imageData = {};
	imageData.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	imageData.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	imageData.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	imageData.height = swapchain->get_extent().height;
	imageData.width = swapchain->get_extent().width;
	imageData.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageData.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageData.numChannels = 4;
	imageData.size = swapchain->get_extent().width * swapchain->get_extent().height;

	return Texture2D(context.get(), imageData);
}

spirv::RenderPass DfrRenderer::createPostProcessRenderPass()
{
	assert(depthBuffer.valid());

	using namespace spirv;
	std::vector<AttachmentFormat> attachments;

	AttachmentFormat* attachment;

	VkAttachmentReference swapchainColorRef = {};

	// Swapchain attachment
	attachment = &attachments.emplace_back();
	attachment->format = swapchain->get_format();
	attachment->sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachment->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	attachment->loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::CONTINUE);
	swapchainColorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	std::vector<VkSubpassDescription> subpassDescs = {};
	{
		VkSubpassDescription* subpassDesc;
		subpassDesc = &subpassDescs.emplace_back();
		subpassDesc->pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc->colorAttachmentCount = 1;
		subpassDesc->pColorAttachments = &swapchainColorRef;
		subpassDesc->pDepthStencilAttachment = nullptr;
	}

	VkClearValue clear = {};
	clear.color = {0.0f, 0.0f, 0.0f, 1.0f};

	auto rp = context->get_pipelineFactory()->createRenderPass(attachments, subpassDescs);
	rp.clearValues = {clear};
	return rp;
}

std::vector<spirv::Framebuffer> DfrRenderer::createPostProcessFramebuffers()
{
	using namespace std;
	assert(depthBuffer.valid());
	assert(postProcessRenderPass.valid());

	vector<spirv::Framebuffer> frameBuffers;
	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		vector<VkImageView> attachments = {
			swapchain->get_imageView(i),
		};

		frameBuffers.push_back(get_pipelineFactory()->createFramebuffer(postProcessRenderPass, swapchain->get_extent(), attachments));
	}

	return std::move(frameBuffers);
}

spirv::SetSingleton* DfrRenderer::get_environmentSet()
{
	return &environmentSet;
}

void DfrRenderer::update(uint32_t frame)
{
	cameraUBOs[frame].write(camera->getUbo());
	settingsUBOs[frame].write(settings);
	lightCaster->update(camera, frame);
}

void DfrRenderer::recordCommands(uint32_t frame)
{
	lightCaster->cast(commandBuffers[frame], drawables.get_data());

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

	mrtRenderPass.begin(commandBuffers[frame], mrtFramebuffer);

	vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);
	vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

	// Do the systemic thing
	mrtPipeline.bind(commandBuffers[frame]);
	vkCmdBindDescriptorSets(commandBuffers[frame], mrtPipeline.bindPoint, mrtShader.pipelineLayout.get(),
							cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
	for (Drawable* drawable : drawables)
	{
		drawable->drawOpaque(commandBuffers[frame], mrtShader.pipelineLayout.get());
	}

	mrtRenderPass.end(commandBuffers[frame]);

	if (ssaoEnabled)
	{
		ssaoRenderPass.begin(commandBuffers[frame], ssaoFramebuffer);

		VkExtent2D ssaoExtent = ssaoFramebuffer.renderArea.extent;

		VkViewport ssaoViewport = {};
		ssaoViewport.minDepth = 0.0f;
		ssaoViewport.maxDepth = 1.0f;
		ssaoViewport.x = 0;
		ssaoViewport.y = static_cast<float>(ssaoExtent.height);
		ssaoViewport.width = static_cast<float>(ssaoExtent.width);
		ssaoViewport.height = -static_cast<float>(ssaoExtent.height);

		vkCmdSetScissor(commandBuffers[frame], 0, 1, &ssaoFramebuffer.renderArea);
		vkCmdSetViewport(commandBuffers[frame], 0, 1, &ssaoViewport);

		ssaoPipeline.bind(commandBuffers[frame]);
		vkCmdBindDescriptorSets(commandBuffers[frame], ssaoPipeline.bindPoint, ssaoShader.pipelineLayout.get(),
								cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffers[frame], ssaoPipeline.bindPoint, ssaoShader.pipelineLayout.get(), ssaoDepthSet.setIdx, 1, &ssaoDepthSet.get(), 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffers[frame], ssaoPipeline.bindPoint, ssaoShader.pipelineLayout.get(),
								ssaoSampleSet.setIdx, 1, &ssaoSampleSet.get(), 0, nullptr);
		vkCmdPushConstants(commandBuffers[frame], ssaoShader.pipelineLayout.get(), ssaoShader.pushConstant.stage, 0,
							ssaoShader.pushConstant.size, &ssaoSettings);
		lightQuad.bind(commandBuffers[frame]);
		vkCmdDrawIndexed(commandBuffers[frame], lightQuad.get_indexCount(), 1, 0, 0, 0);

		ssaoRenderPass.end(commandBuffers[frame]);

		if (ssaoBlurEnable)
		{
			renderdoc::startCapture();
			for (int i = 0; i < ssaoBlurCount; i++)
			{
				ssaoRenderPass.begin(commandBuffers[frame], ssaoBlurFramebuffer);

				vkCmdSetScissor(commandBuffers[frame], 0, 1, &ssaoBlurFramebuffer.renderArea);
				vkCmdSetViewport(commandBuffers[frame], 0, 1, &ssaoViewport);

				ssaoBlurSettings.verticalPass = 0;
				ssaoBlurPipeline.bind(commandBuffers[frame]);
				vkCmdBindDescriptorSets(commandBuffers[frame], ssaoBlurPipeline.bindPoint,
										ssaoBlurShader.pipelineLayout.get(), cameraSets.setIdx, 1, &cameraSets[frame], 0,
										nullptr);
				vkCmdBindDescriptorSets(commandBuffers[frame], ssaoBlurPipeline.bindPoint,
										ssaoBlurShader.pipelineLayout.get(), ssaoBlurSet.setIdx, 1, &ssaoBlurSet.get(), 0,
										nullptr);
				vkCmdPushConstants(commandBuffers[frame], ssaoBlurShader.pipelineLayout.get(),
									ssaoBlurShader.pushConstant.stage, 0, ssaoBlurShader.pushConstant.size,
									&ssaoBlurSettings);
				lightQuad.bind(commandBuffers[frame]);
				vkCmdDrawIndexed(commandBuffers[frame], lightQuad.get_indexCount(), 1, 0, 0, 0);

				ssaoRenderPass.end(commandBuffers[frame]);

				ssaoRenderPass.begin(commandBuffers[frame], ssaoFramebuffer);

				vkCmdSetScissor(commandBuffers[frame], 0, 1, &ssaoFramebuffer.renderArea);
				vkCmdSetViewport(commandBuffers[frame], 0, 1, &ssaoViewport);

				ssaoBlurSettings.verticalPass = 1;
				ssaoBlurPipeline.bind(commandBuffers[frame]);
				vkCmdBindDescriptorSets(commandBuffers[frame], ssaoBlurPipeline.bindPoint,
										ssaoBlurShader.pipelineLayout.get(), cameraSets.setIdx, 1, &cameraSets[frame], 0,
										nullptr);
				vkCmdBindDescriptorSets(commandBuffers[frame], ssaoBlurPipeline.bindPoint,
										ssaoBlurShader.pipelineLayout.get(), ssaoFilterSet.setIdx, 1, &ssaoFilterSet.get(),
										0, nullptr);
				vkCmdPushConstants(commandBuffers[frame], ssaoBlurShader.pipelineLayout.get(),
									ssaoBlurShader.pushConstant.stage, 0, ssaoBlurShader.pushConstant.size,
									&ssaoBlurSettings);
				lightQuad.bind(commandBuffers[frame]);
				vkCmdDrawIndexed(commandBuffers[frame], lightQuad.get_indexCount(), 1, 0, 0, 0);

				ssaoRenderPass.end(commandBuffers[frame]);
			}
			renderdoc::endCapture();
		}

		ssaoFilterRenderPass.begin(commandBuffers[frame], ssaoFilterFramebuffer);

		vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);
		vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

		ssaoFilterPipeline.bind(commandBuffers[frame]);
		vkCmdBindDescriptorSets(commandBuffers[frame], ssaoFilterPipeline.bindPoint,
								ssaoFilterShader.pipelineLayout.get(),
								cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffers[frame], ssaoFilterPipeline.bindPoint,
								ssaoFilterShader.pipelineLayout.get(), ssaoBlurSet.setIdx, 1, &ssaoBlurSet.get(), 0,
								nullptr);
		/*vkCmdPushConstants(commandBuffers[frame], ssaoFilterShader.pipelineLayout.get(),
							ssaoFilterShader.pushConstant.stage, 0, ssaoFilterShader.pushConstant.size,
							&ssaoBlurSettings);*/
		lightQuad.bind(commandBuffers[frame]);
		vkCmdDrawIndexed(commandBuffers[frame], lightQuad.get_indexCount(), 1, 0, 0, 0);

		ssaoFilterRenderPass.end(commandBuffers[frame]);
	}

	lightingRenderPass.begin(commandBuffers[frame], lightingFramebuffer);

	vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);
	vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

	// Point lights first
	// Only if we are running lights. Else skip.
	if (settings.viewRT == Settings::RENDER)
	{
		pointLightPipeline.bind(commandBuffers[frame]);
		lightCaster->bind(commandBuffers[frame], pointLightShader.pipelineLayout.get(), frame);
		vkCmdBindDescriptorSets(commandBuffers[frame], pointLightPipeline.bindPoint, pointLightShader.pipelineLayout.get(),
								cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffers[frame], pointLightPipeline.bindPoint, pointLightShader.pipelineLayout.get(),
								lightInputSet.setIdx, 1, &lightInputSet.get(), 0, nullptr);
		lightVolume.bind(commandBuffers[frame]);
		for (auto it = lightCaster->getPointLightIterator(); it.valid(); ++it)
		{
			glm::ivec4 idx(it.index);
			vkCmdPushConstants(commandBuffers[frame], pointLightShader.pipelineLayout.get(),
							   pointLightShader.pushConstant.stage, 0, pointLightShader.pushConstant.size, &idx[0]);

			vkCmdDrawIndexed(commandBuffers[frame], lightVolume.get_indexCount(), 1, 0, 0, 0);
		}
	}

	// Direction lights, environment, ambient and debug
	dirLightPipeline.bind(commandBuffers[frame]);
	lightCaster->bind(commandBuffers[frame], dirLightShader.pipelineLayout.get(), frame);
	vkCmdBindDescriptorSets(commandBuffers[frame], dirLightPipeline.bindPoint, dirLightShader.pipelineLayout.get(),
							cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffers[frame], dirLightPipeline.bindPoint, dirLightShader.pipelineLayout.get(),
							lightInputSet.setIdx, 1, &lightInputSet.get(), 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffers[frame], dirLightPipeline.bindPoint, dirLightShader.pipelineLayout.get(),
							environmentSet.setIdx, 1, &environmentSet.get(), 0, nullptr);
	lightVolume.bind(commandBuffers[frame]);
	vkCmdDrawIndexed(commandBuffers[frame], lightVolume.get_indexCount(), 1, 0, 0, 0);

	// Transparency
	forwardPipeline.bind(commandBuffers[frame]);
	lightCaster->bind(commandBuffers[frame], forwardShader.pipelineLayout.get(), frame);
	vkCmdBindDescriptorSets(commandBuffers[frame], forwardPipeline.bindPoint, forwardShader.pipelineLayout.get(),
							cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
	vkCmdBindDescriptorSets(commandBuffers[frame], forwardPipeline.bindPoint, forwardShader.pipelineLayout.get(),
							environmentSet.setIdx, 1, &environmentSet.get(), 0, nullptr);
	for (Drawable* drawable : drawables)
	{
		drawable->drawAlphaBlended(commandBuffers[frame], forwardShader.pipelineLayout.get());
	}

	// Lights vis
	lightVisPipeline.bind(commandBuffers[frame]);
	vkCmdBindDescriptorSets(commandBuffers[frame], lightVisPipeline.bindPoint,
							lightVisShader.pipelineLayout.get(), cameraSets.setIdx, 1, &cameraSets[frame], 0,
							nullptr);
	lightVolume.bind(commandBuffers[frame]);
	for (auto it = lightCaster->getPointLightIterator(); it.valid(); ++it)
	{
		glm::mat4 pos = glm::scale(glm::translate(glm::mat4(1.0f), it.data->position), glm::vec3(0.1f));
		vkCmdPushConstants(commandBuffers[frame], lightVisShader.pipelineLayout.get(),
						   lightVisShader.pushConstant.stage, 0, sizeof(glm::mat4), &pos);
		vkCmdPushConstants(commandBuffers[frame], lightVisShader.pipelineLayout.get(),
						   lightVisShader.pushConstant.stage, sizeof(glm::mat4), sizeof(glm::vec4), &it.data->color);
		vkCmdDrawIndexed(commandBuffers[frame], lightVolume.get_indexCount(), 1, 0, 0, 0);
	}

	lightingRenderPass.end(commandBuffers[frame]);

	// Post process

	if (bloomEnable)
	{
		bloom.process(commandBuffers[frame], lightQuad);
	}
	
	postProcessRenderPass.begin(commandBuffers[frame], postProcessFramebuffers[frame]);

	vkCmdSetScissor(commandBuffers[frame], 0, 1, &scissor);
	vkCmdSetViewport(commandBuffers[frame], 0, 1, &viewport);

	hdrTonemap.process(commandBuffers[frame], lightQuad);

	postProcessRenderPass.end(commandBuffers[frame]);
}

const spirv::Shader* DfrRenderer::get_shader() const
{
	return &mrtShader;
}

void DfrRenderer::drawSettings()
{
	if (ImGui::Begin("Deferred Settings"))
	{
		ImGui::DragFloat("Exposure", &hdrTonemap.pushConstant.exposure, 0.1f, 1.0f, 10.0f);
		ImGui::DragFloat("Gamma", &hdrTonemap.pushConstant.gamma, 0.1f, 1.0f, 4.0f);
		ImGui::Checkbox("Enable IBL", (bool*)&settings.enableIBL);
		ImGui::Checkbox("Enable SSAO", &ssaoEnabled);
		if (ssaoEnabled)
		{
			if (ImGui::CollapsingHeader("SSAO Options")) 
			{
				ImGui::DragFloat("Kernel Radius##SSAO", &ssaoSettings.kernelRadius, 0.01f, 0.01f, 1.0f);
				ImGui::DragFloat("Bias##SSAO", &ssaoSettings.bias, 0.01f, 0.01f, 1.0f);
				ImGui::Checkbox("Blur Enabled##SSAO", &ssaoBlurEnable);
				if (ssaoBlurEnable)
				{
					ImGui::DragInt("Blur Iterations##SSAO", &ssaoBlurCount, 0.2f, 1, 5);
					ImGui::Checkbox("Bilateral Blur##SSAO", (bool*)&ssaoBlurSettings.depthAware);
					ImGui::DragFloat("Bilateral Threshold##SSAO", &ssaoBlurSettings.depth, 0.001f, 0.001f, 0.1f);
				}
			}
		}
		ImGui::Checkbox("Enable Bloom", &bloomEnable);
		if (bloomEnable) {
			if (ImGui::CollapsingHeader("Bloom Options##Bloom"))
			{
				ImGui::DragInt("Blur Iterations##Bloom", &bloom.iterations, 0.5f, 1, 10);
				ImGui::DragFloat("Highpass Threshold##Bloom", &bloom.highpassSettings.threshold, 0.1f, 0.1f, 5.0f);
				ImGui::DragFloat("Highpass Tolerance##Bloom", &bloom.highpassSettings.tolerance, 0.01f, 0.0f, 0.5f);
			}
		}

		if (ImGui::CollapsingHeader("MRT Debug Output"))
		{
			if (ImGui::RadioButton("Full Render", settings.viewRT == settings.RENDER))
			{
				settings.viewRT = settings.RENDER;
				hdrTonemap.pushConstant.enable = 1.0f;
			}
			if (ImGui::RadioButton("MRT Position", settings.viewRT == settings.POSITION))
			{
				settings.viewRT = settings.POSITION;
				hdrTonemap.pushConstant.enable = 0.0f;
			}
			if (ImGui::RadioButton("MRT Normal", settings.viewRT == settings.NORMAL))
			{
				settings.viewRT = settings.NORMAL;
				hdrTonemap.pushConstant.enable = 0.0f;
			}
			if (ImGui::RadioButton("MRT Albedo", settings.viewRT == settings.ALBEDO))
			{
				settings.viewRT = settings.ALBEDO;
				hdrTonemap.pushConstant.enable = 0.0f;
			}
			if (ImGui::RadioButton("MRT AO", settings.viewRT == settings.AO))
			{
				settings.viewRT = settings.AO;
				hdrTonemap.pushConstant.enable = 0.0f;
			}
			if (ImGui::RadioButton("MRT METALLIC", settings.viewRT == settings.METALLIC))
			{
				settings.viewRT = settings.METALLIC;
				hdrTonemap.pushConstant.enable = 0.0f;
			}
			if (ImGui::RadioButton("MRT ROUGHNESS", settings.viewRT == settings.ROUGHNESS))
			{
				settings.viewRT = settings.ROUGHNESS;
				hdrTonemap.pushConstant.enable = 0.0f;
			}
			if (ImGui::RadioButton("MRT EMISSION", settings.viewRT == settings.EMISSION))
			{
				settings.viewRT = settings.EMISSION;
				hdrTonemap.pushConstant.enable = 1.0f;
			}
			if (ImGui::RadioButton("MRT IBL", settings.viewRT == settings.IBL))
			{
				settings.viewRT = settings.IBL;
				hdrTonemap.pushConstant.enable = 1.0f;
			}
		}
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

	return context->get_pipelineFactory()->createGraphicsPipeline(mrtShader, mrtRenderPass, info);
}

DfrRenderer::MRTAttachment DfrRenderer::createMRTAttachment()
{
	MRTAttachment attachment;

	auto extent = swapchain->get_extent();

	ImageData2D id2d = {};
	id2d.height = extent.height;
	id2d.width = extent.width;
	id2d.numChannels = 4;
	id2d.anisotropy = VK_FALSE;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	id2d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	id2d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	id2d.access = VK_ACCESS_SHADER_READ_BIT;

	id2d.format = VK_FORMAT_R8G8B8A8_UNORM;
	attachment.albedo = Texture2D(context.get(), id2d, false);
	attachment.emission = Texture2D(context.get(), id2d, false);
	attachment.omr = Texture2D(context.get(), id2d, false);

	id2d.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	attachment.position = Texture2D(context.get(), id2d, false);
	attachment.normal = Texture2D(context.get(), id2d, false);

	return attachment;
}

spirv::SetSingleton DfrRenderer::createLightingInputSet()
{
	assert(mrtAttachment.valid());
	assert(pointLightShader.valid());
	assert(dirLightShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*pointLightShader.getSetWithUniform("I_POSITION"));

	std::vector<const spirv::UniformInfo*> unifs = {
		set.getUniform("I_POSITION"), set.getUniform("I_NORMAL"),	set.getUniform("I_ALBEDO"),
		set.getUniform("I_OMR"),	  set.getUniform("I_EMISSION"),
	};
	std::vector<VkDescriptorImageInfo> infos = {
		mrtAttachment.position.get_imageInfo(), mrtAttachment.normal.get_imageInfo(),
		mrtAttachment.albedo.get_imageInfo(),	mrtAttachment.omr.get_imageInfo(),
		mrtAttachment.emission.get_imageInfo(),
	};
	std::vector<VkWriteDescriptorSet> writes(infos.size());

	for (int i = 0; i < infos.size(); ++i)
	{
		const spirv::UniformInfo* unif = unifs[i];
		VkDescriptorImageInfo* info = &infos[i];

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

Texture2D DfrRenderer::createSSAONoise()
{
	std::uniform_real_distribution<float> randomFloats(-1.0, 1.0); // random floats between [0.0, 1.0]
	std::default_random_engine generator;

	uint32_t randomDirArray[16];
	for (int i = 0; i < 16; i++)
	{
		randomDirArray[i] = glm::packHalf2x16(glm::vec2(randomFloats(generator), randomFloats(generator)));
	}

	ImageData2D id2d = {};
	id2d.data = (uint8_t*)randomDirArray;
	id2d.anisotropy = VK_FALSE;
	id2d.access = VK_ACCESS_SHADER_READ_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	id2d.numChannels = 2;
	id2d.format = VK_FORMAT_R16G16_SFLOAT;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	id2d.height = 4;
	id2d.width = 4;
	id2d.size = sizeof(uint32_t) * 16;
	id2d.layerCount = 1;
	id2d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;

	return Texture2D(context.get(), id2d, false);
}

BaseUBO DfrRenderer::createSSAOKernel()
{
	std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
	std::default_random_engine generator;

	glm::vec4 data[64];
	for (int i = 0; i < 64; i++)
	{
		float f;
		f = 2.0f * randomFloats(generator) - 1.0f;
		float x = f / glm::cos(f);
		f = 2.0f * randomFloats(generator) - 1.0f;
		float y = f / glm::cos(f);
		f = 2.0f * randomFloats(generator) - 1.0f;
		float z = f / glm::cos(f);
		data[i] = glm::normalize(glm::vec4(x, y, 0.5f + 0.5f * z, 0.0f)) * randomFloats(generator);
	}

	auto ubo = BaseUBO(context.get(), sizeof(data));
	ubo.writeData(data, sizeof(data));
	return std::move(ubo);
}

spirv::SetSingleton DfrRenderer::createSSAOSampleSet()
{
	assert(ssaoNoise.valid());
	assert(ssaoShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*ssaoShader.getSetWithUniform("kernel"));

	std::vector<VkWriteDescriptorSet> writes(2);

	const spirv::UniformInfo* unif = ssaoShader.getUniform("kernel");
	VkDescriptorBufferInfo* bufferInfo = &ssaoKernel.get_descriptorInfo();

	VkWriteDescriptorSet* write = &writes[0];
	write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write->descriptorType = unif->type;
	write->descriptorCount = unif->arrayLength;
	write->dstSet = set.get();
	write->dstBinding = unif->binding;
	write->dstArrayElement = 0;
	write->pBufferInfo = bufferInfo;

	unif = ssaoShader.getUniform("noise");
	const VkDescriptorImageInfo* imageInfo = &ssaoNoise.get_imageInfo();

	write = &writes[1];
	write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write->descriptorType = unif->type;
	write->descriptorCount = unif->arrayLength;
	write->dstSet = set.get();
	write->dstBinding = unif->binding;
	write->dstArrayElement = 0;
	write->pImageInfo = imageInfo;

	vkUpdateDescriptorSets(context->get_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	return std::move(set);
}

Texture2D DfrRenderer::createSSAOAttachment()
{

	auto extent = swapchain->get_extent();

	ImageData2D id2d = {};
	id2d.height = extent.height / 2;
	id2d.width = extent.width / 2;
	id2d.numChannels = 4;
	id2d.anisotropy = VK_FALSE;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	id2d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	id2d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	id2d.access = VK_ACCESS_SHADER_READ_BIT;

	id2d.format = VK_FORMAT_R8_UNORM;

	return Texture2D(context.get(), id2d, false);
}

spirv::RenderPass DfrRenderer::createSSAORenderpass()
{
	assert(ssaoAttachment.valid());

	using namespace spirv;

	VkAttachmentReference colorRef;

	std::vector<AttachmentFormat> attachments(1);
	attachments[0].format = ssaoAttachment.get_format();
	attachments[0].sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachments[0].loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);
	colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorRef;
	subpassDesc.pDepthStencilAttachment = nullptr;

	VkClearValue ssaoClear;
	ssaoClear.color = {1.0f, 0.0f, 0.0f, 0.0f};

	auto rp = get_pipelineFactory()->createRenderPass(attachments, {subpassDesc});
	rp.clearValues = {ssaoClear};
	return std::move(rp);
}

spirv::Shader DfrRenderer::createSSAOShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vSSAOShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fSSAOShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline DfrRenderer::createSSAOPipeline()
{
	assert(ssaoRenderPass.valid());
	assert(ssaoShader.valid());

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
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
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

	return context->get_pipelineFactory()->createGraphicsPipeline(ssaoShader, ssaoRenderPass, info);
}

spirv::Framebuffer DfrRenderer::createSSAOFramebuffer()
{
	using namespace std;
	assert(ssaoAttachment.valid());
	assert(ssaoRenderPass.valid());

	vector<VkImageView> attachments = {
		ssaoAttachment.get_imageView(),
	};

	return get_pipelineFactory()->createFramebuffer(
		ssaoRenderPass, {ssaoAttachment.get_width(), ssaoAttachment.get_height()}, attachments);
}

spirv::SetSingleton DfrRenderer::createSSAODepthSet()
{
	assert(ssaoShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*ssaoShader.getSetWithUniform("I_NORMAL"));

	std::vector<const spirv::UniformInfo*> unifs = {
		set.getUniform("I_NORMAL"),
		set.getUniform("I_POSITION"),
	};
	std::vector<VkDescriptorImageInfo> infos = {
		mrtAttachment.normal.get_imageInfo(),
		mrtAttachment.position.get_imageInfo(),
	};
	std::vector<VkWriteDescriptorSet> writes(infos.size());

	for (int i = 0; i < infos.size(); ++i)
	{
		const spirv::UniformInfo* unif = unifs[i];
		VkDescriptorImageInfo* info = &infos[i];

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

spirv::Framebuffer DfrRenderer::createSSAOBlurFramebuffer()
{
	using namespace std;
	assert(ssaoBlurAttachment.valid());
	assert(ssaoRenderPass.valid());

	vector<VkImageView> attachments = {
		ssaoBlurAttachment.get_imageView(),
	};

	return get_pipelineFactory()->createFramebuffer(ssaoRenderPass, ssaoBlurAttachment.get_extent(), attachments);
}

spirv::Shader DfrRenderer::createSSAOBlurShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vSSAOBlurShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fSSAOBlurShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline DfrRenderer::createSSAOBlurPipeline()
{
	assert(ssaoRenderPass.valid());
	assert(ssaoBlurShader.valid());

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
	colorblendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
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

	return context->get_pipelineFactory()->createGraphicsPipeline(ssaoBlurShader, ssaoRenderPass, info);
}

spirv::SetSingleton DfrRenderer::createSSAOBlurSet()
{
	assert(mrtAttachment.valid());
	assert(ssaoBlurShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*ssaoBlurShader.getSetWithUniform("I_SSAO"));

	std::vector<const spirv::UniformInfo*> unifs = {
		set.getUniform("I_SSAO"),
		set.getUniform("I_POSITION"),
	};
	std::vector<VkDescriptorImageInfo> infos = {
		ssaoAttachment.get_imageInfo(),
		mrtAttachment.position.get_imageInfo(),
	};
	std::vector<VkWriteDescriptorSet> writes(infos.size());

	for (int i = 0; i < infos.size(); ++i)
	{
		const spirv::UniformInfo* unif = unifs[i];
		VkDescriptorImageInfo* info = &infos[i];

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

spirv::RenderPass DfrRenderer::createSSAOFilterRenderPass()
{
	assert(mrtAttachment.valid());

	using namespace spirv;

	VkAttachmentReference colorRef;

	std::vector<AttachmentFormat> attachments(1);
	attachments[0].format = mrtAttachment.omr.get_format();
	attachments[0].sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	attachments[0].loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::READ, LoadStoreConfig::StoreAction::READ);
	colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorRef;
	subpassDesc.pDepthStencilAttachment = nullptr;

	VkClearValue ssaoClear;
	ssaoClear.color = {1.0f, 0.0f, 0.0f, 0.0f};

	auto rp = get_pipelineFactory()->createRenderPass(attachments, {subpassDesc});
	rp.clearValues = {ssaoClear};
	return std::move(rp);
}

spirv::Framebuffer DfrRenderer::createSSAOFilterFramebuffer()
{
	using namespace std;
	assert(mrtAttachment.valid());
	assert(ssaoFilterRenderPass.valid());

	vector<VkImageView> attachments = {
		mrtAttachment.omr.get_imageView(),
	};

	return get_pipelineFactory()->createFramebuffer(ssaoFilterRenderPass, swapchain->get_extent(), attachments);
}

spirv::Shader DfrRenderer::createSSAOFilterShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vSSAOFilterShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fSSAOFilterShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline DfrRenderer::createSSAOFilterPipeline()
{
	assert(ssaoFilterRenderPass.valid());
	assert(ssaoFilterShader.valid());

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
	info.rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
	info.rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	info.rasterizerCreateInfo.pNext = nullptr;
	info.rasterizerCreateInfo.flags = 0;

	info.multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	info.multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblendAttachment = {};
	colorblendAttachment.colorWriteMask = 0;
	colorblendAttachment.blendEnable = VK_FALSE;
	colorblendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
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

	info.depthStencilCreateInfo.front = {};
	info.depthStencilCreateInfo.back = {};

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	info.dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	info.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	info.dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	return context->get_pipelineFactory()->createGraphicsPipeline(ssaoFilterShader, ssaoFilterRenderPass, info);
}

spirv::SetSingleton DfrRenderer::createSSAOFilterSet()
{
	assert(mrtAttachment.valid());
	assert(ssaoFilterShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*ssaoFilterShader.getSetWithUniform("I_SSAO"));

	std::vector<const spirv::UniformInfo*> unifs = {
		set.getUniform("I_SSAO"),
		set.getUniform("I_POSITION"),
	};
	std::vector<VkDescriptorImageInfo> infos = {
		ssaoBlurAttachment.get_imageInfo(),
		mrtAttachment.position.get_imageInfo(),
	};
	std::vector<VkWriteDescriptorSet> writes(infos.size());

	for (int i = 0; i < infos.size(); ++i)
	{
		const spirv::UniformInfo* unif = unifs[i];
		VkDescriptorImageInfo* info = &infos[i];

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

spirv::Shader DfrRenderer::createPointLightingShader()
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

spirv::Pipeline DfrRenderer::createPointLightingPipeline()
{
	assert(lightingRenderPass.valid());
	assert(pointLightShader.valid());

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
	info.rasterizerCreateInfo.depthClampEnable = VK_TRUE;	// TODO: Verify working
	info.rasterizerCreateInfo.pNext = nullptr;
	info.rasterizerCreateInfo.flags = 0;

	info.multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	info.multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblendAttachment = {};
	colorblendAttachment.colorWriteMask = 0;
	colorblendAttachment.blendEnable = VK_TRUE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 1;
	info.colorblendCreateInfo.pAttachments = &colorblendAttachment;

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_TRUE;
	info.depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;
	info.depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.maxDepthBounds = 0.0f; // Don't care
	info.depthStencilCreateInfo.minDepthBounds = 1.0f; // Don't care
	info.depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

	info.depthStencilCreateInfo.front = {};
	info.depthStencilCreateInfo.back = {};

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	info.dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	info.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	info.dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	return context->get_pipelineFactory()->createGraphicsPipeline(pointLightShader, lightingRenderPass, info);
}

spirv::Shader DfrRenderer::createDirLightingShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vDirLightingShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fDirLightingShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline DfrRenderer::createDirLightingPipeline()
{
	assert(lightingRenderPass.valid());
	assert(dirLightShader.valid());

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
	colorblendAttachment.colorWriteMask = 0;
	colorblendAttachment.blendEnable = VK_TRUE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 1;
	info.colorblendCreateInfo.pAttachments = &colorblendAttachment;

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_TRUE;
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

	return context->get_pipelineFactory()->createGraphicsPipeline(dirLightShader, lightingRenderPass, info);
}

spirv::Shader DfrRenderer::createLightVisShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vLightVisShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fLightVisShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline DfrRenderer::createLightVisPipeline()
{
	assert(lightingRenderPass.valid());
	assert(lightVisShader.valid());

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
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 1;
	info.colorblendCreateInfo.pAttachments = &colorblendAttachment;

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_TRUE;
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

	return context->get_pipelineFactory()->createGraphicsPipeline(lightVisShader, lightingRenderPass, info);
}

spirv::Framebuffer DfrRenderer::createRenderFramebuffer()
{
	using namespace std;
	assert(depthBuffer.valid());
	assert(mrtAttachment.valid());
	assert(mrtRenderPass.valid());

	vector<VkImageView> attachments = {
		mrtAttachment.position.get_imageView(), mrtAttachment.normal.get_imageView(),
		mrtAttachment.albedo.get_imageView(),	mrtAttachment.omr.get_imageView(),
		mrtAttachment.emission.get_imageView(), depthBuffer.get_imageView(),
	};
	return get_pipelineFactory()->createFramebuffer(mrtRenderPass, swapchain->get_extent(), attachments);
}

spirv::Framebuffer DfrRenderer::createLightingFramebuffer()
{
	using namespace std;
	assert(depthBuffer.valid());
	assert(lightingAttachment.valid());
	assert(lightingRenderPass.valid());

	vector<VkImageView> attachments = {
		lightingAttachment.get_imageView(), depthBuffer.get_imageView(),
	};

	return get_pipelineFactory()->createFramebuffer(lightingRenderPass, swapchain->get_extent(), attachments);
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
	assert(pointLightShader.valid());
	auto unif = pointLightShader.getUniform("settings");

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

spirv::Shader DfrRenderer::createForwardShader()
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vTransparencyShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fTransparencyShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline DfrRenderer::createForwardPipeline()
{
	assert(forwardShader.valid());

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
	colorblendAttachment.blendEnable = VK_TRUE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // TODO: Verify
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

	return context->get_pipelineFactory()->createGraphicsPipeline(forwardShader, lightingRenderPass, info);
}
} // namespace blaze
