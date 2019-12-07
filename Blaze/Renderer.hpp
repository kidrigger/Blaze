
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
	class Renderer;
	using RenderCommand = std::function<void(VkCommandBuffer buf, VkPipelineLayout layout, uint32_t frameCount)>;

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
		Renderer() noexcept
		{
		}

		Renderer(GLFWwindow* window, bool enableValidationLayers = true) noexcept
			: context(window, enableValidationLayers)
		{
			using namespace std;
			using namespace util;

			skyboxCommand = [](VkCommandBuffer cb, VkPipelineLayout lay, uint32_t frameCount) {};

			glfwSetWindowUserPointer(window, this);
			glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height)
				{
					Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
					renderer->windowResized = true;
				});

			try
			{

				swapchain = Swapchain(context);

				shadowCaster = ShadowCaster(context, 16, 16);
				
				depthBufferTexture = createDepthBuffer();
				
				renderPass = Managed(createRenderPass(), [dev = context.get_device()](VkRenderPass& rp) { vkDestroyRenderPass(dev, rp, nullptr); });

				rendererUniformBuffers = createUniformBuffers(rendererUBO);
				settingsUniformBuffers = createUniformBuffers(settingsUBO);
				uboDescriptorSetLayout = Managed(createUBODescriptorSetLayout(), [dev = context.get_device()](VkDescriptorSetLayout& lay) { vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
				environmentDescriptorSetLayout = Managed(createEnvironmentDescriptorSetLayout(), [dev = context.get_device()](VkDescriptorSetLayout& lay) { vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
				materialDescriptorSetLayout = Managed(createMaterialDescriptorSetLayout(), [dev = context.get_device()](VkDescriptorSetLayout& lay) { vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
				descriptorPool = Managed(createDescriptorPool(), [dev = context.get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });
				uboDescriptorSets = createCameraDescriptorSets();

				{
					auto [gPipelineLayout, gPipeline, sbPipeline] = createGraphicsPipeline();
					graphicsPipelineLayout = Managed(gPipelineLayout, [dev = context.get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
					graphicsPipeline = Managed(gPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
					skyboxPipeline = Managed(sbPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
				}

				renderFramebuffers = ManagedVector(createRenderFramebuffers(), [dev = context.get_device()](VkFramebuffer& fb) { vkDestroyFramebuffer(dev, fb, nullptr); });
				commandBuffers = ManagedVector<VkCommandBuffer,false>(allocateCommandBuffers(), [dev = context.get_device(), pool = context.get_graphicsCommandPool()](vector<VkCommandBuffer>& buf) { vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(buf.size()), buf.data()); });

				max_frames_in_flight = static_cast<uint32_t>(commandBuffers.size());

				{
					auto [startSems, endSems, fences] = createSyncObjects();
					imageAvailableSem = ManagedVector(startSems, [dev = context.get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
					renderFinishedSem = ManagedVector(endSems, [dev = context.get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
					inFlightFences = ManagedVector(fences, [dev = context.get_device()](VkFence& sem) { vkDestroyFence(dev, sem, nullptr); });
				}

				gui = GUI(context, swapchain.get_extent(), swapchain.get_format(), swapchain.get_imageViews());

				recordCommandBuffers();

				isComplete = true;
			}
			catch (std::exception& e)
			{
				std::cerr << "RENDERER_CREATION_FAILED: " << e.what() << std::endl;
				isComplete = false;
			}
		}

		Renderer(Renderer&& other) noexcept
			: isComplete(other.isComplete),
			context(std::move(other.context)),
			gui(std::move(other.gui)),
			swapchain(std::move(other.swapchain)),
			depthBufferTexture(std::move(other.depthBufferTexture)),
			renderPass(std::move(other.renderPass)),
			uboDescriptorSetLayout(std::move(other.uboDescriptorSetLayout)),
			environmentDescriptorSetLayout(std::move(other.environmentDescriptorSetLayout)),
			materialDescriptorSetLayout(std::move(other.materialDescriptorSetLayout)),
			descriptorPool(std::move(other.descriptorPool)),
			uboDescriptorSets(std::move(other.uboDescriptorSets)),
			rendererUniformBuffers(std::move(other.rendererUniformBuffers)),
			settingsUniformBuffers(std::move(other.settingsUniformBuffers)),
			rendererUBO(other.rendererUBO),
			settingsUBO(other.settingsUBO),
			graphicsPipelineLayout(std::move(other.graphicsPipelineLayout)),
			graphicsPipeline(std::move(other.graphicsPipeline)),
			skyboxPipeline(std::move(other.skyboxPipeline)),
			renderFramebuffers(std::move(other.renderFramebuffers)),
			commandBuffers(std::move(other.commandBuffers)),
			imageAvailableSem(std::move(other.imageAvailableSem)),
			renderFinishedSem(std::move(other.renderFinishedSem)),
			inFlightFences(std::move(other.inFlightFences)),
			skyboxCommand(std::move(other.skyboxCommand)),
			drawables(std::move(other.drawables)),
			environmentDescriptor(other.environmentDescriptor),
			shadowCaster(std::move(other.shadowCaster))
		{
		}

		Renderer& operator=(Renderer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			isComplete = other.isComplete;
			context = std::move(other.context);
			gui = std::move(other.gui);
			swapchain = std::move(other.swapchain);
			depthBufferTexture = std::move(other.depthBufferTexture);
			renderPass = std::move(other.renderPass);
			uboDescriptorSetLayout = std::move(other.uboDescriptorSetLayout);
			environmentDescriptorSetLayout = std::move(other.environmentDescriptorSetLayout);
			materialDescriptorSetLayout = std::move(other.materialDescriptorSetLayout);
			descriptorPool = std::move(other.descriptorPool);
			uboDescriptorSets = std::move(other.uboDescriptorSets);
			rendererUniformBuffers = std::move(other.rendererUniformBuffers);
			settingsUniformBuffers = std::move(other.settingsUniformBuffers);
			rendererUBO = other.rendererUBO;
			settingsUBO = other.settingsUBO;
			graphicsPipelineLayout = std::move(other.graphicsPipelineLayout);
			graphicsPipeline = std::move(other.graphicsPipeline);
			skyboxPipeline = std::move(other.skyboxPipeline);
			renderFramebuffers = std::move(other.renderFramebuffers);
			commandBuffers = std::move(other.commandBuffers);
			imageAvailableSem = std::move(other.imageAvailableSem);
			renderFinishedSem = std::move(other.renderFinishedSem);
			inFlightFences = std::move(other.inFlightFences);
			skyboxCommand = std::move(other.skyboxCommand);
			drawables = std::move(other.drawables);
			environmentDescriptor = other.environmentDescriptor;
			shadowCaster = std::move(other.shadowCaster);
			return *this;
		}

		Renderer(const Renderer& other) = delete;
		Renderer& operator=(const Renderer& other) = delete;

		void renderFrame();

		TextureCube createIrradianceCube(VkDescriptorSet environment) const;
		TextureCube createPrefilteredCube(VkDescriptorSet environment) const;
		Texture2D	createBrdfLut() const;

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

		// Submit
		void submit(Drawable* cmd)
		{
			drawables.push_back(cmd);
		}

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

		void set_lightUBO(const LightsUniformBufferObject& ubo)
		{
			memcpy(&rendererUBO.dirLightTransform, &ubo, sizeof(ubo));
		}

		void set_settingsUBO(const SettingsUniformBufferObject& ubo)
		{
			memcpy(&settingsUBO, &ubo, sizeof(ubo));
		}

		bool complete() const { return isComplete; }
	private:

		void recreateSwapchain()
		{
			try
			{
				vkDeviceWaitIdle(context.get_device());
				auto [width, height] = get_dimensions();
				while (width == 0 || height == 0)
				{
					std::tie(width, height) = get_dimensions();
					glfwWaitEvents();
				}

				using namespace util;
				swapchain.recreate(context);

				depthBufferTexture = createDepthBuffer();

				renderPass = Managed(createRenderPass(), [dev = context.get_device()](VkRenderPass& rp) { vkDestroyRenderPass(dev, rp, nullptr); });

				rendererUniformBuffers = createUniformBuffers(rendererUBO);
				settingsUniformBuffers = createUniformBuffers(settingsUBO);
				descriptorPool = Managed(createDescriptorPool(), [dev = context.get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });
				uboDescriptorSets = createCameraDescriptorSets();

				{
					auto [gPipelineLayout, gPipeline, sbPipeline] = createGraphicsPipeline();
					graphicsPipelineLayout = Managed(gPipelineLayout, [dev = context.get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
					graphicsPipeline = Managed(gPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
					skyboxPipeline = Managed(sbPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
				}

				renderFramebuffers = ManagedVector(createRenderFramebuffers(), [dev = context.get_device()](VkFramebuffer& fb) { vkDestroyFramebuffer(dev, fb, nullptr); });
				commandBuffers = ManagedVector<VkCommandBuffer, false>(allocateCommandBuffers(), [dev = context.get_device(), pool = context.get_graphicsCommandPool()](std::vector<VkCommandBuffer>& buf) { vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(buf.size()), buf.data()); });

				gui.recreate(context, swapchain.get_extent(), swapchain.get_imageViews());

				recordCommandBuffers();
			}
			catch (std::exception& e)
			{
				std::cerr << e.what() << std::endl;
			}
		}

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
