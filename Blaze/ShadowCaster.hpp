
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "util/Managed.hpp"
#include "util/createFunctions.hpp"
#include "Datatypes.hpp"
#include "UniformBuffer.hpp"
#include "Drawable.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const int32_t SHADOW_MAP_SIZE = 512;

namespace blaze
{
	class ShadowCaster;
	class Shadow
	{
	public:
		float nearPlane{ 0.1f };
		float farPlane{ 512.0f };
		glm::vec3 position{ 0.0f };

	private:
		TextureCube shadowMap;
		TextureCube depthMap;
		util::Managed<VkFramebuffer> framebuffer;
		util::Unmanaged<VkViewport> viewport;

	public:
		Shadow() noexcept
		{
		}

		Shadow(const Context& context, VkRenderPass renderPass) noexcept
		{
			ImageDataCube idc{};
			idc.height = SHADOW_MAP_SIZE;
			idc.width = SHADOW_MAP_SIZE;
			idc.numChannels = 1;
			idc.size = 6 * SHADOW_MAP_SIZE * SHADOW_MAP_SIZE;
			idc.layerSize = SHADOW_MAP_SIZE * SHADOW_MAP_SIZE;
			idc.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			idc.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			idc.format = VK_FORMAT_R32_SFLOAT;
			idc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
			shadowMap = TextureCube(context, idc, false);

			idc.usage ^= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			idc.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			idc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			idc.format = VK_FORMAT_D32_SFLOAT;
			idc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			depthMap = TextureCube(context, idc, false);

			viewport = VkViewport{
				0.0f,
				SHADOW_MAP_SIZE,
				SHADOW_MAP_SIZE,
				-SHADOW_MAP_SIZE,
				0.0f,
				1.0f
			};

			{
				std::vector<VkImageView> attachments = {
					shadowMap.get_imageView(),
					depthMap.get_imageView()
				};

				VkFramebuffer fbo = VK_NULL_HANDLE;
				VkFramebufferCreateInfo fbCreateInfo = {};
				fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				fbCreateInfo.width = SHADOW_MAP_SIZE;
				fbCreateInfo.height = SHADOW_MAP_SIZE;
				fbCreateInfo.layers = 6;
				fbCreateInfo.renderPass = renderPass;
				fbCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
				fbCreateInfo.pAttachments = attachments.data();
				vkCreateFramebuffer(context.get_device(), &fbCreateInfo, nullptr, &fbo);
				framebuffer = util::Managed(fbo, [dev = context.get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
			}
		}

		const VkFramebuffer& get_framebuffer() const { return framebuffer.get(); }
		const VkViewport& get_viewport() const { return viewport.get(); }
		const TextureCube& get_shadowMap() const { return shadowMap; }
		TextureCube& get_shadowMap() { return shadowMap; }

		friend class ShadowCaster;
	};

	class ShadowCaster
	{
	public:
		const VkFormat format{ VK_FORMAT_R32_SFLOAT };

		using ShadowHandle = int32_t;
		using LightHandle = uint32_t;
	private:
		util::Managed<VkRenderPass> renderPass;
		util::Managed<VkPipelineLayout> pipelineLayout;
		util::Managed<VkPipeline> pipeline;
		util::Managed<VkDescriptorPool> dsPool;
		util::Managed<VkDescriptorSetLayout> dsLayout;
		util::Managed<VkDescriptorSetLayout> shadowLayout;
		util::Unmanaged<VkDescriptorSet> uboDescriptorSet;
		util::Unmanaged<VkDescriptorSet> shadowDescriptorSet;
		UniformBuffer<ShadowUniformBufferObject> viewsUBO;
		std::vector<Shadow> shadows;
		std::vector<ShadowHandle> freeStack;
		std::vector<bool> handleValidity;
		LightsUniformBufferObject lightsData;
		uint32_t MAX_SHADOWS;
		uint32_t MAX_LIGHTS;

	public:
		ShadowCaster() noexcept
		{
		}

		ShadowCaster(const Context& context, uint32_t maxLights = 16, uint32_t maxShadows = 16) noexcept
			: MAX_LIGHTS(maxLights),
			MAX_SHADOWS(maxShadows)
		{
			using namespace util;

			assert(MAX_LIGHTS <= 16);
			memset(&lightsData, 0, sizeof(lightsData));
			memset(lightsData.shadowIdx, -1, sizeof(lightsData.shadowIdx));

			renderPass = Managed(createRenderPassMultiView(context.get_device(), 0b00111111, format, VK_FORMAT_D32_SFLOAT), [dev = context.get_device()](VkRenderPass& rp){ vkDestroyRenderPass(dev, rp, nullptr); });

			viewsUBO = UniformBuffer(context, createOmniShadowUBO());
			
			handleValidity = std::vector<bool>(MAX_SHADOWS, false);
			freeStack.reserve(MAX_SHADOWS);
			for (uint32_t i = 0; i < MAX_SHADOWS; i++)
			{
				freeStack.push_back(MAX_SHADOWS - i - 1);
			}

			try
			{
				{
					std::vector<VkDescriptorPoolSize> poolSizes = {
						{
							VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							1
						},
						{
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							MAX_SHADOWS
						}
					};
					dsPool = util::Managed(util::createDescriptorPool(context.get_device(), poolSizes, 17), [dev = context.get_device()](VkDescriptorPool& descPool){vkDestroyDescriptorPool(dev, descPool, nullptr); });
					std::vector<VkDescriptorSetLayoutBinding> bindings = {
						{
							0,
							VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							1,
							VK_SHADER_STAGE_VERTEX_BIT,
							nullptr
						}
					};

					dsLayout = util::Managed(util::createDescriptorSetLayout(context.get_device(), bindings), [dev = context.get_device()](VkDescriptorSetLayout& lay){vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
				
					bindings = {
						{
							0,
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							MAX_SHADOWS,
							VK_SHADER_STAGE_FRAGMENT_BIT,
							nullptr
						}
					};
					shadowLayout = util::Managed(util::createDescriptorSetLayout(context.get_device(), bindings), [dev = context.get_device()](VkDescriptorSetLayout& lay){vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
				}

				{
					std::vector<VkDescriptorSetLayout> descriptorLayouts = {
						dsLayout.get()
					};
					std::vector<VkPushConstantRange> pushConstantRanges = {
						{
							VK_SHADER_STAGE_VERTEX_BIT,
							0,
							sizeof(ModelPushConstantBlock) + sizeof(ShadowPushConstantBlock)
						}
					};
					pipelineLayout = Managed(createPipelineLayout(context.get_device(), descriptorLayouts, pushConstantRanges), [dev = context.get_device()](VkPipelineLayout& lay){vkDestroyPipelineLayout(dev, lay, nullptr); });
				}

				pipeline = Managed(
					createGraphicsPipeline(context.get_device(), pipelineLayout.get(), renderPass.get(),
						{ SHADOW_MAP_SIZE, SHADOW_MAP_SIZE }, "shaders/vShadow.vert.spv", "shaders/fShadow.frag.spv",
						{ VK_DYNAMIC_STATE_VIEWPORT }, VK_CULL_MODE_FRONT_BIT, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS),
					[dev = context.get_device()](VkPipeline& pipe){ vkDestroyPipeline(dev, pipe, nullptr); });

				{
					VkDescriptorSetAllocateInfo allocInfo = {};
					allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
					allocInfo.descriptorPool = dsPool.get();
					allocInfo.descriptorSetCount = 1;
					allocInfo.pSetLayouts = dsLayout.data();

					VkDescriptorSet dSet;
					auto result = vkAllocateDescriptorSets(context.get_device(), &allocInfo, &dSet);
					if (result != VK_SUCCESS)
					{
						std::cerr << "Descriptor Set allocation failed with " << std::to_string(result) << std::endl;
					}
					uboDescriptorSet = dSet;

					VkDescriptorBufferInfo info = {};
					info.buffer = viewsUBO.get_buffer();
					info.offset = 0;
					info.range = sizeof(ShadowUniformBufferObject);

					VkWriteDescriptorSet write = {};
					write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					write.descriptorCount = 1;
					write.dstSet = uboDescriptorSet.get();
					write.dstBinding = 0;
					write.dstArrayElement = 0;
					write.pBufferInfo = &info;

					vkUpdateDescriptorSets(context.get_device(), 1, &write, 0, nullptr);
					viewsUBO.write(context, createOmniShadowUBO());
				}

				shadows = std::vector<Shadow>();
				for (uint32_t i = 0; i < MAX_SHADOWS; i++)
				{
					shadows.emplace_back(context, renderPass.get());
				}

				{
					VkDescriptorSetAllocateInfo allocInfo = {};
					allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
					allocInfo.descriptorPool = dsPool.get();
					allocInfo.descriptorSetCount = 1;
					allocInfo.pSetLayouts = &shadowLayout.get();

					VkDescriptorSet descriptorSet;
					auto result = vkAllocateDescriptorSets(context.get_device(), &allocInfo, &descriptorSet);
					if (result != VK_SUCCESS)
					{
						throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
					}
					shadowDescriptorSet = descriptorSet;

					std::vector<VkDescriptorImageInfo> imageInfos;
					for (auto& shade : shadows)
					{
						imageInfos.push_back(shade.get_shadowMap().get_imageInfo());
					}

					VkWriteDescriptorSet write = {};
					write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					write.descriptorCount = MAX_SHADOWS;
					write.dstSet = descriptorSet;
					write.dstBinding = 0;
					write.dstArrayElement = 0;
					write.pImageInfo = imageInfos.data();

					vkUpdateDescriptorSets(context.get_device(), 1, &write, 0, nullptr);
				}
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

		ShadowCaster(ShadowCaster&& other) noexcept
			: renderPass(std::move(other.renderPass)),
			pipelineLayout(std::move(other.pipelineLayout)),
			pipeline(std::move(other.pipeline)),
			dsPool(std::move(other.dsPool)),
			dsLayout(std::move(other.dsLayout)),
			uboDescriptorSet(std::move(other.uboDescriptorSet)),
			shadowDescriptorSet(std::move(other.shadowDescriptorSet)),
			shadowLayout(std::move(other.shadowLayout)),
			viewsUBO(std::move(other.viewsUBO)),
			shadows(std::move(other.shadows)),
			freeStack(std::move(other.freeStack)),
			handleValidity(std::move(other.handleValidity)),
			MAX_SHADOWS(other.MAX_SHADOWS),
			MAX_LIGHTS(other.MAX_LIGHTS)
		{
			memcpy(&lightsData, &other.lightsData, sizeof(LightsUniformBufferObject));
		}

		ShadowCaster& operator=(ShadowCaster&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			renderPass = std::move(other.renderPass);
			pipelineLayout = std::move(other.pipelineLayout);
			pipeline = std::move(other.pipeline);
			dsPool = std::move(other.dsPool);
			dsLayout = std::move(other.dsLayout);
			uboDescriptorSet = std::move(other.uboDescriptorSet);
			shadowDescriptorSet = std::move(other.shadowDescriptorSet);
			shadowLayout = std::move(other.shadowLayout);
			viewsUBO = std::move(other.viewsUBO);
			shadows = std::move(other.shadows);
			freeStack = std::move(other.freeStack);
			handleValidity = std::move(other.handleValidity);
			memcpy(&lightsData, &other.lightsData, sizeof(LightsUniformBufferObject));
			MAX_SHADOWS = other.MAX_SHADOWS;
			MAX_LIGHTS = other.MAX_LIGHTS;
			return *this;
		}

		ShadowCaster(const ShadowCaster& other) = delete;
		ShadowCaster& operator=(const ShadowCaster& other) = delete;

		LightHandle addLight(const glm::vec3& position, float brightness, bool hasShadow)
		{
			if (lightsData.numLights >= MAX_LIGHTS)
			{
				throw std::runtime_error("Max Light Count Reached.");
			}
			auto handle = lightsData.numLights;
			lightsData.lightPos[handle] = glm::vec4(position, brightness);
			if (hasShadow)
			{
				lightsData.shadowIdx[handle] = createShadow(position);
			}
			else
			{
				lightsData.shadowIdx[handle] = -1;
			}
			lightsData.numLights++;
			return handle;
		}

		void setLightPosition(LightHandle handle, const glm::vec3& position)
		{
			if (handle >= lightsData.numLights)
			{
				throw std::out_of_range("Invalid Light Handle.");
			}
			memcpy(&lightsData.lightPos[handle], &position[0], sizeof(glm::vec3));
			ShadowHandle shade = lightsData.shadowIdx[handle];
			if (shade >= 0)
			{
				shadows[shade].position = lightsData.lightPos[handle];
			}
		}

		void setLightBrightness(LightHandle handle, float brightness)
		{
			if (handle >= lightsData.numLights)
			{
				throw std::out_of_range("Invalid Light Handle.");
			}
			lightsData.lightPos[handle][3] = brightness;
		}

		const LightsUniformBufferObject& getLightsData() const { return lightsData; }

		void bind(VkCommandBuffer cmdBuffer, VkPipelineLayout layout, uint32_t set) const
		{
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, 1, &shadowDescriptorSet.get(), 0, nullptr);
		}

		ShadowHandle createShadow(const glm::vec3& position = glm::vec3(0.0f), float nearPlane = 1.0f, float farPlane = 512.0f)
		{
			if (freeStack.empty())
			{
				throw std::runtime_error("Max shadows reached.");
			}
			auto handle = freeStack.back();
			freeStack.pop_back();
			handleValidity[handle] = true;
			shadows[handle].position = position;
			shadows[handle].nearPlane = nearPlane;
			shadows[handle].farPlane = farPlane;
			return handle;
		}

		void freeShadow(ShadowHandle handle)
		{
			if (!handleValidity[handle])
			{
				throw std::out_of_range("Invalid Shadow Handle.");
			}
			freeStack.push_back(handle);
			handleValidity[handle] = false;
		}

		void cast(const Context& context, VkCommandBuffer cmdBuffer, const std::vector<Drawable*>& drawables)
		{
			for (ShadowHandle handle : lightsData.shadowIdx)
			{
				if (handle < 0) continue;
				cast(context, handle, cmdBuffer, drawables);
			}
		}

		void cast(const Context& context, ShadowHandle handle, VkCommandBuffer cmdBuffer, const std::vector<Drawable*>& drawables)
		{
			if (!handleValidity[handle])
			{
				throw std::out_of_range("Invalid Shadow Handle.");
			}

			shadows[handle].get_shadowMap().transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

			auto shadowPCB = createOmniShadowPCB(shadows[handle]);

			VkRenderPassBeginInfo renderpassBeginInfo = {};
			renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderpassBeginInfo.renderPass = renderPass.get();
			renderpassBeginInfo.framebuffer = shadows[handle].framebuffer.get();
			renderpassBeginInfo.renderArea.offset = { 0, 0 };
			renderpassBeginInfo.renderArea.extent = { SHADOW_MAP_SIZE,SHADOW_MAP_SIZE };

			std::array<VkClearValue, 2> clearColor;
			clearColor[0].color = { 1000.0f, 0.0f, 0.0f, 1.0f };
			clearColor[1].depthStencil = { 1.0f, 0 };
			renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
			renderpassBeginInfo.pClearValues = clearColor.data();

			vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.get());
			vkCmdSetViewport(cmdBuffer, 0, 1, &shadows[handle].viewport.get());
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, uboDescriptorSet.data(), 0, nullptr);
			vkCmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(ModelPushConstantBlock), sizeof(ShadowPushConstantBlock), &shadowPCB);

			for (Drawable* d : drawables)
			{
				d->drawGeometry(cmdBuffer, pipelineLayout.get());
			}

			vkCmdEndRenderPass(cmdBuffer);

			shadows[handle].get_shadowMap().transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}

		void setShadowClipPlanes(ShadowHandle handle, float nearPlane, float farPlane)
		{
			if (!handleValidity[handle])
			{
				throw std::out_of_range("Invalid Shadow Handle.");
			}
			shadows[handle].nearPlane = nearPlane;
			shadows[handle].farPlane = farPlane;
		}

		void setShadowPosition(ShadowHandle handle, const glm::vec3& position)
		{
			if (!handleValidity[handle])
			{
				throw std::out_of_range("Invalid Shadow Handle.");
			}
			shadows[handle].position = position;
		}

		const VkRenderPass& get_renderPass() const { return renderPass.get(); }
		const VkDescriptorSetLayout& get_shadowLayout() const { return shadowLayout.get(); }

	private:
		ShadowPushConstantBlock createOmniShadowPCB(const Shadow& shadow) const
		{
			return {
				glm::perspective(glm::radians(90.0f), 1.0f, shadow.nearPlane, shadow.farPlane),
				shadow.position
			};
		}
		ShadowUniformBufferObject createOmniShadowUBO() const
		{
			return {
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
				}
			};
		}
	};
}
