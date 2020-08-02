
#include "DirectionLightCaster.hpp"

#include <util/files.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#undef min
#undef max

namespace blaze::fwd
{
DirectionLightCaster::DirectionLightCaster(const Context* context, const spirv::SetVector& sets,
										   const spirv::SetSingleton& texSet) noexcept
{
	renderPass = createRenderPass(context);
	shadowShader = createShader(context);
	shadowPipeline = createPipeline(context);

	auto uniform = sets.getUniform(dataUniformName);
	maxLights = uniform->size / static_cast<uint32_t>(sizeof(LightData));
	lights = std::vector<LightData>(maxLights);
	ubos = UBODataVector(context, uniform->size, sets.size());
	count = 0;

	int i = -1;
	for (auto& light : lights)
	{
		light.direction = glm::vec3(0);
		light.brightness = static_cast<float>(i);
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
		auto& shadow = shadows.emplace_back(context, renderPass, DIRECTION_MAP_RESOLUTION, MAX_CSM_SPLITS);
		shadow.next = i + 1;
	}
	shadows.back().next = -1;
	freeShadow = 0;

	bindDataSet(context, sets);
	bindTextureSet(context, texSet);
}

void DirectionLightCaster::recreate(const Context* context, const spirv::SetVector& sets)
{
	ubos = UBODataVector(context, maxLights * sizeof(LightData), sets.size());

	bindDataSet(context, sets);
}

glm::vec4 DirectionLightCaster::createCascadeSplits(int numSplits, float nearPlane, float farPlane, float lambda) const
{
	static_assert(MAX_CSM_SPLITS <= 4);
	glm::vec4 splits(farPlane);
	// Ref:
	// https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
	float _m = 1.0f / static_cast<float>(numSplits);
	for (int i = 1; i < numSplits; i++)
	{
		float c_log = nearPlane * pow(farPlane / nearPlane, i * _m);
		float c_uni = nearPlane + (farPlane - nearPlane) * i * _m;

		splits[i - 1] = (lambda * c_log + (1.0f - lambda) * c_uni);
	}
	splits[3] = farPlane;

	return splits;
}

float DirectionLightCaster::centerDist(float n, float f, float cosine) const
{
	// TODO: FIX: Min waste
	float secTheta = 1.0f / cosine;
	return 0.5f * (f + n) * secTheta * secTheta;
}

void DirectionLightCaster::updateLight(const Camera* camera, LightData* light)
{
	// TODO: Recheck
	glm::vec4 frustumCorners[] = {
		{-1, -1, -1, 1}, // ---
		{-1, 1, -1, 1},	 // -+-
		{1, -1, -1, 1},	 // +--
		{1, 1, -1, 1},	 // ++-
		{-1, -1, 1, 1},	 // --+
		{-1, 1, 1, 1},	 // -++
		{1, -1, 1, 1},	 // +-+
		{1, 1, 1, 1}	 // +++
	};
	glm::mat4 invProj = glm::inverse(camera->get_projection() * camera->get_view());

	for (auto& vert : frustumCorners)
	{
		vert = invProj * vert;
		vert /= vert.w;
	}

	light->cascadeSplits = createCascadeSplits(light->numCascades, camera->get_nearPlane(), camera->get_farPlane());

	float cosine =
		glm::dot(glm::normalize(glm::vec3(frustumCorners[4]) - camera->get_position()), camera->get_direction());
	glm::vec3 cornerRay = glm::normalize(glm::vec3(frustumCorners[4] - frustumCorners[0]));
	
	float prevPlane = camera->get_nearPlane();
	float plane = 0;
	for (int i = 0; i < light->numCascades; ++i)
	{
		plane = light->cascadeSplits[i];
		float cDist = centerDist(prevPlane, plane, cosine);
		auto center = camera->get_direction() * cDist + camera->get_position();

		float nearRatio = (prevPlane / camera->get_farPlane());
		auto corner = cornerRay * nearRatio + camera->get_position();

		float r = glm::distance(center, corner);
		auto& bz = light->direction;

		// TODO Farplane fixes
		auto lightOrthoMatrix = glm::ortho(-r, r, -r, r, 0.0f, 4*r);
		auto lightViewMatrix =
			glm::lookAt(center - 3.0f * bz * r, center, glm::vec3(0, 1, 0));
		glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
		glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		shadowOrigin = shadowMatrix * shadowOrigin;
		shadowOrigin = shadowOrigin * (float)DIRECTION_MAP_RESOLUTION / 2.0f;

		glm::vec4 roundedOrigin = glm::round(shadowOrigin);
		glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
		roundOffset = roundOffset * 2.0f / (float)DIRECTION_MAP_RESOLUTION;
		roundOffset.z = 0.0f;
		roundOffset.w = 0.0f;

		glm::mat4 shadowProj = lightOrthoMatrix;
		shadowProj[3] += roundOffset;
		lightOrthoMatrix = shadowProj;

		light->cascadeViewProj[i] = lightOrthoMatrix * lightViewMatrix;

		prevPlane = plane;
	}
}

void DirectionLightCaster::update(const Camera* camera, uint32_t frame)
{
	for (auto& light : lights)
	{
		updateLight(camera, &light);
	}
	ubos[frame].writeData(lights.data(), maxLights * sizeof(LightData));
}

uint16_t DirectionLightCaster::createLight(const glm::vec3& direction, float brightness, uint32_t numCascades)
{
	if (freeLight < 0)
	{
		return UINT16_MAX;
	}

	LightData* pLight = &lights[freeLight];
	uint16_t idx = static_cast<uint16_t>(freeLight);

	freeLight = -static_cast<int>(pLight->brightness);

	pLight->direction = glm::normalize(direction);
	pLight->brightness = brightness;
	pLight->numCascades = numCascades;
	pLight->shadowIdx = -1;
	// TODO Shadow

	if (numCascades > 0)
	{
		pLight->shadowIdx = createShadow();
		shadows[pLight->shadowIdx].next = idx;
	}

	count++;

	return idx;
}

void DirectionLightCaster::removeLight(uint16_t idx)
{
	assert(idx < maxLights);

	LightData* pLight = &lights[idx];

	assert(pLight->brightness > 0);

	pLight->brightness = -static_cast<float>(freeLight);
	pLight->direction = glm::vec3(0.0f);

	if (pLight->shadowIdx >= 0)
	{
		assert(shadows[pLight->shadowIdx].next == idx);
		removeShadow(pLight->shadowIdx);
	}

	pLight->shadowIdx = -1;

	count--;
	freeLight = idx;
}

bool DirectionLightCaster::setShadow(uint16_t idx, bool enableShadow)
{
	assert(idx < maxLights);

	LightData* pLight = &lights[idx];

	assert(pLight->brightness > 0);

	bool hasShadow = pLight->shadowIdx >= 0;

	if (hasShadow == enableShadow)
	{
		return hasShadow;
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

int DirectionLightCaster::createShadow()
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

void DirectionLightCaster::removeShadow(int idx)
{
	shadows[idx].next = freeShadow;
	freeShadow = idx;
	shadowCount--;
}

void DirectionLightCaster::cast(VkCommandBuffer cmd, const std::vector<Drawable*>& drawables)
{
	for (auto& light : lights)
	{
		if (light.shadowIdx < 0)
			continue;
		DirectionShadow* shadow = &shadows[light.shadowIdx];

		for (int i = 0; i < light.numCascades; ++i)
		{
			renderPass.begin(cmd, shadow->framebuffers[i]);

			shadowPipeline.bind(cmd);
			vkCmdSetViewport(cmd, 0, 1, &shadow->viewport);
			vkCmdSetScissor(cmd, 0, 1, &shadow->scissor);
			vkCmdPushConstants(cmd, shadowShader.pipelineLayout.get(), shadowShader.pushConstant.stage,
							   sizeof(ModelPushConstantBlock), sizeof(glm::mat4), &light.cascadeViewProj[i]);
			for (Drawable* d : drawables)
			{
				d->drawGeometry(cmd, shadowShader.pipelineLayout.get());
			}

			renderPass.end(cmd);
		}
	}
}

void DirectionLightCaster::bindDataSet(const Context* context, const spirv::SetVector& sets)
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

void DirectionLightCaster::bindTextureSet(const Context* context, const spirv::SetSingleton& set)
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

spirv::RenderPass DirectionLightCaster::createRenderPass(const Context* context)
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

	VkClearValue clear = {};
	clear.depthStencil = {1.0f, 0};
	auto rp = context->get_pipelineFactory()->createRenderPass(format, subpass);
	rp.clearValues = {clear};
	return std::move(rp);
}

spirv::Shader DirectionLightCaster::createShader(const Context* context)
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

spirv::Pipeline DirectionLightCaster::createPipeline(const Context* context)
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
	info.rasterizerCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
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

DirectionShadow::DirectionShadow(const Context* context, const spirv::RenderPass& renderPass, uint32_t mapResolution,
								   uint32_t numCascades) noexcept
{
	ImageData2D id2d{};
	id2d.height = mapResolution;
	id2d.width = mapResolution;
	id2d.numChannels = 1;
	id2d.size = mapResolution * mapResolution;
	id2d.layerCount = numCascades;
	id2d.anisotropy = VK_FALSE;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	id2d.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	id2d.format = VK_FORMAT_D32_SFLOAT;
	id2d.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	id2d.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	shadowMap = Texture2D(context, id2d, false);

	viewport = VkViewport{0.0f,
						  static_cast<float>(mapResolution),
						  static_cast<float>(mapResolution),
						  -static_cast<float>(mapResolution),
						  0.0f,
						  1.0f};

	scissor.offset = {0, 0};
	scissor.extent = {mapResolution, mapResolution};

	for (int i = 0; i < MAX_CSM_SPLITS; ++i)
	{
		std::vector<VkImageView> attachments = {shadowMap.get_imageView(i)};

		framebuffers.push_back(
			context->get_pipelineFactory()->createFramebuffer(renderPass, scissor.extent, attachments));
	}
}
} // namespace blaze
