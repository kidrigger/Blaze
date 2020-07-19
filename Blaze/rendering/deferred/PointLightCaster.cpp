
#include "PointLightCaster.hpp"

#include <util/files.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#undef min
#undef max

namespace blaze::dfr
{
PointLightCaster::PointLightCaster(const Context* context, uint32_t maxLights, const spirv::SetVector& sets,
								   const spirv::SetSingleton& texSet) noexcept
	: maxLights(maxLights)
{
	renderPass = createRenderPass(context);
	shadowShader = createShader(context);
	shadowPipeline = createPipeline(context);

	auto uniform = sets.getUniform(dataUniformName);
	lights = std::vector<LightData>(maxLights);
	ubos = SSBODataVector(context, maxLights * sizeof(LightData), sets.size());
	count = 0;

	int i = -1;
	for (auto& light : lights)
	{
		light.position = glm::vec3(0);
		light.brightness = static_cast<float>(i);
		light.radius = 0.0f;
		light.shadowIdx = -1;
		i--;
	}
	i = std::numeric_limits<int>::min();
	lights.back().brightness = static_cast<float>(i);
	freeLight = 0;

	uniform = texSet.getUniform(textureUniformName);
	maxShadows = uniform->arrayLength;

	for (uint32_t i = 0; i < maxShadows; ++i)
	{
		auto& shadow = shadows.emplace_back(context, renderPass.get(), OMNI_MAP_RESOLUTION);
		shadow.next = i + 1;
	}
	shadows.back().next = -1;
	freeShadow = 0;

	bindDataSet(context, sets);
	bindTextureSet(context, texSet);

	CubemapUBlock block = {
		glm::perspective(glm::radians(90.0f), 1.0f, 1.0f, 2.0f),
		{
			// POSITIVE_X (Outside in - so NEG_X face)
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f) + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// NEGATIVE_X (Outside in - so POS_X face)
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f) + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// POSITIVE_Y
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f) + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
			// NEGATIVE_Y
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f) + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			// POSITIVE_Z
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f) + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// NEGATIVE_Z
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f) + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		},
	};

	viewSet = context->get_pipelineFactory()->createSet(*shadowShader.getSetWithUniform("views"));
	viewUBO = UBO(context, block);
	{
		auto unif = viewSet.getUniform("views");
		VkDescriptorBufferInfo info = viewUBO.get_descriptorInfo();

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = unif->type;
		write.descriptorCount = unif->arrayLength;
		write.dstSet = viewSet.get();
		write.dstBinding = unif->binding;
		write.dstArrayElement = 0;
		write.pBufferInfo = &info;

		vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);
	}
}

void PointLightCaster::recreate(const Context* context, const spirv::SetVector& sets)
{
	ubos = SSBODataVector(context, maxLights * sizeof(LightData), sets.size());

	for (uint32_t i = 0; i < sets.size(); ++i)
	{
		update(i);
	}

	bindDataSet(context, sets);
}

void PointLightCaster::update(uint32_t frame)
{
	ubos[frame].writeData(lights.data(), maxLights * sizeof(LightData));
}

uint16_t PointLightCaster::createLight(const glm::vec3& position, float brightness, float radius, bool enableShadow)
{
	if (freeLight < 0)
	{
		return UINT16_MAX;
	}

	LightData* pLight = &lights[freeLight];
	uint16_t idx = static_cast<uint16_t>(freeLight);

	freeLight = -static_cast<int>(pLight->brightness);

	pLight->position = position;
	pLight->brightness = brightness;
	pLight->radius = radius;
	pLight->shadowIdx = -1;
	if (enableShadow)
	{
		pLight->shadowIdx = createShadow();
		shadows[pLight->shadowIdx].next = idx;
	}

	count++;

	return idx;
}

void PointLightCaster::removeLight(uint16_t idx)
{
	assert(idx < maxLights);

	LightData* pLight = &lights[idx];

	assert(pLight->brightness > 0);

	pLight->brightness = -static_cast<float>(freeLight);
	pLight->position = glm::vec3(0.0f);

	if (pLight->shadowIdx >= 0)
	{
		assert(shadows[pLight->shadowIdx].next == idx);
		removeShadow(pLight->shadowIdx);
	}

	pLight->shadowIdx = -1;

	count--;
	freeLight = idx;
}

bool PointLightCaster::setShadow(uint16_t idx, bool enableShadow)
{
	assert(idx < maxLights);

	LightData* pLight = &lights[idx];

	assert(pLight->brightness >= 0);

	if ((pLight->shadowIdx < 0) != enableShadow)
	{
		return pLight->shadowIdx >= 0;
	}
	else if (enableShadow)
	{
		pLight->shadowIdx = createShadow();
		if (pLight->shadowIdx >= 0)
		{
			shadows[pLight->shadowIdx].next = idx;
			return true;
		}
	}
	else
	{
		removeShadow(pLight->shadowIdx);
		pLight->shadowIdx = -1;
	}
	return false;
}

int PointLightCaster::createShadow()
{
	if (freeShadow < 0)
	{
		return -1;
	}
	auto i = freeShadow;
	freeShadow = shadows[i].next;
	shadowCount++;

	return i;
}

void PointLightCaster::removeShadow(int idx)
{
	shadows[idx].next = freeShadow;
	freeShadow = idx;
	shadowCount--;
}

void PointLightCaster::cast(VkCommandBuffer cmd, const std::vector<Drawable*>& drawables)
{
	for (auto& light : lights)
	{
		if (light.shadowIdx < 0)
			continue;
		PointShadow* shadow = &shadows[light.shadowIdx];

		VkRenderPassBeginInfo renderpassBeginInfo = {};
		renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpassBeginInfo.renderPass = renderPass.get();
		renderpassBeginInfo.framebuffer = shadow->framebuffer.get();
		renderpassBeginInfo.renderArea.offset = {0, 0};
		renderpassBeginInfo.renderArea.extent = {shadow->shadowMap.get_width(), shadow->shadowMap.get_height()};

		std::array<VkClearValue, 1> clearColor;
		clearColor[0].depthStencil = {1.0f, 0};
		renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
		renderpassBeginInfo.pClearValues = clearColor.data();

		vkCmdBeginRenderPass(cmd, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		float denom = (0.3f - light.radius);
		float p22 = light.radius / denom;
		float p32 = (0.3f * light.radius) / denom;

		PointShadow::PCB pcb = {
			light.position,
			light.radius,
			p22,
			p32,
		};

		VkRect2D scissor;
		scissor.extent = {shadow->shadowMap.get_width(), shadow->shadowMap.get_height()};
		scissor.offset = {0, 0};

		shadowPipeline.bind(cmd);
		vkCmdSetViewport(cmd, 0, 1, &shadow->viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowShader.pipelineLayout.get(), viewSet.setIdx,
								1, &viewSet.get(), 0, nullptr);
		vkCmdPushConstants(cmd, shadowShader.pipelineLayout.get(), shadowShader.pushConstant.stage,
						   sizeof(ModelPushConstantBlock), sizeof(PointShadow::PCB), &pcb);
		for (Drawable* d : drawables)
		{
			d->drawGeometry(cmd, shadowShader.pipelineLayout.get());
		}

		vkCmdEndRenderPass(cmd);
	}
}

void PointLightCaster::bindDataSet(const Context* context, const spirv::SetVector& sets)
{
	const spirv::UniformInfo* unif = sets.getUniform(dataUniformName);

	for (uint32_t i = 0; i < sets.size(); i++)
	{
		VkDescriptorBufferInfo info = ubos[i].get_descriptorInfo();

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = unif->type;
		write.descriptorCount = unif->arrayLength;
		write.dstSet = sets[i];
		write.dstBinding = unif->binding;
		write.dstArrayElement = 0;
		write.pBufferInfo = &info;

		vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);
	}
}

void PointLightCaster::bindTextureSet(const Context* context, const spirv::SetSingleton& set)
{
	const spirv::UniformInfo* unif = set.getUniform(textureUniformName);

	std::vector<VkDescriptorImageInfo> infos;
	infos.reserve(maxShadows);
	for (auto& shadow : shadows)
	{
		infos.push_back(shadow.shadowMap.get_imageInfo());
	}

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorType = unif->type;
	write.descriptorCount = unif->arrayLength;
	write.dstSet = set.get();
	write.dstBinding = unif->binding;
	write.dstArrayElement = 0;
	write.pImageInfo = infos.data();

	vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);
}

spirv::RenderPass PointLightCaster::createRenderPass(const Context* context)
{
	using namespace spirv;
	std::vector<AttachmentFormat> format(1);
	format[0].usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	format[0].format = VK_FORMAT_D32_SFLOAT;
	format[0].sampleCount = VK_SAMPLE_COUNT_1_BIT;
	format[0].loadStoreConfig = LoadStoreConfig(LoadStoreConfig::LoadAction::CLEAR, LoadStoreConfig::StoreAction::READ);

	VkAttachmentReference depthRef = {};
	depthRef.attachment = 0;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::vector<VkSubpassDescription> subpass(1);
	subpass[0].pDepthStencilAttachment = &depthRef;
	subpass[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass[0].inputAttachmentCount = 0;
	subpass[0].pInputAttachments = nullptr;
	subpass[0].colorAttachmentCount = 0;
	subpass[0].pColorAttachments = nullptr;
	subpass[0].preserveAttachmentCount = 0;
	subpass[0].pPreserveAttachments = nullptr;
	subpass[0].pResolveAttachments = nullptr;
	subpass[0].flags = 0;

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

	return context->get_pipelineFactory()->createRenderPass(format, subpass, &multiview);
}

spirv::Shader PointLightCaster::createShader(const Context* context)
{
	std::vector<spirv::ShaderStageData> stages;

	spirv::ShaderStageData* stage;
	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(vertShaderFileName);
	stage->stage = VK_SHADER_STAGE_VERTEX_BIT;

	stage = &stages.emplace_back();
	stage->spirv = util::loadBinaryFile(fragShaderFileName);
	stage->stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	return context->get_pipelineFactory()->createShader(stages);
}

spirv::Pipeline PointLightCaster::createPipeline(const Context* context)
{
	assert(shadowShader.valid());
	assert(renderPass.valid());

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

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 0;
	info.colorblendCreateInfo.pAttachments = nullptr;

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

	return context->get_pipelineFactory()->createGraphicsPipeline(shadowShader, renderPass, info);
}

// Point Shadow 2
PointShadow::PointShadow(const Context* context, VkRenderPass renderPass, uint32_t mapResolution) noexcept
{
	ImageDataCube idc{};
	idc.height = mapResolution;
	idc.width = mapResolution;
	idc.numChannels = 1;
	idc.size = 6 * mapResolution * mapResolution;
	idc.layerSize = mapResolution * mapResolution;
	idc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	idc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	idc.format = VK_FORMAT_D32_SFLOAT;
	idc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	idc.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	shadowMap = TextureCube(context, idc, false);

	viewport = VkViewport{0.0f,
						  static_cast<float>(mapResolution),
						  static_cast<float>(mapResolution),
						  -static_cast<float>(mapResolution),
						  0.0f,
						  1.0f};

	{
		std::vector<VkImageView> attachments = {shadowMap.get_imageView()};

		VkFramebuffer fbo = VK_NULL_HANDLE;
		VkFramebufferCreateInfo fbCreateInfo = {};
		fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbCreateInfo.width = mapResolution;
		fbCreateInfo.height = mapResolution;
		fbCreateInfo.layers = 6;
		fbCreateInfo.renderPass = renderPass;
		fbCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbCreateInfo.pAttachments = attachments.data();
		vkCreateFramebuffer(context->get_device(), &fbCreateInfo, nullptr, &fbo);
		framebuffer = vkw::Framebuffer(fbo, context->get_device());
	}
}

} // namespace blaze