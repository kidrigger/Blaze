
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "util/Managed.hpp"
#include "util/createFunctions.hpp"
#include "Datatypes.hpp"
#include "UniformBuffer.hpp"
#include "Drawable.hpp"
#include "Camera.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const int32_t POINT_SHADOW_MAP_SIZE = 512;
const int32_t DIR_SHADOW_MAP_SIZE = 2048;

namespace blaze
{
	class ShadowCaster;
	class PointShadow
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
		PointShadow() noexcept
		{
		}

		PointShadow(const Context& context, VkRenderPass renderPass) noexcept
		{
			ImageDataCube idc{};
			idc.height = POINT_SHADOW_MAP_SIZE;
			idc.width = POINT_SHADOW_MAP_SIZE;
			idc.numChannels = 1;
			idc.size = 6 * POINT_SHADOW_MAP_SIZE * POINT_SHADOW_MAP_SIZE;
			idc.layerSize = POINT_SHADOW_MAP_SIZE * POINT_SHADOW_MAP_SIZE;
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
				POINT_SHADOW_MAP_SIZE,
				POINT_SHADOW_MAP_SIZE,
				-POINT_SHADOW_MAP_SIZE,
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
				fbCreateInfo.width = POINT_SHADOW_MAP_SIZE;
				fbCreateInfo.height = POINT_SHADOW_MAP_SIZE;
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

	class DirectionalShadow
	{
	public:
		float nearPlane{ 0.1f };
		float farPlane{ 512.0f };
		float width{ 512 };
		float height{ 512 };
		glm::vec3 position{ 0.0f };
		glm::vec3 direction{ 0.0f };

	private:
		Texture2D shadowMap;
		util::Managed<VkFramebuffer> framebuffer;
		util::Unmanaged<VkViewport> viewport;

	public:
		DirectionalShadow() noexcept
		{
		}

		DirectionalShadow(const Context& context, VkRenderPass renderPass) noexcept
		{
			ImageData2D idc{};
			idc.height = DIR_SHADOW_MAP_SIZE;
			idc.width = DIR_SHADOW_MAP_SIZE;
			idc.numChannels = 1;
			idc.size = DIR_SHADOW_MAP_SIZE * DIR_SHADOW_MAP_SIZE;
			idc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			idc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			idc.format = VK_FORMAT_D32_SFLOAT;
			idc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			shadowMap = Texture2D(context, idc, false);

			viewport = VkViewport{
				0.0f,
				DIR_SHADOW_MAP_SIZE,
				DIR_SHADOW_MAP_SIZE,
				-DIR_SHADOW_MAP_SIZE,
				0.0f,
				1.0f
			};

			{
				std::vector<VkImageView> attachments = {
					shadowMap.get_imageView()
				};

				VkFramebuffer fbo = VK_NULL_HANDLE;
				VkFramebufferCreateInfo fbCreateInfo = {};
				fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				fbCreateInfo.width = DIR_SHADOW_MAP_SIZE;
				fbCreateInfo.height = DIR_SHADOW_MAP_SIZE;
				fbCreateInfo.layers = 1;
				fbCreateInfo.renderPass = renderPass;
				fbCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
				fbCreateInfo.pAttachments = attachments.data();
				vkCreateFramebuffer(context.get_device(), &fbCreateInfo, nullptr, &fbo);
				framebuffer = util::Managed(fbo, [dev = context.get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
			}
		}

		const VkFramebuffer& get_framebuffer() const { return framebuffer.get(); }
		const VkViewport& get_viewport() const { return viewport.get(); }
		const Texture2D& get_shadowMap() const { return shadowMap; }
		Texture2D& get_shadowMap() { return shadowMap; }

		friend class ShadowCaster;
	};

	class ShadowHandler
	{
	public:
		using ShadowHandle = int32_t;
	private:
		std::vector<PointShadow> pointShadows;
		std::vector<ShadowHandle> freeStack;
		std::vector<bool> handleValidity;
	public:
		ShadowHandler()
		{

		}
	};

	class ShadowCaster
	{
	public:
		const VkFormat format{ VK_FORMAT_R32_SFLOAT };

		using ShadowHandle = int32_t;
		using LightHandle = uint32_t;
	private:
		util::Managed<VkRenderPass> renderPassOmni;
		util::Managed<VkRenderPass> renderPassDirectional;
		util::Managed<VkPipelineLayout> pipelineLayout;
		util::Managed<VkPipeline> pipelineOmni;
		util::Managed<VkPipeline> pipelineDirectional;
		util::Managed<VkDescriptorPool> dsPool;
		util::Managed<VkDescriptorSetLayout> dsLayout;
		util::Managed<VkDescriptorSetLayout> shadowLayout;
		util::Unmanaged<VkDescriptorSet> uboDescriptorSet;
		util::Unmanaged<VkDescriptorSet> shadowDescriptorSet;
		UniformBuffer<ShadowUniformBufferObject> viewsUBO;

		static const uint32_t LIGHT_MASK_INDEX = 0xF0FFFFFF;
		static const uint32_t LIGHT_MASK_TYPE  = 0x0F000000;
		static const uint32_t LIGHT_TYPE_POINT = 0x01000000;
		static const uint32_t LIGHT_TYPE_DIR   = 0x02000000;

		std::vector<PointShadow> pointShadows;
		std::vector<ShadowHandle> pointShadowFreeStack;
		std::vector<bool> pointShadowHandleValidity;

		std::vector<DirectionalShadow> dirShadows;
		std::vector<ShadowHandle> dirShadowFreeStack;
		std::vector<bool> dirShadowHandleValidity;

		LightsUniformBufferObject lightsData;
		uint32_t MAX_POINT_SHADOWS;
		uint32_t MAX_DIR_SHADOWS;
		uint32_t MAX_POINT_LIGHTS;
		uint32_t MAX_DIR_LIGHTS;

	public:
		ShadowCaster() noexcept
		{
		}

		ShadowCaster(const Context& context, uint32_t maxLights = 16, uint32_t maxShadows = 16) noexcept
			: MAX_POINT_LIGHTS(maxLights),
			MAX_DIR_LIGHTS(1),
			MAX_POINT_SHADOWS(maxShadows),
			MAX_DIR_SHADOWS(1)
		{
			using namespace util;

			assert(MAX_POINT_LIGHTS <= 16);
			assert(MAX_DIR_LIGHTS <= 4);
			memset(&lightsData, 0, sizeof(lightsData));
			memset(lightsData.shadowIdx, -1, sizeof(lightsData.shadowIdx));

			pointShadowHandleValidity = std::vector<bool>(MAX_POINT_SHADOWS, false);
			pointShadowFreeStack.reserve(MAX_POINT_SHADOWS);
			for (uint32_t i = 0; i < MAX_POINT_SHADOWS; i++)
			{
				pointShadowFreeStack.push_back(MAX_POINT_SHADOWS - i - 1);
			}

			dirShadowHandleValidity = std::vector<bool>(MAX_DIR_SHADOWS, false);
			dirShadowFreeStack.reserve(MAX_DIR_SHADOWS);
			for (uint32_t i = 0; i < MAX_DIR_SHADOWS; i++)
			{
				dirShadowFreeStack.push_back(MAX_DIR_SHADOWS - i - 1);
			}

			renderPassOmni = Managed(createRenderPassMultiView(context.get_device(), 0b00111111, format, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL), [dev = context.get_device()](VkRenderPass& rp){ vkDestroyRenderPass(dev, rp, nullptr); });
			renderPassDirectional = Managed(createShadowRenderPass(context.get_device()), [dev = context.get_device()](VkRenderPass& rp){ vkDestroyRenderPass(dev, rp, nullptr); });

			viewsUBO = UniformBuffer(context, createOmniShadowUBO());

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
							MAX_POINT_SHADOWS + MAX_DIR_SHADOWS
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
							MAX_POINT_SHADOWS,
							VK_SHADER_STAGE_FRAGMENT_BIT,
							nullptr
						},
						{
							1,
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							MAX_DIR_SHADOWS,
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

				pipelineOmni = Managed(
					createGraphicsPipeline(context.get_device(), pipelineLayout.get(), renderPassOmni.get(),
						{ POINT_SHADOW_MAP_SIZE, POINT_SHADOW_MAP_SIZE }, "shaders/vShadow.vert.spv", "shaders/fShadow.frag.spv",
						{ VK_DYNAMIC_STATE_VIEWPORT }, VK_CULL_MODE_FRONT_BIT, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS),
					[dev = context.get_device()](VkPipeline& pipe){ vkDestroyPipeline(dev, pipe, nullptr); });

				pipelineDirectional = Managed(
					createGraphicsPipeline(context.get_device(), pipelineLayout.get(), renderPassDirectional.get(),
						{ DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE }, "shaders/vDirShadow.vert.spv", "shaders/fDirShadow.frag.spv",
						{ VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_DEPTH_BIAS }, VK_CULL_MODE_FRONT_BIT, VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS),
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

				for (uint32_t i = 0; i < MAX_POINT_SHADOWS; i++)
				{
					pointShadows.emplace_back(context, renderPassOmni.get());
				}
				for (uint32_t i = 0; i < MAX_DIR_SHADOWS; i++)
				{
					dirShadows.emplace_back(context, renderPassDirectional.get());
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

					std::vector<VkDescriptorImageInfo> pointImageInfos;
					for (auto& shade : pointShadows)
					{
						pointImageInfos.push_back(shade.get_shadowMap().get_imageInfo());
					}

					std::array<VkWriteDescriptorSet, 2> writes;

					writes[0] = {};
					writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writes[0].descriptorCount = MAX_POINT_SHADOWS;
					writes[0].dstSet = descriptorSet;
					writes[0].dstBinding = 0;
					writes[0].dstArrayElement = 0;
					writes[0].pImageInfo = pointImageInfos.data();

					std::vector<VkDescriptorImageInfo> dirImageInfos;
					for (auto& shade : dirShadows)
					{
						dirImageInfos.push_back(shade.get_shadowMap().get_imageInfo());
					}

					writes[1] = {};
					writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
					writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					writes[1].descriptorCount = MAX_DIR_SHADOWS;
					writes[1].dstSet = descriptorSet;
					writes[1].dstBinding = 1;
					writes[1].dstArrayElement = 0;
					writes[1].pImageInfo = dirImageInfos.data();

					vkUpdateDescriptorSets(context.get_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
				}
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

		ShadowCaster(ShadowCaster&& other) noexcept
			: renderPassOmni(std::move(other.renderPassOmni)),
			renderPassDirectional(std::move(other.renderPassDirectional)),
			pipelineLayout(std::move(other.pipelineLayout)),
			pipelineOmni(std::move(other.pipelineOmni)),
			pipelineDirectional(std::move(other.pipelineDirectional)),
			dsPool(std::move(other.dsPool)),
			dsLayout(std::move(other.dsLayout)),
			uboDescriptorSet(std::move(other.uboDescriptorSet)),
			shadowDescriptorSet(std::move(other.shadowDescriptorSet)),
			shadowLayout(std::move(other.shadowLayout)),
			viewsUBO(std::move(other.viewsUBO)),
			pointShadows(std::move(other.pointShadows)),
			pointShadowFreeStack(std::move(other.pointShadowFreeStack)),
			pointShadowHandleValidity(std::move(other.pointShadowHandleValidity)),
			dirShadows(std::move(other.dirShadows)),
			dirShadowFreeStack(std::move(other.dirShadowFreeStack)),
			dirShadowHandleValidity(std::move(other.dirShadowHandleValidity)),
			MAX_POINT_SHADOWS(other.MAX_POINT_SHADOWS),
			MAX_DIR_SHADOWS(other.MAX_DIR_SHADOWS),
			MAX_POINT_LIGHTS(other.MAX_POINT_LIGHTS),
			MAX_DIR_LIGHTS(other.MAX_DIR_LIGHTS)
		{
			memcpy(&lightsData, &other.lightsData, sizeof(LightsUniformBufferObject));
		}

		ShadowCaster& operator=(ShadowCaster&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			renderPassOmni = std::move(other.renderPassOmni);
			renderPassDirectional = std::move(other.renderPassDirectional);
			pipelineLayout = std::move(other.pipelineLayout);
			pipelineOmni = std::move(other.pipelineOmni);
			pipelineDirectional = std::move(other.pipelineDirectional);
			dsPool = std::move(other.dsPool);
			dsLayout = std::move(other.dsLayout);
			uboDescriptorSet = std::move(other.uboDescriptorSet);
			shadowDescriptorSet = std::move(other.shadowDescriptorSet);
			shadowLayout = std::move(other.shadowLayout);
			viewsUBO = std::move(other.viewsUBO);
			pointShadows = std::move(other.pointShadows);
			pointShadowFreeStack = std::move(other.pointShadowFreeStack);
			pointShadowHandleValidity = std::move(other.pointShadowHandleValidity);
			dirShadows = std::move(other.dirShadows);
			dirShadowFreeStack = std::move(other.dirShadowFreeStack);
			dirShadowHandleValidity = std::move(other.dirShadowHandleValidity);
			memcpy(&lightsData, &other.lightsData, sizeof(LightsUniformBufferObject));
			MAX_POINT_SHADOWS = other.MAX_POINT_SHADOWS;
			MAX_DIR_SHADOWS = other.MAX_DIR_SHADOWS;
			MAX_POINT_LIGHTS = other.MAX_POINT_LIGHTS;
			MAX_DIR_LIGHTS = other.MAX_DIR_LIGHTS;
			return *this;
		}

		ShadowCaster(const ShadowCaster& other) = delete;
		ShadowCaster& operator=(const ShadowCaster& other) = delete;

		LightHandle addPointLight(const glm::vec3& position, float brightness, bool hasShadow)
		{
			if (lightsData.numPointLights >= (int)MAX_POINT_LIGHTS)
			{
				throw std::runtime_error("Max Light Count Reached.");
			}
			auto handle = lightsData.numPointLights;
			lightsData.lightPos[handle] = glm::vec4(position, brightness);
			if (hasShadow)
			{
				lightsData.shadowIdx[handle] = createPointShadow(position);
			}
			else
			{
				lightsData.shadowIdx[handle] = -1;
			}
			lightsData.numPointLights++;
			return handle | LIGHT_TYPE_POINT;
		}

		LightHandle addDirLight(const glm::vec3& position, const glm::vec3& direction, float brightness, bool hasShadow)
		{
			if (lightsData.numDirLights >= (int)MAX_DIR_LIGHTS)
			{
				throw std::runtime_error("Max Light Count Reached.");
			}
			auto handle = lightsData.numDirLights;
			lightsData.lightDir[handle] = glm::vec4(direction, brightness);
			if (hasShadow)
			{
				auto shade = createDirShadow(position, direction);
				assert(shade == handle);
			}
			else
			{
				lightsData.shadowIdx[handle] = -1;
			}
			lightsData.numDirLights++;
			return handle | LIGHT_TYPE_DIR;
		}

		void setLightPosition(LightHandle handle, const glm::vec3& position)
		{
			assert((handle & LIGHT_MASK_TYPE) == LIGHT_TYPE_POINT);
			int handleIdx = handle & LIGHT_MASK_INDEX;
			if (handleIdx >= lightsData.numPointLights)
			{
				throw std::out_of_range("Invalid Light Handle.");
			}
			memcpy(&lightsData.lightPos[handleIdx], &position[0], sizeof(glm::vec3));
			ShadowHandle shade = lightsData.shadowIdx[handleIdx];
			if (shade >= 0)
			{
				pointShadows[shade].position = lightsData.lightPos[handleIdx];
			}
		}

		void setLightBrightness(LightHandle handle, float brightness)
		{
			int handleIdx = handle & LIGHT_MASK_INDEX;
			switch (handle & LIGHT_MASK_TYPE)
			{
			case LIGHT_TYPE_POINT:
			{
				if (handleIdx >= lightsData.numPointLights)
				{
					throw std::out_of_range("Invalid Light Handle.");
				}
				lightsData.lightPos[handleIdx][3] = brightness;
			};
			break;
			case LIGHT_TYPE_DIR:
			{
				if (handleIdx >= lightsData.numDirLights)
				{
					throw std::out_of_range("Invalid Light Handle.");
				}
				lightsData.lightDir[handleIdx][3] = brightness;
			};
			break;
			}
		}

		const LightsUniformBufferObject& getLightsData() const { return lightsData; }

		void bind(VkCommandBuffer cmdBuffer, VkPipelineLayout layout, uint32_t set) const
		{
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, 1, &shadowDescriptorSet.get(), 0, nullptr);
		}

		ShadowHandle createPointShadow(const glm::vec3& position = glm::vec3(0.0f), float nearPlane = 1.0f, float farPlane = 512.0f)
		{
			if (pointShadowFreeStack.empty())
			{
				throw std::runtime_error("Max shadows reached.");
			}
			auto handle = pointShadowFreeStack.back();
			pointShadowFreeStack.pop_back();
			pointShadowHandleValidity[handle] = true;
			pointShadows[handle].position = position;
			pointShadows[handle].nearPlane = nearPlane;
			pointShadows[handle].farPlane = farPlane;
			return handle;
		}

		ShadowHandle createDirShadow(const glm::vec3& position = glm::vec3(0.0f), const glm::vec3& direction = glm::vec3(0,-1,0), float width = 64.0f, float height = 64.0f, float nearPlane = 1.0f, float  farPlane = 512.0f)
		{
			if (dirShadowFreeStack.empty())
			{
				throw std::runtime_error("Max shadows reached.");
			}
			auto handle = dirShadowFreeStack.back();
			dirShadowFreeStack.pop_back();
			dirShadowHandleValidity[handle] = true;
			dirShadows[handle].position = position;
			dirShadows[handle].direction = direction;
			dirShadows[handle].width = width;
			dirShadows[handle].height = height;
			dirShadows[handle].nearPlane = nearPlane;
			dirShadows[handle].farPlane = farPlane;
			return handle;
		}

		/*
		void freeShadow(ShadowHandle handle)
		{
			if (handle < 0)
			{
				throw std::out_of_range("Invalid Shadow Handle.");
			}

			auto shadowType = handle & SHADOW_HANDLE_TYPE_MASK;
			auto handleIdx = handle & SHADOW_HANDLE_INDEX_MASK;
			
			switch (shadowType)
			{
			case SHADOW_HANDLE_POINT_TYPE:
			{
				if (!pointShadowHandleValidity[handleIdx])
				{
					throw std::out_of_range("Invalid Shadow Handle.");
				}
				pointShadowFreeStack.push_back(handle);
				pointShadowHandleValidity[handleIdx] = false;
			}; break;
			case SHADOW_HANDLE_DIRECTIONAL_TYPE:
			{
				if (!dirShadowHandleValidity[handleIdx])
				{
					throw std::out_of_range("Invalid Shadow Handle.");
				}
				dirShadowFreeStack.push_back(handle);
				dirShadowHandleValidity[handleIdx] = false;
			}; break;
			default:
			{
				throw std::runtime_error("Unknown Handle");
			}
			}
		}
		*/

		void cast(const Context& context, Camera* camera, VkCommandBuffer cmdBuffer, const std::vector<Drawable*>& drawables)
		{
			for (ShadowHandle handle : lightsData.shadowIdx)
			{
				if (handle < 0) continue;
				cast(context, handle, pointShadows[handle], cmdBuffer, drawables);
			}

			for (ShadowHandle i = 0; i < lightsData.numDirLights; i++)
			{
				cast(context, i, dirShadows[i], camera, cmdBuffer, drawables);
			}
		}

		void cast(const Context& context, ShadowHandle handle, PointShadow& shadow, VkCommandBuffer cmdBuffer, const std::vector<Drawable*>& drawables)
		{
			auto shadowPCB = createOmniShadowPCB(shadow);

			VkRenderPassBeginInfo renderpassBeginInfo = {};
			renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderpassBeginInfo.renderPass = renderPassOmni.get();
			renderpassBeginInfo.framebuffer = shadow.framebuffer.get();
			renderpassBeginInfo.renderArea.offset = { 0, 0 };
			renderpassBeginInfo.renderArea.extent = { POINT_SHADOW_MAP_SIZE,POINT_SHADOW_MAP_SIZE };

			std::array<VkClearValue, 2> clearColor;
			clearColor[0].color = { 1000.0f, 0.0f, 0.0f, 1.0f };
			clearColor[1].depthStencil = { 1.0f, 0 };
			renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
			renderpassBeginInfo.pClearValues = clearColor.data();

			vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineOmni.get());
			vkCmdSetViewport(cmdBuffer, 0, 1, &shadow.viewport.get());
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, uboDescriptorSet.data(), 0, nullptr);
			vkCmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(ModelPushConstantBlock), sizeof(ShadowPushConstantBlock), &shadowPCB);

			for (Drawable* d : drawables)
			{
				d->drawGeometry(cmdBuffer, pipelineLayout.get());
			}

			vkCmdEndRenderPass(cmdBuffer);
		}

		void cast(const Context& context, ShadowHandle handle, DirectionalShadow& shadow, Camera* cam, VkCommandBuffer cmdBuffer, const std::vector<Drawable*>& drawables)
		{
			auto shadowPCB = createDirShadowPCB(shadow, cam);
			lightsData.dirLightTransform[handle] = shadowPCB.projection;

			VkRenderPassBeginInfo renderpassBeginInfo = {};
			renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderpassBeginInfo.renderPass = renderPassDirectional.get();
			renderpassBeginInfo.framebuffer = shadow.framebuffer.get();
			renderpassBeginInfo.renderArea.offset = { 0, 0 };
			renderpassBeginInfo.renderArea.extent = { DIR_SHADOW_MAP_SIZE, DIR_SHADOW_MAP_SIZE };

			std::array<VkClearValue, 1> clearColor;
			clearColor[0].depthStencil = { 1.0f, 0 };
			renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
			renderpassBeginInfo.pClearValues = clearColor.data();

			vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineDirectional.get());
			vkCmdSetDepthBias(cmdBuffer, 1.25f, 0.0f, 1.75f);
			vkCmdSetViewport(cmdBuffer, 0, 1, &shadow.viewport.get());
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, uboDescriptorSet.data(), 0, nullptr);
			vkCmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(ModelPushConstantBlock), sizeof(ShadowPushConstantBlock), &shadowPCB);

			for (Drawable* d : drawables)
			{
				d->drawGeometry(cmdBuffer, pipelineLayout.get());
			}

			vkCmdEndRenderPass(cmdBuffer);

			shadow.get_shadowMap().transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}

		void setShadowClipPlanes(ShadowHandle handle, float nearPlane, float farPlane)
		{
			if (!pointShadowHandleValidity[handle])
			{
				throw std::out_of_range("Invalid Shadow Handle.");
			}
			pointShadows[handle].nearPlane = nearPlane;
			pointShadows[handle].farPlane = farPlane;
		}

		void setShadowPosition(ShadowHandle handle, const glm::vec3& position)
		{
			auto handleIdx = handle;
			if (!pointShadowHandleValidity[handleIdx])
			{
				throw std::out_of_range("Invalid Shadow Handle.");
			}
			pointShadows[handleIdx].position = position;
		}

		const VkRenderPass& get_renderPass() const { return renderPassOmni.get(); }
		const VkDescriptorSetLayout& get_shadowLayout() const { return shadowLayout.get(); }

	private:
		ShadowPushConstantBlock createDirShadowPCB(const DirectionalShadow& shadow, Camera* camera) const
		{
			glm::vec4 cube[] = {
				{-1,-1,-1,1},
				{1,-1,-1,1},
				{1,1,-1,1},
				{-1,1,-1,1},
				{-1,1,1,1},
				{1,1,1,1},
				{1,-1,1,1},
				{-1,-1,1,1}
			};
			glm::mat4 invProj = glm::inverse(camera->get_projection() * camera->get_view());

			glm::vec3 bz = glm::normalize(shadow.direction);
			glm::vec3 bx = glm::normalize(glm::cross(bz, glm::vec3(0, 1, 0)));
			glm::vec3 by = glm::cross(bx, bz);

			glm::mat3 basis = glm::transpose(glm::mat3(bx, by, bz));
			glm::vec3 center = glm::vec3(0.0f);
			for (auto& vec : cube)
			{
				vec = invProj * vec;
				vec /= vec.w;
				vec = glm::vec4(basis * glm::vec3(vec), 1.0f);
				center += glm::vec3(vec);
			}
			center *= 0.125;

			glm::vec3 v = glm::vec3(-1);
			for (auto& vec : cube)
			{
				v = glm::max(glm::abs(glm::vec3(vec) - center), v);
			}

			return
			{
				glm::ortho(-v.x, v.x, -v.y, v.y, shadow.nearPlane, shadow.farPlane) * glm::lookAt(center + 2.0f * bz * v.z - bz * (shadow.nearPlane + shadow.farPlane), center, glm::vec3(0,1,0)),
				shadow.direction
			};
		}
		ShadowPushConstantBlock createOmniShadowPCB(const PointShadow& shadow) const
		{
			return
			{
				glm::perspective(glm::radians(90.0f), 1.0f, shadow.nearPlane, shadow.farPlane),
				shadow.position
			};
		}
		ShadowUniformBufferObject createOmniShadowUBO() const
		{
			return
			{
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
