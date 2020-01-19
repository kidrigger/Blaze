
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "util/Managed.hpp"
#include "util/createFunctions.hpp"
#include "Datatypes.hpp"
#include "UniformBuffer.hpp"
#include "Drawable.hpp"
#include "Camera.hpp"
#include "Context.hpp"
#include "Texture2D.hpp"
#include "TextureCube.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const int32_t POINT_SHADOW_MAP_SIZE = 512;
const int32_t DIR_SHADOW_MAP_SIZE = 2048;

namespace blaze
{
	class ShadowCaster;
	
    /**
     * @class PointShadow
     *
     * @brief Encapsulates the attachments and framebuffer for a point light shadow.
     *
     * Contains the shadowMap (R32) and a depthMap (D32) along with the framebuffer and 
     * viewport configured.
     *
     * @note Not supposed to be used externally.
     *
     * @cond PRIVATE
     */
    class PointShadow
	{
	public:
		float nearPlane{ 0.1f };
		float farPlane{ 20.0f };
		glm::vec3 position{ 0.0f };

	private:
		TextureCube shadowMap;
		TextureCube depthMap;
		util::Managed<VkFramebuffer> framebuffer;
		util::Unmanaged<VkViewport> viewport;

	public:
		PointShadow() noexcept {}

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
			idc.access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            shadowMap = TextureCube(context, idc, false);

			idc.usage ^= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			idc.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			idc.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			idc.format = VK_FORMAT_D32_SFLOAT;
			idc.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            idc.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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
    /**
     * @endcond
     */
    
    /**
     * @class DirShadow
     *
     * @brief Encapsulates the attachments and framebuffer for a directional light shadow.
     *
     * Contains the shadowMap/depthMap (D32) along with the framebuffer and 
     * viewport configured.
     *
     * @note Not supposed to be used externally.
     *
     * @cond PRIVATE
     */
	class DirectionalShadow
	{
	public:
		float nearPlane{ 0.1f };
		float farPlane{ 512.0f };
		float width{ 512 };
		float height{ 512 };
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
            idc.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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
    /**
     * @endcond
     */

    /**
     * @class ShadowCaster
     *
     * @brief The system responsible for lights and shadows.
     *
     * ShadowCaster contains info and buffers related to lights and shadow indices as well
     * as the shadow descriptor set.
     */
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
        /**
         * @fn ShadowCaster()
         *
         * @brief Default empty constructor.
         */
		ShadowCaster() noexcept {}

        /**
         * @fn ShadowCaster(const Context& context, uint32_t maxLights, uint32_t maxShadows)
         *
         * @brief Actual constructor for shadow caster.
         *
         * @param context The currect Vulkan Context
         * @param maxLights The number of point lights to allocate.
         * @param maxShadows The number of shadows to allocate.
         */
		ShadowCaster(const Context& context, uint32_t maxLights = 16, uint32_t maxShadows = 16) noexcept;
	
        /**
         * @name Move Constructors.
         *
         * @brief ShadowCaster is move only. Copy is deleted.
         *
         * @{
         */
		ShadowCaster(ShadowCaster&& other) noexcept;
		ShadowCaster& operator=(ShadowCaster&& other) noexcept;
		ShadowCaster(const ShadowCaster& other) = delete;
		ShadowCaster& operator=(const ShadowCaster& other) = delete;
        /**
         * @}
         */

        /**
         * @fn addPointLight
         *
         * @brief Adds a new point light into the system.
         *
         * @param position Position of the light in the world.
         * @param brightness The brightness of light.
         * @param hasShadow To enable shadows.
         */
		LightHandle addPointLight(const glm::vec3& position, float brightness, bool hasShadow);

        /**
         * @fn addDirLight
         *
         * @brief Adds a new directional light into the system.
         *
         * @param direction Direction of the light in the world.
         * @param brightness The brightness of light.
         * @param hasShadow To enable shadows.
         */
		LightHandle addDirLight(const glm::vec3& direction, float brightness, bool hasShadow);

        /**
         * @name Light Property Setters
         *
         * @brief Let's light properties.
         * 
         * @{
         */

        
        /// @brief Set Position for point lights.
		void setLightPosition(LightHandle handle, const glm::vec3& position);
        /// @brief Set Direction for directional lights.
        void setLightDirection(LightHandle handle, const glm::vec3& direction);
        /// @brief Set Brightness for any lights.
		void setLightBrightness(LightHandle handle, float brightness);
        /**
         * @}
         */

        /**
         * @brief Get's the light data for use.
         *
         * @returns Light uniform buffer.
         */
        inline const LightsUniformBufferObject& getLightsData() const { return lightsData; }

        /**
         * @fn bind
         *
         * @brief Binds the shadow maps to the pipeline.
         *
         * @param cmdBuffer The command buffer to record to.
         * @param layout The Pipeline layout of the bound pipeline.
         * @param set The descriptor set index to bind at.
         */
		inline void bind(VkCommandBuffer cmdBuffer, VkPipelineLayout layout, uint32_t set) const
		{
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, set, 1, &shadowDescriptorSet.get(), 0, nullptr);
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

        /**
         * @fn cast(const Context& context, Camera* camera, VkCommandBuffer cmdBuffer, const std::vector<Drawable*>& drawables)
         *
         * @brief Cast all the shadows.
         *
         * The cast function interates and casts each of the shadows that exist.
         */
		void cast(const Context& context, Camera* camera, VkCommandBuffer cmdBuffer, const std::vector<Drawable*>& drawables);

		void setShadowClipPlanes(ShadowHandle handle, float nearPlane, float farPlane)
		{
			if (!pointShadowHandleValidity[handle])
			{
				throw std::out_of_range("Invalid Shadow Handle.");
			}
			pointShadows[handle].nearPlane = nearPlane;
			pointShadows[handle].farPlane = farPlane;
		}

        /**
         * @name Getters
         *
         * @brief Getter for private members.
         *
         * @{
         */
		const VkDescriptorSetLayout& get_shadowLayout() const { return shadowLayout.get(); }
        /**
         * @}
         */

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

		ShadowHandle createPointShadow(const glm::vec3& position = glm::vec3(0.0f), float nearPlane = 1.0f, float farPlane = 20.0f)
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

		ShadowHandle createDirShadow(const glm::vec3& direction = glm::vec3(0,-1,0), float width = 64.0f, float height = 64.0f, float nearPlane = 1.0f, float  farPlane = 512.0f)
		{
			if (dirShadowFreeStack.empty())
			{
				throw std::runtime_error("Max shadows reached.");
			}
			auto handle = dirShadowFreeStack.back();
			dirShadowFreeStack.pop_back();
			dirShadowHandleValidity[handle] = true;
			dirShadows[handle].direction = direction;
			dirShadows[handle].width = width;
			dirShadows[handle].height = height;
			dirShadows[handle].nearPlane = nearPlane;
			dirShadows[handle].farPlane = farPlane;
			return handle;
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

            shadow.get_shadowMap().implicitTransferLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

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
			vkCmdSetDepthBias(cmdBuffer, 1.75f, 0.0f, 2.25f);
			vkCmdSetViewport(cmdBuffer, 0, 1, &shadow.viewport.get());
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.get(), 0, 1, uboDescriptorSet.data(), 0, nullptr);
			vkCmdPushConstants(cmdBuffer, pipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, sizeof(ModelPushConstantBlock), sizeof(ShadowPushConstantBlock), &shadowPCB);

			for (Drawable* d : drawables)
			{
				d->drawGeometry(cmdBuffer, pipelineLayout.get());
			}

			vkCmdEndRenderPass(cmdBuffer);

			shadow.get_shadowMap().transferLayout(cmdBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}
	};
}
