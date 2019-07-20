
#pragma once

#include "util/Managed.hpp"
#include "Context.hpp"

#include <map>
#include <vector>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
	class Renderer
	{
	public:
		const int MAX_FRAMES_IN_FLIGHT = 2;

	private:
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
		util::Managed<VkPipelineLayout> graphicsPipelineLayout;
		util::Managed<VkPipeline> graphicsPipeline;
		util::ManagedVector<VkFramebuffer> framebuffers;
		util::ManagedVector<VkCommandBuffer, false> commandBuffers;
		util::ManagedVector<VkSemaphore> imageAvailableSem;
		util::ManagedVector<VkSemaphore> renderFinishedSem;
		util::ManagedVector<VkFence> inFlightFences;

		int currentFrame{ 0 };

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
				renderPass = Managed(createRenderPass(), [dev = context.get_device()](VkRenderPass& rp) { vkDestroyRenderPass(dev, rp, nullptr); });

				{
					auto [gPipelineLayout, gPipeline] = createGraphicsPipeline();
					graphicsPipelineLayout = Managed(gPipelineLayout, [dev = context.get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
					graphicsPipeline = Managed(gPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
				}

				framebuffers = ManagedVector(createFramebuffers(), [dev = context.get_device()](VkFramebuffer& fb) { vkDestroyFramebuffer(dev, fb, nullptr); });
				commandBuffers = ManagedVector<VkCommandBuffer,false>(allocateCommandBuffers(), [dev = context.get_device(), pool = context.get_commandPool()](vector<VkCommandBuffer>& buf) { vkFreeCommandBuffers(dev, pool, buf.size(), buf.data()); });

				recordCommandBuffers();

				{
					auto [startSems, endSems, fences] = createSyncObjects();
					imageAvailableSem = ManagedVector(startSems, [dev = context.get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
					renderFinishedSem = ManagedVector(endSems, [dev = context.get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
					inFlightFences = ManagedVector(fences, [dev = context.get_device()](VkFence& sem) { vkDestroyFence(dev, sem, nullptr); });
				}

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
			renderPass(std::move(other.renderPass)),
			graphicsPipelineLayout(std::move(other.graphicsPipelineLayout)),
			graphicsPipeline(std::move(other.graphicsPipeline)),
			framebuffers(std::move(other.framebuffers)),
			commandBuffers(std::move(other.commandBuffers)),
			imageAvailableSem(std::move(other.imageAvailableSem)),
			renderFinishedSem(std::move(other.renderFinishedSem)),
			inFlightFences(std::move(other.inFlightFences))
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
			renderPass = std::move(other.renderPass);
			graphicsPipelineLayout = std::move(other.graphicsPipelineLayout);
			graphicsPipeline = std::move(other.graphicsPipeline);
			framebuffers = std::move(other.framebuffers);
			commandBuffers = std::move(other.commandBuffers);
			imageAvailableSem = std::move(other.imageAvailableSem);
			renderFinishedSem = std::move(other.renderFinishedSem);
			inFlightFences = std::move(other.inFlightFences);
			return *this;
		}

		Renderer(const Renderer& other) = delete;
		Renderer& operator=(const Renderer& other) = delete;

		void renderFrame();

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

		// Context forwarding
		VkDevice get_device() const { return context.get_device(); }

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
			renderPass = Managed(createRenderPass(), [dev = context.get_device()](VkRenderPass& rp) { vkDestroyRenderPass(dev, rp, nullptr); });

			{
				auto [gPipelineLayout, gPipeline] = createGraphicsPipeline();
				graphicsPipelineLayout = Managed(gPipelineLayout, [dev = context.get_device()](VkPipelineLayout& lay) { vkDestroyPipelineLayout(dev, lay, nullptr); });
				graphicsPipeline = Managed(gPipeline, [dev = context.get_device()](VkPipeline& lay) { vkDestroyPipeline(dev, lay, nullptr); });
			}

			framebuffers = ManagedVector(createFramebuffers(), [dev = context.get_device()](VkFramebuffer& fb) { vkDestroyFramebuffer(dev, fb, nullptr); });
			commandBuffers = ManagedVector<VkCommandBuffer, false>(allocateCommandBuffers(), [dev = context.get_device(), pool = context.get_commandPool()](std::vector<VkCommandBuffer>& buf) { vkFreeCommandBuffers(dev, pool, buf.size(), buf.data()); });

			recordCommandBuffers();
		}

		std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> Renderer::createSwapchain() const;
		std::vector<VkImage> getSwapchainImages() const;
		std::vector<VkImageView> createSwapchainImageViews() const;
		VkRenderPass createRenderPass() const;
		std::tuple<VkPipelineLayout, VkPipeline> createGraphicsPipeline() const;
		std::vector<VkFramebuffer> createFramebuffers() const;
		std::vector<VkCommandBuffer> allocateCommandBuffers() const;
		std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> Renderer::createSyncObjects() const;
		void recordCommandBuffers();
	};
}
