
#include "Environment.hpp"

#include <util/processing.hpp>
#include <rendering/ARenderer.hpp>

namespace blaze::util
{
TextureCube createIrradianceCube(const Context& context, VkDescriptorSetLayout envLayout, VkDescriptorSet environment)
{
	struct PCB
	{
		float deltaPhi{(2.0f * PI) / 180.0f};
		float deltaTheta{(0.5f * PI) / 64.0f};
	} pcb = {};

	util::Texture2CubemapInfo<PCB> info = {
		"shaders/vIrradianceMultiview.vert.spv", "shaders/fIrradiance.frag.spv", environment, envLayout, 64u, pcb,
	};

	return util::Process<PCB>::convertDescriptorToCubemap(context, info);
}

TextureCube createPrefilteredCube(const Context& context, VkDescriptorSetLayout envLayout, VkDescriptorSet environment)
{
	struct PCB
	{
		float roughness;
		float miplevel;
	};

	util::Texture2CubemapInfo<PCB> info = {
		"shaders/vIrradiance.vert.spv", "shaders/fPrefilter.frag.spv", environment, envLayout, 128u, {0, 0},
	};
	auto timer = AutoTimer("Process " + info.frag_shader + " took (us)");

	const uint32_t dim = info.cube_side;

	util::Managed<VkPipelineLayout> irPipelineLayout;
	util::Managed<VkPipeline> irPipeline;
	util::Managed<VkRenderPass> irRenderPass;
	util::Managed<VkFramebuffer> irFramebuffer;

	VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

	// Setup the TextureCube
	ImageDataCube idc{};
	idc.height = dim;
	idc.width = dim;
	idc.numChannels = 4;
	idc.size = 4 * 6 * dim * dim;
	idc.layerSize = 4 * dim * dim;
	idc.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	idc.format = format;
	idc.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	TextureCube irradianceMap(context, idc, true);

	ImageData2D id2d{};
	id2d.height = dim;
	id2d.width = dim;
	id2d.numChannels = 4;
	id2d.size = 4 * dim * dim;
	id2d.format = format;
	id2d.usage = id2d.usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	id2d.access = VK_ACCESS_SHADER_WRITE_BIT;
	Texture2D fbColorAttachment(context, id2d, false);

	struct CubePushConstantBlock
	{
		glm::mat4 mvp;
	};

	{
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {info.layout};

		std::vector<VkPushConstantRange> pushConstantRanges;
		{
			VkPushConstantRange pushConstantRange = {};
			pushConstantRange.offset = 0;
			pushConstantRange.size = sizeof(CubePushConstantBlock);
			pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			pushConstantRanges.push_back(pushConstantRange);
			pushConstantRange.offset = sizeof(CubePushConstantBlock);
			pushConstantRange.size = sizeof(PCB);
			pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			pushConstantRanges.push_back(pushConstantRange);
		}

		irPipelineLayout = util::Managed(
			util::createPipelineLayout(context.get_device(), descriptorSetLayouts, pushConstantRanges),
			[dev = context.get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
	}

	irRenderPass =
		util::Managed(util::createRenderPass(context.get_device(), format),
					  [dev = context.get_device()](VkRenderPass& pass) { vkDestroyRenderPass(dev, pass, nullptr); });

	{
		std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT};

		VkPipeline tPipeline = util::createGraphicsPipeline(
			context.get_device(), irPipelineLayout.get(), irRenderPass.get(), {dim, dim}, info.vert_shader,
			info.frag_shader, dynamicStateEnables, VK_CULL_MODE_FRONT_BIT);
		irPipeline = util::Managed(
			tPipeline, [dev = context.get_device()](VkPipeline& pipe) { vkDestroyPipeline(dev, pipe, nullptr); });
	}

	{
		VkFramebuffer fbo = VK_NULL_HANDLE;
		VkFramebufferCreateInfo fbCreateInfo = {};
		fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbCreateInfo.width = dim;
		fbCreateInfo.height = dim;
		fbCreateInfo.layers = 1;
		fbCreateInfo.renderPass = irRenderPass.get();
		fbCreateInfo.attachmentCount = 1;
		fbCreateInfo.pAttachments = &fbColorAttachment.get_imageView();
		vkCreateFramebuffer(context.get_device(), &fbCreateInfo, nullptr, &fbo);
		irFramebuffer = util::Managed(
			fbo, [dev = context.get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
	}

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

	CubePushConstantBlock pcb{};

	uint32_t totalMips = irradianceMap.get_miplevels();
	uint32_t mipsize = dim;
	auto cmdBuffer = context.startCommandBufferRecord();
	for (uint32_t miplevel = 0; miplevel < totalMips; miplevel++)
	{
		for (int face = 0; face < 6; face++)
		{

			// RENDERPASSES
			VkViewport viewport = {};
			viewport.x = 0.0f;
			viewport.y = static_cast<float>(mipsize);
			viewport.width = static_cast<float>(mipsize);
			viewport.height = -static_cast<float>(mipsize);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRenderPassBeginInfo renderpassBeginInfo = {};
			renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderpassBeginInfo.renderPass = irRenderPass.get();
			renderpassBeginInfo.framebuffer = irFramebuffer.get();
			renderpassBeginInfo.renderArea.offset = {0, 0};
			renderpassBeginInfo.renderArea.extent = {mipsize, mipsize};

			std::vector<VkClearValue> clearColor(1);
			clearColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
			renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
			renderpassBeginInfo.pClearValues = clearColor.data();

			vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipeline.get());
			vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipelineLayout.get(), 0, 1,
									&info.descriptor, 0, nullptr);

			pcb.mvp = proj * matrices[face];
			info.pcb.roughness = (float)miplevel / (float)(totalMips - 1);
			info.pcb.miplevel = static_cast<float>(miplevel);
			vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
							   sizeof(CubePushConstantBlock), &pcb);
			vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT,
							   sizeof(CubePushConstantBlock), sizeof(PCB), &info.pcb);

			VkDeviceSize offsets = {0};

			cube.bind(cmdBuffer);
			vkCmdDrawIndexed(cmdBuffer, cube.get_indexCount(), 1, 0, 0, 0);

			vkCmdEndRenderPass(cmdBuffer);

			fbColorAttachment.transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
											 VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
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
						   irradianceMap.get_image(), irradianceMap.get_imageInfo().imageLayout, 1, &copyRegion);

			fbColorAttachment.transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
											 VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
											 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}

		mipsize /= 2;
	}

	irradianceMap.transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
								 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	context.flushCommandBuffer(cmdBuffer);

	return irradianceMap;
}

Texture2D createBrdfLut(const Context& context)
{
	const uint32_t dim = 512;

	util::Managed<VkPipelineLayout> irPipelineLayout;
	util::Managed<VkPipeline> irPipeline;
	util::Managed<VkRenderPass> irRenderPass;
	util::Managed<VkFramebuffer> irFramebuffer;

	VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
	auto timer = AutoTimer("Process fBrdfLut.frag.spv took (us)");

	ImageData2D id2d{};
	id2d.height = dim;
	id2d.width = dim;
	id2d.numChannels = 4;
	id2d.size = 4 * dim * dim;
	id2d.format = format;
	id2d.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	id2d.access = VK_ACCESS_TRANSFER_WRITE_BIT;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	Texture2D lut(context, id2d, false);

	id2d.usage = id2d.usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	id2d.access = VK_ACCESS_SHADER_WRITE_BIT;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	Texture2D fbColorAttachment(context, id2d, false);

	struct CubePushConstantBlock
	{
		glm::mat4 mvp;
	};

	{
		VkPushConstantRange pcr = {};
		pcr.offset = 0;
		pcr.size = sizeof(CubePushConstantBlock);
		pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		irPipelineLayout = util::Managed(
			util::createPipelineLayout(context.get_device(), std::vector<VkDescriptorSetLayout>(),
									   std::vector<VkPushConstantRange>{pcr}),
			[dev = context.get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
	}

	irRenderPass =
		util::Managed(util::createRenderPass(context.get_device(), format),
					  [dev = context.get_device()](VkRenderPass& pass) { vkDestroyRenderPass(dev, pass, nullptr); });

	{
		auto tPipeline = util::createGraphicsPipeline(context.get_device(), irPipelineLayout.get(), irRenderPass.get(),
													  {dim, dim}, "shaders/vBrdfLut.vert.spv",
													  "shaders/fBrdfLut.frag.spv", {}, VK_CULL_MODE_FRONT_BIT);
		irPipeline = util::Managed(
			tPipeline, [dev = context.get_device()](VkPipeline& pipe) { vkDestroyPipeline(dev, pipe, nullptr); });
	}

	{
		VkFramebuffer fbo = VK_NULL_HANDLE;
		VkFramebufferCreateInfo fbCreateInfo = {};
		fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbCreateInfo.width = dim;
		fbCreateInfo.height = dim;
		fbCreateInfo.layers = 1;
		fbCreateInfo.renderPass = irRenderPass.get();
		fbCreateInfo.attachmentCount = 1;
		fbCreateInfo.pAttachments = &fbColorAttachment.get_imageView();
		vkCreateFramebuffer(context.get_device(), &fbCreateInfo, nullptr, &fbo);
		irFramebuffer = util::Managed(
			fbo, [dev = context.get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
	}

	auto rect = getUVRect(context);

	glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f);

	CubePushConstantBlock pcb{glm::mat4(1.0f)};

	auto cmdBuffer = context.startCommandBufferRecord();

	// RENDERPASSES

	VkRenderPassBeginInfo renderpassBeginInfo = {};
	renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpassBeginInfo.renderPass = irRenderPass.get();
	renderpassBeginInfo.framebuffer = irFramebuffer.get();
	renderpassBeginInfo.renderArea.offset = {0, 0};
	renderpassBeginInfo.renderArea.extent = {dim, dim};

	std::array<VkClearValue, 1> clearColor;
	clearColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
	renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
	renderpassBeginInfo.pClearValues = clearColor.data();

	vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipeline.get());

	vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CubePushConstantBlock),
					   &pcb);

	VkDeviceSize offsets = {0};

	rect.bind(cmdBuffer);
	vkCmdDrawIndexed(cmdBuffer, rect.get_indexCount(), 1, 0, 0, 0);

	vkCmdEndRenderPass(cmdBuffer);

	fbColorAttachment.transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT,
									 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

	VkImageCopy copyRegion = {};

	copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.srcSubresource.baseArrayLayer = 0;
	copyRegion.srcSubresource.mipLevel = 0;
	copyRegion.srcSubresource.layerCount = 1;
	copyRegion.srcOffset = {0, 0, 0};

	copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.dstSubresource.baseArrayLayer = 0;
	copyRegion.dstSubresource.mipLevel = 0;
	copyRegion.dstSubresource.layerCount = 1;
	copyRegion.dstOffset = {0, 0, 0};

	copyRegion.extent.width = static_cast<uint32_t>(dim);
	copyRegion.extent.height = static_cast<uint32_t>(dim);
	copyRegion.extent.depth = 1;

	vkCmdCopyImage(cmdBuffer, fbColorAttachment.get_image(), fbColorAttachment.get_imageInfo().imageLayout,
				   lut.get_image(), lut.get_imageInfo().imageLayout, 1, &copyRegion);

	lut.transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

	context.flushCommandBuffer(cmdBuffer);

	return lut;
}

Environment::Environment(ARenderer* renderer, TextureCube&& skybox)
{
	auto& found = renderer->get_shader().uniformLocations.find("skybox");
	assert(found != renderer->get_shader().uniformLocations.end());

	auto [setIdx, skyboxIdx] = found->second;
	set = renderer->get_pipelineFactory()->createSet(renderer->get_shader().sets[setIdx]);
	auto lay = renderer->get_shader().sets[setIdx].layout.get();

	const spirv::UniformInfo* skyboxInfo = nullptr;
	const spirv::UniformInfo* irradianceInfo = nullptr;
	const spirv::UniformInfo* prefilteredInfo = nullptr;
	const spirv::UniformInfo* brdfLutInfo = nullptr;
	for (auto& uniform : renderer->get_shader().sets[setIdx].uniforms)
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
	write.dstSet = set.get();
	write.dstArrayElement = 0;
	write.descriptorCount = 1;

	this->skybox = std::move(skybox);

	write.dstBinding = skyboxInfo->binding;
	write.descriptorType = skyboxInfo->type;
	write.pImageInfo = &this->skybox.get_imageInfo();
	vkUpdateDescriptorSets(renderer->get_context()->get_device(), 1, &write, 0, nullptr);

	this->irradianceMap = createIrradianceCube(*renderer->get_context(), lay, set.get());

	write.dstBinding = irradianceInfo->binding;
	write.descriptorType = irradianceInfo->type;
	write.pImageInfo = &this->irradianceMap.get_imageInfo();
	vkUpdateDescriptorSets(renderer->get_context()->get_device(), 1, &write, 0, nullptr);

	this->prefilteredMap = createPrefilteredCube(*renderer->get_context(), lay, set.get());

	write.dstBinding = prefilteredInfo->binding;
	write.descriptorType = prefilteredInfo->type;
	write.pImageInfo = &this->prefilteredMap.get_imageInfo();
	vkUpdateDescriptorSets(renderer->get_context()->get_device(), 1, &write, 0, nullptr);

	this->brdfLut = createBrdfLut(*renderer->get_context());

	write.dstBinding = brdfLutInfo->binding;
	write.descriptorType = brdfLutInfo->type;
	write.pImageInfo = &this->brdfLut.get_imageInfo();
	vkUpdateDescriptorSets(renderer->get_context()->get_device(), 1, &write, 0, nullptr);
}
} // namespace blaze::util
