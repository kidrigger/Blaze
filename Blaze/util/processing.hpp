
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <Primitives.hpp>
#include <core/TextureCube.hpp>
#include <core/UniformBuffer.hpp>
#include <core/Context.hpp>
#include <string>
#include <util/DebugTimer.hpp>
#include <util/createFunctions.hpp>

namespace blaze::util
{
/**
 * @class Texture2CubemapInfo
 *
 * @tparam PCB The type of the struct used for PCB.
 *
 * @brief The information to use for the processing.
 */
template <typename PCB>
struct Texture2CubemapInfo
{
	/// @brief The vertex shader to use.
	std::string vert_shader;

	/// @brief The fragment shader to use.
	std::string frag_shader;

	/// @brief The descriptor to use.
	VkDescriptorSet descriptor;

	/// @brief The layout of the descriptor in use/
	VkDescriptorSetLayout layout;

	/// @brief The size of the side of the cubemap.
	uint32_t cube_side{512};

	/// @brief The push constant block to push into the pipeline.
	PCB pcb{};
};

/**
 * @name Ignorable typing.
 *
 * @brief The struct and templated functions to use for ignoring type based parameter.
 *
 * @cond PRIVATE
 */
struct Ignore
{
};

template <typename T>
constexpr bool is_ignore(const T& _x)
{
	return false;
}
template <>
constexpr bool is_ignore<Ignore>(const Ignore& _x)
{
	return true;
}
/**
 * @endcond
 */

/**
 * @class Process
 *
 * @tparam PCB The type of the struct used for PCB.
 *
 * @brief The static processing class for converting from a descriptor to a cubemap.
 */
template <typename PCB>
class Process
{
public:
	/**
	 * @brief Converts the descriptor to a cubemap.
	 *
	 * @param context The current Vulkan Context.
	 * @param info The Texture2CubemapInfo to configure the type.
	 */
	static TextureCube convertDescriptorToCubemap(const Context* context, const Texture2CubemapInfo<PCB>& info)
	{
		auto timer = AutoTimer("Process " + info.frag_shader + " took (us)");
		const uint32_t dim = info.cube_side;

		util::Managed<VkPipelineLayout> irPipelineLayout;
		util::Managed<VkPipeline> irPipeline;
		util::Managed<VkRenderPass> irRenderPass;
		util::Managed<VkFramebuffer> irFramebuffer;
		util::Managed<VkDescriptorSetLayout> views;
		util::Managed<VkDescriptorPool> descriptorPool;
		util::Managed<VkDescriptorSet> descriptorSet;

		VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

		// Setup the TextureCube
		ImageDataCube idc{};
		idc.height = dim;
		idc.width = dim;
		idc.numChannels = 4;
		idc.size = 4 * 6 * dim * dim;
		idc.layerSize = 4 * dim * dim;
		idc.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		idc.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		idc.format = format;
		idc.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		TextureCube irradianceMap(context, idc, false);

		struct CubePushConstantBlock
		{
			glm::mat4 mvp;
		};

		{
			std::vector<VkDescriptorPoolSize> poolSizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}};
			descriptorPool = util::Managed(util::createDescriptorPool(context->get_device(), poolSizes, 1),
										   [dev = context->get_device()](VkDescriptorPool& descPool) {
											   vkDestroyDescriptorPool(dev, descPool, nullptr);
										   });
			std::vector<VkDescriptorSetLayoutBinding> bindings = {
				{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}};

			views = util::Managed(util::createDescriptorSetLayout(context->get_device(), bindings),
								  [dev = context->get_device()](VkDescriptorSetLayout& lay) {
									  vkDestroyDescriptorSetLayout(dev, lay, nullptr);
								  });
		}

		{

			std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {info.layout, views.get()};

			std::vector<VkPushConstantRange> pushConstantRanges;
			{
				VkPushConstantRange pushConstantRange = {};
				pushConstantRange.offset = 0;
				pushConstantRange.size = sizeof(CubePushConstantBlock);
				pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
				pushConstantRanges.push_back(pushConstantRange);
				if (!is_ignore(info.pcb))
				{
					pushConstantRange.offset = sizeof(CubePushConstantBlock);
					pushConstantRange.size = sizeof(PCB);
					pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
					pushConstantRanges.push_back(pushConstantRange);
				}
			}
			irPipelineLayout = util::Managed(
				util::createPipelineLayout(context->get_device(), descriptorSetLayouts, pushConstantRanges),
				[dev = context->get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
		}

		irRenderPass = util::Managed(
			util::createRenderPassMultiView(context->get_device(), 0b00111111, format),
			[dev = context->get_device()](VkRenderPass& pass) { vkDestroyRenderPass(dev, pass, nullptr); });

		{
			auto tPipeline = util::createGraphicsPipeline(context->get_device(), irPipelineLayout.get(),
														  irRenderPass.get(), {dim, dim}, info.vert_shader,
														  info.frag_shader, {}, VK_CULL_MODE_FRONT_BIT);
			irPipeline = util::Managed(
				tPipeline, [dev = context->get_device()](VkPipeline& pipe) { vkDestroyPipeline(dev, pipe, nullptr); });
		}

		{
			VkFramebuffer fbo = VK_NULL_HANDLE;
			VkFramebufferCreateInfo fbCreateInfo = {};
			fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbCreateInfo.width = dim;
			fbCreateInfo.height = dim;
			fbCreateInfo.layers = 6;
			fbCreateInfo.renderPass = irRenderPass.get();
			fbCreateInfo.attachmentCount = 1;
			fbCreateInfo.pAttachments = &irradianceMap.get_imageView();
			vkCreateFramebuffer(context->get_device(), &fbCreateInfo, nullptr, &fbo);
			irFramebuffer = util::Managed(
				fbo, [dev = context->get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
		}

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

		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool.get();
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = views.data();

			VkDescriptorSet dSet;
			auto result = vkAllocateDescriptorSets(context->get_device(), &allocInfo, &dSet);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
			}
			descriptorSet = util::Managed(dSet, [dev = context->get_device(), pool = descriptorPool.get()](
													VkDescriptorSet& ds) { vkFreeDescriptorSets(dev, pool, 1, &ds); });

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

		CubePushConstantBlock pcb{};

		{
			auto cmdBuffer = context->startCommandBufferRecord();

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
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipelineLayout.get(), 0, 1,
									&info.descriptor, 0, nullptr);
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipelineLayout.get(), 1, 1,
									descriptorSet.data(), 0, nullptr);

			pcb.mvp = glm::mat4(1.0f);
			vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0,
							   sizeof(CubePushConstantBlock), &pcb);
			if (!is_ignore(info.pcb))
			{
				vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT,
								   sizeof(CubePushConstantBlock), sizeof(PCB), &info.pcb);
			}

			VkDeviceSize offsets = {0};

			vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &cube.get_vertexBuffer(), &offsets);
			vkCmdBindIndexBuffer(cmdBuffer, cube.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdBuffer, cube.get_indexCount(), 1, 0, 0, 0);

			vkCmdEndRenderPass(cmdBuffer);
			context->flushCommandBuffer(cmdBuffer);
		}

		auto cmdBuffer = context->startCommandBufferRecord();

		irradianceMap.transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
									 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
									 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		context->flushCommandBuffer(cmdBuffer);

		return irradianceMap;
	}
};
} // namespace blaze::util
