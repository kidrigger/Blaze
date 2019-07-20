// Blaze.cpp : Defines the entry point for the application.
//

#include "Blaze.hpp"
#include "util/Managed.hpp"
#include "util/DeviceSelection.hpp"
#include "Context.hpp"
#include "Renderer.hpp"

#include <optional>
#include <vector>
#include <iostream>
#include <set>
#include <algorithm>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#define VALIDATION_LAYERS_ENABLED

#ifdef NDEBUG
#ifdef VALIDATION_LAYERS_ENABLED
#undef VALIDATION_LAYERS_ENABLED
#endif
#endif

namespace blaze
{
	// Constants
	const int WIDTH = 800;
	const int HEIGHT = 600;

	const int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef VALIDATION_LAYERS_ENABLED
	const bool enableValidationLayers = true;
#else
	const bool enableValidationLayers = false;
#endif

	void run()
	{
		// Usings
		using namespace std;
		using std::byte;

		// Variables
		GLFWwindow* window = nullptr;
		Context ctx;
		Renderer renderer;
		// Moved to renderer // VkRenderPass renderpass = VK_NULL_HANDLE;
		// Moved to renderer // VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
		// Moved to renderer // VkPipeline graphicsPipeline = VK_NULL_HANDLE;
		vector<VkFramebuffer> swapchainFramebuffers;
		vector<VkCommandBuffer> commandBuffers;
		vector<VkSemaphore> imageAvailableSem(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
		vector<VkSemaphore> renderFinishedSem(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
		vector<VkFence> inFlightFences(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);

		// GLFW Setup
		assert(glfwInit());

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Hello, Vulkan", nullptr, nullptr);
		assert(window != nullptr);

		// Instance
		ctx = Context(window, true);
		if (!ctx.complete())
		{
			throw std::runtime_error("Context could not be created");
		}

		renderer = Renderer(window, ctx);
		if (!renderer.complete())
		{
			throw std::runtime_error("Renderer could not be created");
		}

		// Framebuffers
		swapchainFramebuffers.resize(renderer.get_swapchainImageViewsCount());
		for (size_t i = 0; i < renderer.get_swapchainImageViewsCount(); i++)
		{
			vector<VkImageView> attachments = {
				renderer.get_swapchainImageView(i)
			};

			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderer.get_renderPass();
			createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			createInfo.pAttachments = attachments.data();
			createInfo.width = renderer.get_swapchainExtent().width;
			createInfo.height = renderer.get_swapchainExtent().height;
			createInfo.layers = 1;

			assert(vkCreateFramebuffer(ctx.get_device(), &createInfo, nullptr, &swapchainFramebuffers[i]) == VK_SUCCESS);
		}

		{
			commandBuffers.resize(swapchainFramebuffers.size());

			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = ctx.get_commandPool();
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
			assert(vkAllocateCommandBuffers(ctx.get_device(), &allocInfo, commandBuffers.data()) == VK_SUCCESS);
		}

		for (size_t i = 0; i < commandBuffers.size(); i++)
		{
			VkCommandBufferBeginInfo commandBufferBeginInfo = {};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

			assert(vkBeginCommandBuffer(commandBuffers[i], &commandBufferBeginInfo) == VK_SUCCESS);

			VkRenderPassBeginInfo renderpassBeginInfo = {};
			renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderpassBeginInfo.renderPass = renderer.get_renderPass();
			renderpassBeginInfo.framebuffer = swapchainFramebuffers[i];
			renderpassBeginInfo.renderArea.offset = { 0, 0 };
			renderpassBeginInfo.renderArea.extent = renderer.get_swapchainExtent();

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderpassBeginInfo.clearValueCount = 1;
			renderpassBeginInfo.pClearValues = &clearColor;
			vkCmdBeginRenderPass(commandBuffers[i], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, renderer.get_graphicsPipeline());

			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

			vkCmdEndRenderPass(commandBuffers[i]);

			assert(vkEndCommandBuffer(commandBuffers[i]) == VK_SUCCESS);
		}

		auto createSemaphore = [](VkDevice device) -> VkSemaphore
		{
			VkSemaphore semaphore = VK_NULL_HANDLE;
			VkSemaphoreCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			if (vkCreateSemaphore(device, &createInfo, nullptr, &semaphore) == VK_SUCCESS)
			{
				return semaphore;
			}
			return VK_NULL_HANDLE;
		};

		auto createFence = [](VkDevice device) -> VkFence
		{
			VkFence fence = VK_NULL_HANDLE;
			VkFenceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			if (vkCreateFence(device, &createInfo, nullptr, &fence) == VK_SUCCESS)
			{
				return fence;
			}
			return VK_NULL_HANDLE;
		};

		for (auto& sem : imageAvailableSem)
		{
			sem = createSemaphore(ctx.get_device());
			assert(sem != VK_NULL_HANDLE);
		}

		for (auto& sem : renderFinishedSem)
		{
			sem = createSemaphore(ctx.get_device());
			assert(sem != VK_NULL_HANDLE);
		}

		for (auto& fence : inFlightFences)
		{
			fence = createFence(ctx.get_device());
			assert(fence != VK_NULL_HANDLE);
		}

		// Run
		size_t currentFrame = 0;
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();

			vkWaitForFences(ctx.get_device(), 1, &inFlightFences[currentFrame], VK_TRUE, numeric_limits<uint64_t>::max());
			vkResetFences(ctx.get_device(), 1, &inFlightFences[currentFrame]);

			uint32_t imageIndex;
			vkAcquireNextImageKHR(ctx.get_device(), renderer.get_swapchain(), numeric_limits<uint64_t>::max(), imageAvailableSem[currentFrame], VK_NULL_HANDLE, &imageIndex);

			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkSemaphore waitSemaphores[] = { imageAvailableSem[currentFrame] };
			VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
			submitInfo.waitSemaphoreCount = 1u;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

			VkSemaphore signalSemaphores[] = { renderFinishedSem[currentFrame] };
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;

			assert(vkQueueSubmit(ctx.get_graphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) == VK_SUCCESS);

			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;

			VkSwapchainKHR swapchains[] = { renderer.get_swapchain() };
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapchains;
			presentInfo.pImageIndices = &imageIndex;

			assert(vkQueuePresentKHR(ctx.get_presentQueue(), &presentInfo) == VK_SUCCESS);

			currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		}

		// Wait for all commands to finish
		vkDeviceWaitIdle(ctx.get_device());

		// Cleanup

		for (auto& sem : imageAvailableSem)
		{
			vkDestroySemaphore(ctx.get_device(), sem, nullptr);
		}
		for (auto& sem : renderFinishedSem)
		{
			vkDestroySemaphore(ctx.get_device(), sem, nullptr);
		}
		for (auto& fence : inFlightFences)
		{
			vkDestroyFence(ctx.get_device(), fence, nullptr);
		}

		for (auto framebuffer : swapchainFramebuffers)
		{
			vkDestroyFramebuffer(ctx.get_device(), framebuffer, nullptr);
		}

		glfwDestroyWindow(window);
		glfwTerminate();
	}
}

int main(int argc, char* argv[])
{
	blaze::run();

	return 0;
}
