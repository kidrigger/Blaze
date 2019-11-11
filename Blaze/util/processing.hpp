
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>
#include <Context.hpp>
#include <TextureCube.hpp>
#include "DebugTimer.hpp"
#include "util/createFunctions.hpp"

namespace blaze::util
{
	template <typename PCB>
	struct Texture2CubemapInfo
	{
		std::string vert_shader;
		std::string frag_shader;
		VkDescriptorSet descriptor;
		VkDescriptorSetLayout layout;
		uint32_t cube_side{ 512 };
		PCB pcb{};
	};

	struct Ignore {};

	template <typename T>
	constexpr bool is_ignore(const T& _x) { return false; }
	template <>
	constexpr bool is_ignore<Ignore>(const Ignore& _x) { return true; }

	template <typename PCB>
	class Process
	{
	public:

		static TextureCube Process::convertDescriptorToCubemap(const Context& context, const Texture2CubemapInfo<PCB>& info)
		{
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
			idc.access = VK_ACCESS_TRANSFER_WRITE_BIT;
			TextureCube irradianceMap(context, idc, false);

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
				std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
					info.layout
				};

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
				irPipelineLayout = util::Managed(util::createPipelineLayout(context.get_device(), descriptorSetLayouts, pushConstantRanges), [dev = context.get_device()](VkPipelineLayout& lay){vkDestroyPipelineLayout(dev, lay, nullptr); });
			}

			irRenderPass = util::Managed(util::createRenderPass(context.get_device(), format), [dev = context.get_device()](VkRenderPass& pass) { vkDestroyRenderPass(dev, pass, nullptr); });

			{
				auto tPipeline = util::createGraphicsPipeline(context.get_device(), irPipelineLayout.get(), irRenderPass.get(), { dim, dim },
					info.vert_shader, info.frag_shader, {}, VK_CULL_MODE_FRONT_BIT);
				irPipeline = util::Managed(tPipeline, [dev = context.get_device()](VkPipeline& pipe) { vkDestroyPipeline(dev, pipe, nullptr); });
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
				irFramebuffer = util::Managed(fbo, [dev = context.get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
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

			for (int face = 0; face < 6; face++)
			{
				auto cmdBuffer = context.startCommandBufferRecord();

				// RENDERPASSES

				VkRenderPassBeginInfo renderpassBeginInfo = {};
				renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderpassBeginInfo.renderPass = irRenderPass.get();
				renderpassBeginInfo.framebuffer = irFramebuffer.get();
				renderpassBeginInfo.renderArea.offset = { 0, 0 };
				renderpassBeginInfo.renderArea.extent = { dim, dim };

				std::array<VkClearValue, 1> clearColor;
				clearColor[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
				renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
				renderpassBeginInfo.pClearValues = clearColor.data();

				vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipeline.get());
				vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipelineLayout.get(), 0, 1, &info.descriptor, 0, nullptr);

				pcb.mvp = proj * matrices[face];
				vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CubePushConstantBlock), &pcb);
				if (!is_ignore(info.pcb))
				{
					vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(CubePushConstantBlock), sizeof(PCB), &info.pcb);
				}

				VkDeviceSize offsets = { 0 };

				vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &cube.get_vertexBuffer(), &offsets);
				vkCmdBindIndexBuffer(cmdBuffer, cube.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmdBuffer, cube.get_indexCount(), 1, 0, 0, 0);

				vkCmdEndRenderPass(cmdBuffer);

				fbColorAttachment.transferLayout(cmdBuffer,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = face;
				copyRegion.dstSubresource.mipLevel = 0;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(dim);
				copyRegion.extent.height = static_cast<uint32_t>(dim);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(cmdBuffer,
					fbColorAttachment.get_image(), fbColorAttachment.get_imageInfo().imageLayout,
					irradianceMap.get_image(), irradianceMap.get_imageInfo().imageLayout,
					1, &copyRegion);

				fbColorAttachment.transferLayout(cmdBuffer,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

				context.flushCommandBuffer(cmdBuffer);
			}

			auto cmdBuffer = context.startCommandBufferRecord();

			irradianceMap.transferLayout(cmdBuffer,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				0, VK_ACCESS_SHADER_READ_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

			context.flushCommandBuffer(cmdBuffer);

			return irradianceMap;
		}
	};
}
