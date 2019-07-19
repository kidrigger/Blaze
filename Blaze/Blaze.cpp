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
#include <fstream>
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

	VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& shaderCode)
	{
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderCode.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());
		VkShaderModule shaderModule = VK_NULL_HANDLE;
		auto result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
		if (result == VK_SUCCESS)
		{
			return shaderModule;
		}
		throw std::runtime_error("Shader Module creation failed with " + std::to_string(result));
	}

	std::vector<char> loadBinaryFile(const std::string& filename)
	{
		using namespace std;
		ifstream file(filename, ios::ate | ios::binary);
		if (file.is_open())
		{
			size_t filesize = file.tellg();
			vector<char> filedata(filesize);
			file.seekg(0);
			file.read(filedata.data(), filesize);
			file.close();
			return filedata;
		}
		throw std::runtime_error("File (" + filename + ") could not be opened.");
	}

	VkRenderPass createRenderpass(VkDevice device, VkFormat swapchainFormat)
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapchainFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDesc = {};
		subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDesc.colorAttachmentCount = 1;
		subpassDesc.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		createInfo.attachmentCount = 1;
		createInfo.pAttachments = &colorAttachment;
		createInfo.subpassCount = 1;
		createInfo.pSubpasses = &subpassDesc;
		createInfo.dependencyCount = 1;
		createInfo.pDependencies = &dependency;

		VkRenderPass renderpass = VK_NULL_HANDLE;
		auto result = vkCreateRenderPass(device, &createInfo, nullptr, &renderpass);
		if (result == VK_SUCCESS)
		{
			return renderpass;
		}
		throw std::runtime_error("RenderPass creation failed with " + std::to_string(result));
	};

	std::tuple<VkPipelineLayout, VkPipeline> createGraphicsPipeline(VkDevice device, VkRenderPass renderpass, VkExtent2D swapchainExtent)
	{
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline graphicsPipeline = VK_NULL_HANDLE;

		auto vertexShaderCode = loadBinaryFile("shaders/vShader.vert.spv");
		auto fragmentShaderCode = loadBinaryFile("shaders/fShader.frag.spv");

		VkShaderModule vertexShaderModule = createShaderModule(device, vertexShaderCode);
		VkShaderModule fragmentShaderModule = createShaderModule(device, fragmentShaderCode);

		VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
		vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexShaderStageCreateInfo.module = vertexShaderModule;
		vertexShaderStageCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
		fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentShaderStageCreateInfo.module = fragmentShaderModule;
		fragmentShaderStageCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStagesCreateInfo[] = {
			vertexShaderStageCreateInfo,
			fragmentShaderStageCreateInfo
		};

		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 0;
		vertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
		vertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapchainExtent.width;
		viewport.height = (float)swapchainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = swapchainExtent;

		VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
		viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCreateInfo.viewportCount = 1;
		viewportStateCreateInfo.pViewports = &viewport;
		viewportStateCreateInfo.scissorCount = 1;
		viewportStateCreateInfo.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
		rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerCreateInfo.depthClampEnable = VK_FALSE;
		rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizerCreateInfo.lineWidth = 1.0f;
		rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
		multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
		multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorblendAttachment = {};
		colorblendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorblendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorblendCreateInfo = {};
		colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorblendCreateInfo.logicOpEnable = VK_FALSE;
		colorblendCreateInfo.attachmentCount = 1;
		colorblendCreateInfo.pAttachments = &colorblendAttachment;

		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.dynamicStateCount = 2;
		dynamicStateCreateInfo.pDynamicStates = dynamicStates;

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

		auto result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS)
		{
			vkDestroyShaderModule(device, vertexShaderModule, nullptr);
			vkDestroyShaderModule(device, fragmentShaderModule, nullptr);

			throw std::runtime_error("Pipeline Layout Creation Failed with " + result);
		}

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount = 2;
		pipelineCreateInfo.pStages = shaderStagesCreateInfo;
		pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
		pipelineCreateInfo.pDepthStencilState = nullptr;
		pipelineCreateInfo.pColorBlendState = &colorblendCreateInfo;
		pipelineCreateInfo.pDynamicState = nullptr;
		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = renderpass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
		if (result != VK_SUCCESS)
		{
			vkDestroyShaderModule(device, vertexShaderModule, nullptr);
			vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
			throw std::runtime_error("Graphics Pipeline creation failed with " + std::to_string(result));
		}

		vkDestroyShaderModule(device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
		return std::make_tuple(pipelineLayout, graphicsPipeline);
	};

	void run()
	{
		// Usings
		using namespace std;
		using std::byte;

		// Variables
		GLFWwindow* window = nullptr;
		Context ctx;
		Renderer renderer;
		VkRenderPass renderpass = VK_NULL_HANDLE;
		VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
		VkPipeline graphicsPipeline = VK_NULL_HANDLE;
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

		renderpass = createRenderpass(ctx.get_device(), renderer.get_swapchainFormat());

		tie(graphicsPipelineLayout, graphicsPipeline) = createGraphicsPipeline(ctx.get_device(), renderpass, renderer.get_swapchainExtent());

		// Framebuffers
		swapchainFramebuffers.resize(renderer.get_swapchainImageViewsCount());
		for (size_t i = 0; i < renderer.get_swapchainImageViewsCount(); i++)
		{
			vector<VkImageView> attachments = {
				renderer.get_swapchainImageView(i)
			};

			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderpass;
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
			renderpassBeginInfo.renderPass = renderpass;
			renderpassBeginInfo.framebuffer = swapchainFramebuffers[i];
			renderpassBeginInfo.renderArea.offset = { 0, 0 };
			renderpassBeginInfo.renderArea.extent = renderer.get_swapchainExtent();

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderpassBeginInfo.clearValueCount = 1;
			renderpassBeginInfo.pClearValues = &clearColor;
			vkCmdBeginRenderPass(commandBuffers[i], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

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

		vkDestroyPipeline(ctx.get_device(), graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(ctx.get_device(), graphicsPipelineLayout, nullptr);
		vkDestroyRenderPass(ctx.get_device(), renderpass, nullptr);

		glfwDestroyWindow(window);
		glfwTerminate();
	}
}

int main(int argc, char* argv[])
{
	blaze::run();

	return 0;
}
