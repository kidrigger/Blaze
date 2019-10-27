
#pragma once

#include "util/Managed.hpp"
#include "util/createFunctions.hpp"
#include "Context.hpp"
#include "Datatypes.hpp"
#include "UniformBuffer.hpp"
#include "TextureCube.hpp"
#include "Texture2D.hpp"

#include <map>
#include <vector>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
	class Renderer;
	using RenderCommand = std::function<void(VkCommandBuffer buf, VkPipelineLayout layout)>;

	class Renderer
	{
	private:
		uint32_t max_frames_in_flight{ 2 };
		bool isComplete{ false };
		bool windowResized{ false };

		std::function<std::pair<uint32_t, uint32_t>(void)> getWindowSize;

		Context context;

		util::Managed<VkSwapchainKHR> swapchain;
		util::Unmanaged<VkFormat> swapchainFormat;
		util::Unmanaged<VkExtent2D> swapchainExtent;

		util::UnmanagedVector<VkImage> swapchainImages;
		util::ManagedVector<VkImageView> swapchainImageViews;

		util::Managed<VkRenderPass> renderPass;

		util::Managed<VkDescriptorSetLayout> uboDescriptorSetLayout;
		util::Managed<VkDescriptorPool> descriptorPool;
		util::UnmanagedVector<VkDescriptorSet> descriptorSets;

		util::Managed<VkDescriptorSetLayout> environmentDescriptorSetLayout;
		util::Managed<VkDescriptorSetLayout> materialDescriptorSetLayout;

		std::vector<UniformBuffer<CameraUniformBufferObject>> uniformBuffers;
		CameraUniformBufferObject cameraUBO{};

		util::Managed<VkPipelineLayout> graphicsPipelineLayout;
		util::Managed<VkPipeline> graphicsPipeline;
		util::Managed<VkPipeline> skyboxPipeline;

		util::ManagedVector<VkFramebuffer> framebuffers;
		util::ManagedVector<VkCommandBuffer, false> commandBuffers;

		util::ManagedVector<VkSemaphore> imageAvailableSem;
		util::ManagedVector<VkSemaphore> renderFinishedSem;
		util::ManagedVector<VkFence> inFlightFences;

		std::vector<RenderCommand> renderCommands;
		RenderCommand skyboxCommand;
		std::vector<bool> commandBufferDirty;

		util::Managed<ImageObject> depthBuffer;
		util::Managed<VkImageView> depthBufferView;

		uint32_t currentFrame{ 0 };

		// Felt cute, may refactor later.
		struct EnvironmentMaps
		{
			TextureCube IrradianceMap;
			TextureCube PreFilteredMap;
			Texture2D	LutBrdf2D;
		} environmentMaps;

	public:
		Renderer() noexcept
		{
		}

		Renderer(GLFWwindow* window) noexcept
			: context(window),
			getWindowSize([window]()
				{
					int width, height;
					glfwGetWindowSize(window, &width, &height);
					return std::make_pair((uint32_t)width, (uint32_t)height);
				})
		{
			using namespace std;
			using namespace util;

			skyboxCommand = [](VkCommandBuffer cb, VkPipelineLayout lay) {};

			glfwSetWindowUserPointer(window, this);
			glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height)
				{
					Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
					renderer->windowResized = true;
				});

			try
			{
				{
					auto [swapc, swapcFormat, swapcExtent] = createSwapchain();
					swapchain = Managed(swapc, [dev = context.get_device()](VkSwapchainKHR& swpc){ vkDestroySwapchainKHR(dev, swpc, nullptr); });
					swapchainFormat = swapcFormat;
					swapchainExtent = swapcExtent;
				}

				swapchainImages = getSwapchainImages();
				swapchainImageViews = ManagedVector(createSwapchainImageViews(), [dev = context.get_device()](VkImageView& view) { vkDestroyImageView(dev, view, nullptr); });

				depthBuffer = Managed(createDepthBuffer(), [alloc = context.get_allocator()](ImageObject& obj) { vmaDestroyImage(alloc, obj.image, obj.allocation); });
				depthBufferView = Managed(createImageView(context.get_device(), depthBuffer.get().image, VK_IMAGE_VIEW_TYPE_2D, depthBuffer.get().format, VK_IMAGE_ASPECT_DEPTH_BIT, 1), [dev = context.get_device()](VkImageView& iv) { vkDestroyImageView(dev, iv, nullptr); });

				renderPass = Managed(createRenderPass(), [dev = context.get_device()](VkRenderPass& rp) { vkDestroyRenderPass(dev, rp, nullptr); });

				uniformBuffers = createUniformBuffers(cameraUBO);
				uboDescriptorSetLayout = Managed(createUBODescriptorSetLayout(), [dev = context.get_device()](VkDescriptorSetLayout& lay) { vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
				environmentDescriptorSetLayout = Managed(createEnvironmentDescriptorSetLayout(), [dev = context.get_device()](VkDescriptorSetLayout& lay) { vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
				materialDescriptorSetLayout = Managed(createMaterialDescriptorSetLayout(), [dev = context.get_device()](VkDescriptorSetLayout& lay) { vkDestroyDescriptorSetLayout(dev, lay, nullptr); });
				descriptorPool = Managed(createDescriptorPool(), [dev = context.get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });
				descriptorSets = createDescriptorSets();

				{
					auto [gPipelineLayout, gPipeline, sbPipeline] = createGraphicsPipeline();
					graphicsPipelineLayout = Managed(gPipelineLayout, [dev = context.get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
					graphicsPipeline = Managed(gPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
					skyboxPipeline = Managed(sbPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
				}

				framebuffers = ManagedVector(createFramebuffers(), [dev = context.get_device()](VkFramebuffer& fb) { vkDestroyFramebuffer(dev, fb, nullptr); });
				commandBuffers = ManagedVector<VkCommandBuffer,false>(allocateCommandBuffers(), [dev = context.get_device(), pool = context.get_graphicsCommandPool()](vector<VkCommandBuffer>& buf) { vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(buf.size()), buf.data()); });
				commandBufferDirty.resize(commandBuffers.size(), true);

				max_frames_in_flight = static_cast<uint32_t>(commandBuffers.size());

				{
					auto [startSems, endSems, fences] = createSyncObjects();
					imageAvailableSem = ManagedVector(startSems, [dev = context.get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
					renderFinishedSem = ManagedVector(endSems, [dev = context.get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
					inFlightFences = ManagedVector(fences, [dev = context.get_device()](VkFence& sem) { vkDestroyFence(dev, sem, nullptr); });
				}

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
			: getWindowSize(std::move(other.getWindowSize)),
			isComplete(other.isComplete),
			context(std::move(other.context)),
			swapchain(std::move(other.swapchain)),
			swapchainFormat(std::move(other.swapchainFormat)),
			swapchainExtent(std::move(other.swapchainExtent)),
			swapchainImages(std::move(other.swapchainImages)),
			swapchainImageViews(std::move(other.swapchainImageViews)),
			depthBuffer(std::move(other.depthBuffer)),
			depthBufferView(std::move(other.depthBufferView)),
			renderPass(std::move(other.renderPass)),
			uboDescriptorSetLayout(std::move(other.uboDescriptorSetLayout)),
			environmentDescriptorSetLayout(std::move(other.environmentDescriptorSetLayout)),
			materialDescriptorSetLayout(std::move(other.materialDescriptorSetLayout)),
			descriptorPool(std::move(other.descriptorPool)),
			descriptorSets(std::move(other.descriptorSets)),
			uniformBuffers(std::move(other.uniformBuffers)),
			cameraUBO(other.cameraUBO),
			graphicsPipelineLayout(std::move(other.graphicsPipelineLayout)),
			graphicsPipeline(std::move(other.graphicsPipeline)),
			skyboxPipeline(std::move(other.skyboxPipeline)),
			framebuffers(std::move(other.framebuffers)),
			commandBuffers(std::move(other.commandBuffers)),
			imageAvailableSem(std::move(other.imageAvailableSem)),
			renderFinishedSem(std::move(other.renderFinishedSem)),
			inFlightFences(std::move(other.inFlightFences)),
			skyboxCommand(std::move(other.skyboxCommand)),
			renderCommands(std::move(other.renderCommands)),
			commandBufferDirty(std::move(other.commandBufferDirty)),
			environmentMaps(std::move(other.environmentMaps))
		{
		}

		Renderer& operator=(Renderer&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			getWindowSize = std::move(other.getWindowSize);
			isComplete = other.isComplete;
			context = std::move(other.context);
			swapchain = std::move(other.swapchain);
			swapchainFormat = std::move(other.swapchainFormat);
			swapchainExtent = std::move(other.swapchainExtent);
			swapchainImages = std::move(other.swapchainImages);
			swapchainImageViews = std::move(other.swapchainImageViews);
			depthBuffer = std::move(other.depthBuffer);
			depthBufferView = std::move(other.depthBufferView);
			renderPass = std::move(other.renderPass);
			uboDescriptorSetLayout = std::move(other.uboDescriptorSetLayout);
			environmentDescriptorSetLayout = std::move(other.environmentDescriptorSetLayout);
			materialDescriptorSetLayout = std::move(other.materialDescriptorSetLayout);
			descriptorPool = std::move(other.descriptorPool);
			descriptorSets = std::move(other.descriptorSets);
			uniformBuffers = std::move(other.uniformBuffers);
			cameraUBO = other.cameraUBO;
			graphicsPipelineLayout = std::move(other.graphicsPipelineLayout);
			graphicsPipeline = std::move(other.graphicsPipeline);
			skyboxPipeline = std::move(other.skyboxPipeline);
			framebuffers = std::move(other.framebuffers);
			commandBuffers = std::move(other.commandBuffers);
			imageAvailableSem = std::move(other.imageAvailableSem);
			renderFinishedSem = std::move(other.renderFinishedSem);
			inFlightFences = std::move(other.inFlightFences);
			skyboxCommand = std::move(other.skyboxCommand);
			renderCommands = std::move(other.renderCommands);
			commandBufferDirty = std::move(other.commandBufferDirty);
			environmentMaps = std::move(other.environmentMaps);
			return *this;
		}

		Renderer(const Renderer& other) = delete;
		Renderer& operator=(const Renderer& other) = delete;

		void renderFrame();

		TextureCube createIrradianceCube(VkDescriptorSet environment) const;

		std::pair<uint32_t, uint32_t> get_dimensions() const { return getWindowSize(); }
		VkSwapchainKHR get_swapchain() const { return swapchain.get(); }
		VkFormat get_swapchainFormat() const { return swapchainFormat.get(); }
		VkExtent2D get_swapchainExtent() const { return swapchainExtent.get(); }
		size_t get_swapchainImageCount() const { return swapchainImageViews.size(); }
		const VkImageView& get_swapchainImageView(size_t index) const { return swapchainImageViews[index]; }
		VkRenderPass get_renderPass() const { return renderPass.get(); }
		VkPipeline get_graphicsPipeline() const { return graphicsPipeline.get(); }
		size_t get_framebufferCount() const { return framebuffers.size(); }
		const VkFramebuffer& get_framebuffer(size_t index) const { return framebuffers[index]; }
		size_t get_commandBufferCount() const { return commandBuffers.size(); }
		const VkCommandBuffer& get_commandBuffer(size_t index) const { return commandBuffers[index]; }
		VkDescriptorSetLayout get_uboLayout() const { return uboDescriptorSetLayout.get(); }
		VkDescriptorSetLayout get_materialLayout() const { return materialDescriptorSetLayout.get(); }
		VkDescriptorSetLayout get_environmentLayout() const { return environmentDescriptorSetLayout.get(); }

		// Context forwarding
		VkDevice get_device() const { return context.get_device(); }
		VkPhysicalDevice get_physicalDevice() const { return context.get_physicalDevice(); }
		VkQueue get_transferQueue() const { return context.get_transferQueue(); }
		VkCommandPool get_transferCommandPool() const { return context.get_transferCommandPool(); }
		const Context& get_context() const { return context; }

		// Submit
		template <class RNDRCMD>
		void submit(const RNDRCMD& cmd)
		{
			renderCommands.emplace_back(cmd);
			for (auto& val : commandBufferDirty)
			{
				val = true;
			}
		}

		template <class RNDRCMD>
		void set_skyboxCommand(const RNDRCMD& cmd)
		{
			skyboxCommand = cmd;
		}

		void set_cameraUBO(const CameraUniformBufferObject& ubo)
		{
			cameraUBO = ubo;
		}

		bool complete() const { return isComplete; }
	private:

		void recreateSwapchain()
		{
			vkDeviceWaitIdle(context.get_device());
			auto [width, height] = getWindowSize();
			while (width == 0 || height == 0)
			{
				std::tie(width, height) = getWindowSize();
				glfwWaitEvents();
			}

			using namespace util;
			{
				auto [swapc, swapcFormat, swapcExtent] = createSwapchain();
				swapchain = Managed(swapc, [dev = context.get_device()](VkSwapchainKHR& swpc){ vkDestroySwapchainKHR(dev, swpc, nullptr); });
				swapchainFormat = swapcFormat;
				swapchainExtent = swapcExtent;
			}

			swapchainImages = getSwapchainImages();
			swapchainImageViews = ManagedVector(createSwapchainImageViews(), [dev = context.get_device()](VkImageView& view) { vkDestroyImageView(dev, view, nullptr); });

			depthBuffer = Managed(createDepthBuffer(), [alloc = context.get_allocator()](ImageObject& obj) { vmaDestroyImage(alloc, obj.image, obj.allocation); });
			depthBufferView = Managed(createImageView(context.get_device(), depthBuffer.get().image, VK_IMAGE_VIEW_TYPE_2D, depthBuffer.get().format, VK_IMAGE_ASPECT_DEPTH_BIT, 1), [dev = context.get_device()](VkImageView& iv) { vkDestroyImageView(dev, iv, nullptr); });

			renderPass = Managed(createRenderPass(), [dev = context.get_device()](VkRenderPass& rp) { vkDestroyRenderPass(dev, rp, nullptr); });

			uniformBuffers = createUniformBuffers(cameraUBO);
			descriptorPool = Managed(createDescriptorPool(), [dev = context.get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });
			descriptorSets = createDescriptorSets();

			{
				auto [gPipelineLayout, gPipeline, sbPipeline] = createGraphicsPipeline();
				graphicsPipelineLayout = Managed(gPipelineLayout, [dev = context.get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
				graphicsPipeline = Managed(gPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
				skyboxPipeline = Managed(sbPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
			}

			framebuffers = ManagedVector(createFramebuffers(), [dev = context.get_device()](VkFramebuffer& fb) { vkDestroyFramebuffer(dev, fb, nullptr); });
			commandBuffers = ManagedVector<VkCommandBuffer, false>(allocateCommandBuffers(), [dev = context.get_device(), pool = context.get_graphicsCommandPool()](std::vector<VkCommandBuffer>& buf) { vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(buf.size()), buf.data()); });
			commandBufferDirty.resize(commandBuffers.size(), true);

			recordCommandBuffers();
		}

		std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> Renderer::createSwapchain() const;
		std::vector<VkImage> getSwapchainImages() const;
		std::vector<VkImageView> createSwapchainImageViews() const;
		VkRenderPass createRenderPass() const;
		VkDescriptorSetLayout createUBODescriptorSetLayout() const;
		VkDescriptorSetLayout createEnvironmentDescriptorSetLayout() const;
		VkDescriptorSetLayout createMaterialDescriptorSetLayout() const;
		VkDescriptorPool createDescriptorPool() const;
		std::vector<VkDescriptorSet> createDescriptorSets() const;
		std::vector<UniformBuffer<CameraUniformBufferObject>> createUniformBuffers(const CameraUniformBufferObject& ubo) const;
		std::tuple<VkPipelineLayout, VkPipeline, VkPipeline> createGraphicsPipeline() const;
		std::vector<VkFramebuffer> createFramebuffers() const;
		std::vector<VkCommandBuffer> allocateCommandBuffers() const;
		std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> Renderer::createSyncObjects() const;
		void recordCommandBuffers();
		void rebuildCommandBuffer(int frame);

		ImageObject createDepthBuffer() const;

		void updateUniformBuffer(int frame, const CameraUniformBufferObject& ubo)
		{
			void* data;
			vmaMapMemory(context.get_allocator(), uniformBuffers[frame].get_allocation(), &data);
			memcpy(data, &ubo, uniformBuffers[frame].get_size());
			vmaUnmapMemory(context.get_allocator(), uniformBuffers[frame].get_allocation());
		}
	};
}
