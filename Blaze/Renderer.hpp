
#pragma once

#include "util/Managed.hpp"
#include "util/createFunctions.hpp"
#include "Context.hpp"
#include "Datatypes.hpp"
#include "UniformBuffer.hpp"
#include "TextureCube.hpp"
#include "Texture2D.hpp"
#include "GUI.hpp"
#include "Swapchain.hpp"
#include "ShadowCaster.hpp"
#include "Drawable.hpp"

#include <map>
#include <vector>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
	using RenderCommand = std::function<void(VkCommandBuffer buf, VkPipelineLayout layout, uint32_t frameCount)>;

    /**
     * @class Renderer
     *
     * @brief Contains entire rendering logic.
     *
     * This class contains the main drawing logic and all the related components.
     * This is a forward renderer that supports PBR rendering.
     */
	class Renderer
	{
	private:
		uint32_t max_frames_in_flight{ 2 };
		bool isComplete{ false };
		bool windowResized{ false };

		Context context;
		Swapchain swapchain;
		GUI gui;
		ShadowCaster shadowCaster;
		Camera* camera;

		util::Managed<VkRenderPass> renderPass;

		util::Managed<VkDescriptorSetLayout> uboDescriptorSetLayout;
		util::Managed<VkDescriptorSetLayout> environmentDescriptorSetLayout;
		util::Managed<VkDescriptorSetLayout> materialDescriptorSetLayout;

		util::Managed<VkDescriptorPool> descriptorPool;
		util::UnmanagedVector<VkDescriptorSet> uboDescriptorSets;

		std::vector<UniformBuffer<RendererUniformBufferObject>> rendererUniformBuffers;
		RendererUniformBufferObject rendererUBO{};
		std::vector<UniformBuffer<SettingsUniformBufferObject>> settingsUniformBuffers;
		SettingsUniformBufferObject settingsUBO{};

		util::Managed<VkPipelineLayout> graphicsPipelineLayout;
		util::Managed<VkPipeline> graphicsPipeline;
		util::Managed<VkPipeline> skyboxPipeline;

		util::ManagedVector<VkFramebuffer> renderFramebuffers;
		util::ManagedVector<VkCommandBuffer, false> commandBuffers;

		util::ManagedVector<VkSemaphore> imageAvailableSem;
		util::ManagedVector<VkSemaphore> renderFinishedSem;
		util::ManagedVector<VkFence> inFlightFences;

		std::vector<Drawable*> drawables;
		VkDescriptorSet environmentDescriptor{ VK_NULL_HANDLE };
		RenderCommand skyboxCommand;

		Texture2D depthBufferTexture;

		uint32_t currentFrame{ 0 };

	public:
        /**
         * @fn Renderer()
         *
         * @brief Default empty constructor.
         */
		Renderer() noexcept {}

        /**
         * @fn Renderer(GLFWwindow* window, bool enableValidationLayers = true)
         *
         * @brief Actual constructor
         *
         * @param window The GLFW window pointer.
         * @param enableValidationLayers Enable for Debugging.
         */
		Renderer(GLFWwindow* window, bool enableValidationLayers = true) noexcept;

        /**
         * @name Move Constructors.
         *
         * @brief Move only, Copy is deleted.
         * @{
         */
		Renderer(Renderer&& other) noexcept;
		Renderer& operator=(Renderer&& other) noexcept;
		Renderer(const Renderer& other) = delete;
		Renderer& operator=(const Renderer& other) = delete;
        /**
         * @}
         */

        /**
         * @fn renderFrame()
         *
         * @brief Actually rendering a frame and submitting to the present queue.
         */
		void renderFrame();

        /**
         * @name Environment Mapping
         *
         * @brief Creates maps for the environment based on the skybox.
         *
         * @{
         */
		TextureCube createIrradianceCube(VkDescriptorSet environment) const;
		TextureCube createPrefilteredCube(VkDescriptorSet environment) const;
		Texture2D	createBrdfLut() const;
        /**
         * @}
         */

        /**
         * @name Getters
         *
         * @brief Getters for private members.
         *
         * @{
         */
		std::pair<uint32_t, uint32_t> get_dimensions() const
		{
			int x, y;
			glfwGetWindowSize(context.get_window(), &x, &y);
			return { x,y };
		}
		const VkFormat& get_swapchainFormat() const { return swapchain.get_format(); }
		const VkExtent2D& get_swapchainExtent() const { return swapchain.get_extent(); }
		size_t get_swapchainImageCount() const { return swapchain.get_imageCount(); }
		const VkImageView& get_swapchainImageView(uint32_t index) const { return swapchain.get_imageView(index); }
		const std::vector<VkImageView>& get_swapchainImageViews() const { return swapchain.get_imageViews(); }
		const VkRenderPass& get_renderPass() const { return renderPass.get(); }
		const VkPipeline& get_graphicsPipeline() const { return graphicsPipeline.get(); }
		size_t get_framebufferCount() const { return renderFramebuffers.size(); }
		const VkFramebuffer& get_framebuffer(size_t index) const { return renderFramebuffers[index]; }
		size_t get_commandBufferCount() const { return commandBuffers.size(); }
		const VkCommandBuffer& get_commandBuffer(size_t index) const { return commandBuffers[index]; }
		const VkDescriptorSetLayout& get_uboLayout() const { return uboDescriptorSetLayout.get(); }
		const VkDescriptorSetLayout& get_materialLayout() const { return materialDescriptorSetLayout.get(); }
		const VkDescriptorSetLayout& get_environmentLayout() const { return environmentDescriptorSetLayout.get(); }
		ShadowCaster& get_lightSystem() { return shadowCaster; }

		// Context forwarding
		VkDevice get_device() const { return context.get_device(); }
		VkPhysicalDevice get_physicalDevice() const { return context.get_physicalDevice(); }
		VkQueue get_transferQueue() const { return context.get_transferQueue(); }
		VkCommandPool get_transferCommandPool() const { return context.get_transferCommandPool(); }
		const Context& get_context() const { return context; }
        /**
         * @}
         */

		/**
         * @fn submit
         *
         * @brief Submits a Drawable to draw to screen.
         */
		void submit(Drawable* cmd)
		{
			drawables.push_back(cmd);
		}

        /**
         * @name Setters
         *
         * @brief Setters for various properties.
         *
         * @{
         */
		void set_environmentDescriptor(VkDescriptorSet envDS)
		{
			environmentDescriptor = envDS;
		}

		template <class RNDRCMD>
		void set_skyboxCommand(const RNDRCMD& cmd)
		{
			skyboxCommand = cmd;
		}

		void set_cameraUBO(const CameraUniformBufferObject& ubo)
		{
			memcpy(&rendererUBO, &ubo, sizeof(ubo));
		}

		void set_camera(Camera* cam)
		{
			camera = cam;
		}

		void set_lightUBO(const LightsUniformBufferObject& ubo)
		{
			memcpy(&rendererUBO.dirLightTransform, &ubo, sizeof(ubo));
		}

		void set_settingsUBO(const SettingsUniformBufferObject& ubo)
		{
			memcpy(&settingsUBO, &ubo, sizeof(ubo));
		}
        /**
         * @}
         */

        /**
         * @fn complete()
         *
         * @brief Checks if the renderer is complete.
         *
         * A Renderer is considered complete if and only if every member is initialized.
         * 
         * @returns \a true If the renderer sucessfully initialized
         * @returns \a false If the renderer was empty or had initialization errors.
         */
		bool complete() const { return isComplete; }
	
    private:
		void recreateSwapchain();

		VkRenderPass createRenderPass() const;
		VkDescriptorSetLayout createUBODescriptorSetLayout() const;
		VkDescriptorSetLayout createEnvironmentDescriptorSetLayout() const;
		VkDescriptorSetLayout createMaterialDescriptorSetLayout() const;
		VkDescriptorPool createDescriptorPool() const;
		std::vector<VkDescriptorSet> createCameraDescriptorSets() const;
		std::vector<UniformBuffer<RendererUniformBufferObject>> createUniformBuffers(const RendererUniformBufferObject& ubo) const;
		std::vector<UniformBuffer<SettingsUniformBufferObject>> createUniformBuffers(const SettingsUniformBufferObject& ubo) const;
		std::tuple<VkPipelineLayout, VkPipeline, VkPipeline> createGraphicsPipeline() const;
		std::vector<VkFramebuffer> createRenderFramebuffers() const;
		std::vector<VkCommandBuffer> allocateCommandBuffers() const;
		std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> createSyncObjects() const;
		void recordCommandBuffers();
		void rebuildCommandBuffer(int frame);

		Texture2D createDepthBuffer() const;

		void updateUniformBuffer(int frame, const RendererUniformBufferObject& ubo)
		{
			rendererUniformBuffers[frame].write(context, ubo);
		}

		void updateUniformBuffer(int frame, const SettingsUniformBufferObject& ubo)
		{
			settingsUniformBuffers[frame].write(context, ubo);
		}
	};
}
