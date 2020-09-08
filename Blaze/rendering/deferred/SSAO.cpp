
#include "SSAO.hpp"

#include <random>
#include <util/files.hpp>
#include <gui/GUI.hpp>

#include <thirdparty/optick/optick.h>

namespace blaze
{
SSAO::SSAO(const Context* context, const Texture2D* position, const Texture2D* normal, const Texture2D* omr)
{
	auto extent = position->get_extent();
	std::tie(viewport, scissor) = createViewportScissor(extent);
	extent.height /= 2;
	extent.width /= 2;
	std::tie(halfViewport, halfScissor) = createViewportScissor(extent);

	noise = createNoise(context);
	kernel = createKernel(context);

	samplingAttachment = createSamplingAttachment(context);
	renderpass = createSamplingRenderpass(context);
	samplingShader = createSamplingShader(context);
	samplingPipeline = createSamplingPipeline(context);
	samplingFramebuffer = createSamplingFramebuffer(context);

	kernelSet = createKernelSet(context);
	samplingPosNormSet = createSamplingPosNormSet(context, position, normal);

	blurAttachment = createSamplingAttachment(context);
	blurShader = createBlurShader(context);
	blurPipeline = createBlurPipeline(context);
	blurFramebuffer = createBlurFramebuffer(context);
	blurSet = createBlurSet(context, position);

	filterRenderPass = createFilterRenderPass(context, omr);
	filterFramebuffer = createFilterFramebuffer(context, omr);
	filterShader = createFilterShader(context);
	filterPipeline = createFilterPipeline(context);
	filterSet = createFilterSet(context, position);
}

void SSAO::recreate(const Context* context, const Texture2D* position, const Texture2D* normal, const Texture2D* omr)
{
	auto extent = position->get_extent();
	std::tie(viewport, scissor) = createViewportScissor(extent);
	extent.height /= 2;
	extent.width /= 2;
	std::tie(halfViewport, halfScissor) = createViewportScissor(extent);

	samplingAttachment = createSamplingAttachment(context);
	samplingFramebuffer = createSamplingFramebuffer(context);

	kernelSet = createKernelSet(context);
	samplingPosNormSet = createSamplingPosNormSet(context, position, normal);

	blurAttachment = createSamplingAttachment(context);
	blurFramebuffer = createBlurFramebuffer(context);
	blurSet = createBlurSet(context, position);

	filterFramebuffer = createFilterFramebuffer(context, omr);
	filterSet = createFilterSet(context, omr);
}

void SSAO::process(VkCommandBuffer cmd, spirv::SetVector& cameraSets, uint32_t frame, IndexedVertexBuffer<Vertex>& lightQuad)
{
	if (enabled)
	{
		OPTICK_EVENT();
		renderpass.begin(cmd, samplingFramebuffer);

		VkExtent2D ssaoExtent = samplingFramebuffer.renderArea.extent;

		VkViewport ssaoViewport = {};
		ssaoViewport.minDepth = 0.0f;
		ssaoViewport.maxDepth = 1.0f;
		ssaoViewport.x = 0;
		ssaoViewport.y = static_cast<float>(ssaoExtent.height);
		ssaoViewport.width = static_cast<float>(ssaoExtent.width);
		ssaoViewport.height = -static_cast<float>(ssaoExtent.height);

		vkCmdSetScissor(cmd, 0, 1, &samplingFramebuffer.renderArea);
		vkCmdSetViewport(cmd, 0, 1, &ssaoViewport);

		samplingPipeline.bind(cmd);
		vkCmdBindDescriptorSets(cmd, samplingPipeline.bindPoint, samplingShader.pipelineLayout.get(),
								cameraSets.setIdx, 1, &cameraSets[frame], 0, nullptr);
		vkCmdBindDescriptorSets(cmd, samplingPipeline.bindPoint, samplingShader.pipelineLayout.get(),
								samplingPosNormSet.setIdx, 1, &samplingPosNormSet.get(), 0, nullptr);
		vkCmdBindDescriptorSets(cmd, samplingPipeline.bindPoint, samplingShader.pipelineLayout.get(),
								kernelSet.setIdx, 1, &kernelSet.get(), 0, nullptr);
		vkCmdPushConstants(cmd, samplingShader.pipelineLayout.get(), samplingShader.pushConstant.stage, 0,
						   samplingShader.pushConstant.size, &settings);
		lightQuad.bind(cmd);
		vkCmdDrawIndexed(cmd, lightQuad.get_indexCount(), 1, 0, 0, 0);

		renderpass.end(cmd);

		if (blurEnabled)
		{
			for (int i = 0; i < blurCount; i++)
			{
				renderpass.begin(cmd, blurFramebuffer);

				vkCmdSetScissor(cmd, 0, 1, &blurFramebuffer.renderArea);
				vkCmdSetViewport(cmd, 0, 1, &ssaoViewport);

				blurSettings.verticalPass = 0;
				blurPipeline.bind(cmd);
				vkCmdBindDescriptorSets(cmd, blurPipeline.bindPoint,
										blurShader.pipelineLayout.get(), cameraSets.setIdx, 1, &cameraSets[frame],
										0, nullptr);
				vkCmdBindDescriptorSets(cmd, blurPipeline.bindPoint,
										blurShader.pipelineLayout.get(), blurSet.setIdx, 1, &blurSet.get(),
										0, nullptr);
				vkCmdPushConstants(cmd, blurShader.pipelineLayout.get(),
								   blurShader.pushConstant.stage, 0, blurShader.pushConstant.size,
								   &blurSettings);
				lightQuad.bind(cmd);
				vkCmdDrawIndexed(cmd, lightQuad.get_indexCount(), 1, 0, 0, 0);

				renderpass.end(cmd);

				renderpass.begin(cmd, samplingFramebuffer);

				vkCmdSetScissor(cmd, 0, 1, &samplingFramebuffer.renderArea);
				vkCmdSetViewport(cmd, 0, 1, &ssaoViewport);

				blurSettings.verticalPass = 1;
				blurPipeline.bind(cmd);
				vkCmdBindDescriptorSets(cmd, blurPipeline.bindPoint,
										blurShader.pipelineLayout.get(), cameraSets.setIdx, 1, &cameraSets[frame],
										0, nullptr);
				vkCmdBindDescriptorSets(cmd, blurPipeline.bindPoint,
										blurShader.pipelineLayout.get(), filterSet.setIdx, 1,
										&filterSet.get(), 0, nullptr);
				vkCmdPushConstants(cmd, blurShader.pipelineLayout.get(),
								   blurShader.pushConstant.stage, 0, blurShader.pushConstant.size,
								   &blurSettings);
				lightQuad.bind(cmd);
				vkCmdDrawIndexed(cmd, lightQuad.get_indexCount(), 1, 0, 0, 0);

				renderpass.end(cmd);
			}
		}

		filterRenderPass.begin(cmd, filterFramebuffer);

		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		filterPipeline.bind(cmd);
		vkCmdBindDescriptorSets(cmd, filterPipeline.bindPoint,
								filterShader.pipelineLayout.get(), cameraSets.setIdx, 1, &cameraSets[frame], 0,
								nullptr);
		vkCmdBindDescriptorSets(cmd, filterPipeline.bindPoint,
								filterShader.pipelineLayout.get(), blurSet.setIdx, 1, &blurSet.get(), 0,
								nullptr);

		lightQuad.bind(cmd);
		vkCmdDrawIndexed(cmd, lightQuad.get_indexCount(), 1, 0, 0, 0);

		filterRenderPass.end(cmd);
	}
}

void SSAO::drawSettings()
{
	if (ImGui::CollapsingHeader("SSAO"))
	{
		ImGui::Checkbox("Enabled##SSAO", &enabled);
		if (!enabled)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		ImGui::DragFloat("Kernel Radius##SSAO", &settings.kernelRadius, 0.01f, 0.01f, 1.0f);
		ImGui::DragFloat("Bias##SSAO", &settings.bias, 0.01f, 0.01f, 1.0f);

		ImGui::Checkbox("Blur Enabled##SSAO", &blurEnabled);
		if (!blurEnabled)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		ImGui::DragInt("Blur Iterations##SSAO", &blurCount, 0.2f, 1, 5);
		ImGui::Checkbox("Bilateral Blur##SSAO", (bool*)&blurSettings.depthAware);
		ImGui::DragFloat("Bilateral Threshold##SSAO", &blurSettings.depth, 0.001f, 0.001f, 0.1f);

		if (!blurEnabled)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		if (!enabled)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
	}
}

Texture2D SSAO::createNoise(const Context* context)
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

	return Texture2D(context, id2d, false);
}

BaseUBO SSAO::createKernel(const Context* context)
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

	auto ubo = BaseUBO(context, sizeof(data));
	ubo.writeData(data, sizeof(data));
	return std::move(ubo);
}

spirv::SetSingleton SSAO::createKernelSet(const Context* context)
{
	assert(noise.valid());
	assert(samplingShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*samplingShader.getSetWithUniform("kernel"));

	std::vector<VkWriteDescriptorSet> writes(2);

	const spirv::UniformInfo* unif = samplingShader.getUniform("kernel");
	VkDescriptorBufferInfo* bufferInfo = &kernel.get_descriptorInfo();

	VkWriteDescriptorSet* write = &writes[0];
	write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write->descriptorType = unif->type;
	write->descriptorCount = unif->arrayLength;
	write->dstSet = set.get();
	write->dstBinding = unif->binding;
	write->dstArrayElement = 0;
	write->pBufferInfo = bufferInfo;

	unif = samplingShader.getUniform("noise");
	const VkDescriptorImageInfo* imageInfo = &noise.get_imageInfo();

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

Texture2D SSAO::createSamplingAttachment(const Context* context)
{
	auto extent = scissor.extent;

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

	return Texture2D(context, id2d, false);
}

spirv::RenderPass SSAO::createSamplingRenderpass(const Context* context)
{
	assert(samplingAttachment.valid());

	using namespace spirv;

	VkAttachmentReference colorRef;

	std::vector<AttachmentFormat> attachments(1);
	attachments[0].format = samplingAttachment.get_format();
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

	auto rp = context->get_pipelineFactory()->createRenderPass(attachments, {subpassDesc});
	rp.clearValues = {ssaoClear};
	return std::move(rp);
}

spirv::Shader SSAO::createSamplingShader(const Context* context)
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline SSAO::createSamplingPipeline(const Context* context)
{
	assert(renderpass.valid());
	assert(samplingShader.valid());

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

	return context->get_pipelineFactory()->createGraphicsPipeline(samplingShader, renderpass, info);
}

spirv::Framebuffer SSAO::createSamplingFramebuffer(const Context* context)
{
	using namespace std;
	assert(samplingAttachment.valid());
	assert(renderpass.valid());

	vector<VkImageView> attachments = {
		samplingAttachment.get_imageView(),
	};

	return context->get_pipelineFactory()->createFramebuffer(
		renderpass, {samplingAttachment.get_width(), samplingAttachment.get_height()}, attachments);
}

spirv::SetSingleton SSAO::createSamplingPosNormSet(const Context* context, const Texture2D* position,
												 const Texture2D* normal)
{
	assert(samplingShader.valid());
	assert(normal->valid());
	assert(position->valid());

	auto set = context->get_pipelineFactory()->createSet(*samplingShader.getSetWithUniform("I_NORMAL"));

	std::vector<const spirv::UniformInfo*> unifs = {
		set.getUniform("I_NORMAL"),
		set.getUniform("I_POSITION"),
	};
	std::vector<VkDescriptorImageInfo> infos = {
		normal->get_imageInfo(),
		position->get_imageInfo(),
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

spirv::Framebuffer SSAO::createBlurFramebuffer(const Context* context)
{
	using namespace std;
	assert(blurAttachment.valid());
	assert(renderpass.valid());

	vector<VkImageView> attachments = {
		blurAttachment.get_imageView(),
	};

	return context->get_pipelineFactory()->createFramebuffer(renderpass, blurAttachment.get_extent(),
															 attachments);
}

spirv::Shader SSAO::createBlurShader(const Context* context)
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vBlurShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fBlurShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline SSAO::createBlurPipeline(const Context* context)
{
	assert(renderpass.valid());
	assert(blurShader.valid());

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

	return context->get_pipelineFactory()->createGraphicsPipeline(blurShader, renderpass, info);
}

spirv::SetSingleton SSAO::createBlurSet(const Context* context, const Texture2D* position)
{
	assert(position->valid());
	assert(blurShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*blurShader.getSetWithUniform("I_SSAO"));

	std::vector<const spirv::UniformInfo*> unifs = {
		set.getUniform("I_SSAO"),
		set.getUniform("I_POSITION"),
	};
	std::vector<VkDescriptorImageInfo> infos = {
		samplingAttachment.get_imageInfo(),
		position->get_imageInfo(),
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

spirv::RenderPass SSAO::createFilterRenderPass(const Context* context, const Texture2D* omr)
{
	assert(omr->valid());

	using namespace spirv;

	VkAttachmentReference colorRef;

	std::vector<AttachmentFormat> attachments(1);
	attachments[0].format = omr->get_format();
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

	auto rp = context->get_pipelineFactory()->createRenderPass(attachments, {subpassDesc});
	rp.clearValues = {ssaoClear};
	return std::move(rp);
}

spirv::Framebuffer SSAO::createFilterFramebuffer(const Context* context, const Texture2D* omr)
{
	using namespace std;
	assert(omr->valid());
	assert(filterRenderPass.valid());

	vector<VkImageView> attachments = {
		omr->get_imageView(),
	};

	return context->get_pipelineFactory()->createFramebuffer(filterRenderPass, scissor.extent, attachments);
}

spirv::Shader SSAO::createFilterShader(const Context* context)
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vFilterShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fFilterShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline SSAO::createFilterPipeline(const Context* context)
{
	assert(filterRenderPass.valid());
	assert(filterShader.valid());

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

	return context->get_pipelineFactory()->createGraphicsPipeline(filterShader, filterRenderPass, info);
}

spirv::SetSingleton SSAO::createFilterSet(const Context* context, const Texture2D* position)
{
	assert(position->valid());
	assert(filterShader.valid());

	auto set = context->get_pipelineFactory()->createSet(*filterShader.getSetWithUniform("I_SSAO"));

	std::vector<const spirv::UniformInfo*> unifs = {
		set.getUniform("I_SSAO"),
		set.getUniform("I_POSITION"),
	};
	std::vector<VkDescriptorImageInfo> infos = {
		blurAttachment.get_imageInfo(),
		position->get_imageInfo(),
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

} // namespace blaze