
#include "Renderer.hpp"
#include "util/DeviceSelection.hpp"
#include "util/files.hpp"
#include "util/createFunctions.hpp"
#include "Datatypes.hpp"

#include <algorithm>
#include <string>
#include <stdexcept>

#undef max
#undef min

namespace blaze
{
	void Renderer::renderFrame()
	{
		using namespace std;

		uint32_t imageIndex;
		auto result = vkAcquireNextImageKHR(context.get_device(), swapchain.get(), numeric_limits<uint64_t>::max(), imageAvailableSem[currentFrame], VK_NULL_HANDLE, &imageIndex);

		vkWaitForFences(context.get_device(), 1, &inFlightFences[imageIndex], VK_TRUE, numeric_limits<uint64_t>::max());
		rebuildCommandBuffer(imageIndex);
		updateUniformBuffer(imageIndex, cameraUBO);

		if (result != VK_SUCCESS)
		{
			throw runtime_error("Image acquire failed with " + to_string(result));
		}

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSem[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { renderFinishedSem[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(context.get_device(), 1, &inFlightFences[imageIndex]);

		result = vkQueueSubmit(context.get_graphicsQueue(), 1, &submitInfo, inFlightFences[imageIndex]);
		if (result != VK_SUCCESS)
		{
			throw runtime_error("Queue Submit failed with " + to_string(result));
		}

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapchains[] = { swapchain.get() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapchains;
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(context.get_presentQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || windowResized)
		{
			windowResized = false;
			recreateSwapchain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw runtime_error("Image acquiring failed with " + to_string(result));
		}

		currentFrame = (currentFrame + 1) % max_frames_in_flight;
	}

	std::tuple<VkSwapchainKHR, VkFormat, VkExtent2D> Renderer::createSwapchain() const
	{
		util::SwapchainSupportDetails swapchainSupport = util::getSwapchainSupport(context.get_physicalDevice(), context.get_surface());

		VkSurfaceFormatKHR surfaceFormat = swapchainSupport.formats[0];
		for (const auto& availableFormat : swapchainSupport.formats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				surfaceFormat = availableFormat;
				break;
			}
		}

		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& availablePresentMode : swapchainSupport.presentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = availablePresentMode;
				break;
			}
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				presentMode = availablePresentMode;
			}
		}

		VkExtent2D swapExtent;
		auto capabilities = swapchainSupport.capabilities;
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			swapExtent = capabilities.currentExtent;
		}
		else
		{
			auto [width, height] = getWindowSize();
			swapExtent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			swapExtent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0)
		{
			imageCount = std::min(imageCount, capabilities.maxImageCount);
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = context.get_surface();
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = swapExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = swapchain.get();

		auto queueIndices = context.get_queueFamilyIndices();
		uint32_t queueFamilyIndices[] = { queueIndices.graphicsIndex.value(), queueIndices.presentIndex.value() };
		if (queueIndices.graphicsIndex != queueIndices.presentIndex)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		VkSwapchainKHR swapchain;
		auto result = vkCreateSwapchainKHR(context.get_device(), &createInfo, nullptr, &swapchain);
		if (result == VK_SUCCESS)
		{
			return std::make_tuple(swapchain, surfaceFormat.format, swapExtent);
		}
		throw std::runtime_error("Swapchain creation failed with " + std::to_string(result));
	}

	std::vector<VkImage> Renderer::getSwapchainImages() const
	{
		uint32_t swapchainImageCount = 0;
		std::vector<VkImage> swapchainImages;
		vkGetSwapchainImagesKHR(context.get_device(), swapchain.get(), &swapchainImageCount, nullptr);
		swapchainImages.resize(swapchainImageCount);
		vkGetSwapchainImagesKHR(context.get_device(), swapchain.get(), &swapchainImageCount, swapchainImages.data());
		return swapchainImages;
	}

	std::vector<VkImageView> Renderer::createSwapchainImageViews() const
	{
		std::vector<VkImageView> swapchainImageViews(swapchainImages.size());
		for (size_t i = 0; i < swapchainImages.size(); i++)
		{
			swapchainImageViews[i] = util::createImageView(context.get_device(), swapchainImages[i], swapchainFormat.get());
		}
		return swapchainImageViews;
	}

	VkRenderPass Renderer::createRenderPass() const
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapchainFormat.get();
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
		auto result = vkCreateRenderPass(context.get_device(), &createInfo, nullptr, &renderpass);
		if (result == VK_SUCCESS)
		{
			return renderpass;
		}
		throw std::runtime_error("RenderPass creation failed with " + std::to_string(result));
	}

	VkDescriptorSetLayout Renderer::createUBODescriptorSetLayout() const
	{
		VkDescriptorSetLayout descriptorSetLayout;

		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;

		auto result = vkCreateDescriptorSetLayout(context.get_device(), &layoutInfo, nullptr, &descriptorSetLayout);
		if (result != VK_SUCCESS)
		{
			throw new std::runtime_error("DescriptorSet layout creation failed with " + std::to_string(result));
		}

		return descriptorSetLayout;
	}

	VkDescriptorSetLayout Renderer::createMaterialDescriptorSetLayout() const
	{
		VkDescriptorSetLayout descriptorSetLayout;

		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 0;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		samplerLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &samplerLayoutBinding;

		auto result = vkCreateDescriptorSetLayout(get_device(), &layoutInfo, nullptr, &descriptorSetLayout);
		if (result != VK_SUCCESS)
		{
			throw new std::runtime_error("DescriptorSet layout creation failed with " + std::to_string(result));
		}

		return descriptorSetLayout;
	};

	VkDescriptorPool Renderer::createDescriptorPool() const
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = static_cast<uint32_t>(swapchainImages.size());

		VkDescriptorPoolCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		createInfo.poolSizeCount = 1;
		createInfo.pPoolSizes = &poolSize;
		createInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());

		VkDescriptorPool pool;
		auto result = vkCreateDescriptorPool(context.get_device(), &createInfo, nullptr, &pool);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Descriptor pool creation failed with " + std::to_string(result));
		}
		return pool;
	}

	std::vector<VkDescriptorSet> Renderer::createDescriptorSets() const
	{
		std::vector<VkDescriptorSetLayout> layouts(swapchainImages.size(), uboDescriptorSetLayout.get());
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool.get();
		allocInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
		allocInfo.pSetLayouts = layouts.data();

		std::vector<VkDescriptorSet> descriptorSets(swapchainImages.size());
		auto result = vkAllocateDescriptorSets(context.get_device(), &allocInfo, descriptorSets.data());
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
		}

		for (int i = 0; i < swapchainImages.size(); i++)
		{
			VkDescriptorBufferInfo info = {};
			info.buffer = uniformBuffers[i].get_buffer();
			info.offset = 0;
			info.range = sizeof(UniformBufferObject);

			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.descriptorCount = 1;
			write.dstSet = descriptorSets[i];
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.pBufferInfo = &info;

			vkUpdateDescriptorSets(context.get_device(), 1, &write, 0, nullptr);
		}

		return descriptorSets;
	}

	std::vector<UniformBuffer<UniformBufferObject>> Renderer::createUniformBuffers(const UniformBufferObject& ubo) const
	{
		std::vector<UniformBuffer<UniformBufferObject>> ubos;
		ubos.reserve(swapchainImages.size());
		for (int i = 0; i < swapchainImages.size(); i++)
		{
			ubos.emplace_back(*this, ubo);
		}
		return std::move(ubos);
	}

	std::tuple<VkPipelineLayout, VkPipeline> Renderer::createGraphicsPipeline() const
	{
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkPipeline graphicsPipeline = VK_NULL_HANDLE;

		auto vertexShaderCode = util::loadBinaryFile("shaders/vShader.vert.spv");
		auto fragmentShaderCode = util::loadBinaryFile("shaders/fShader.frag.spv");

		auto vertexShaderModule = util::Managed(util::createShaderModule(context.get_device(), vertexShaderCode), [dev = context.get_device()](VkShaderModule& sm) { vkDestroyShaderModule(dev, sm, nullptr); });
		auto fragmentShaderModule = util::Managed(util::createShaderModule(context.get_device(), fragmentShaderCode), [dev = context.get_device()](VkShaderModule& sm) { vkDestroyShaderModule(dev, sm, nullptr); });

		VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
		vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexShaderStageCreateInfo.module = vertexShaderModule.get();
		vertexShaderStageCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
		fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentShaderStageCreateInfo.module = fragmentShaderModule.get();
		fragmentShaderStageCreateInfo.pName = "main";

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStagesCreateInfo = {
			vertexShaderStageCreateInfo,
			fragmentShaderStageCreateInfo
		};

		auto bindingDescriptions = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescriptions;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapchainExtent.get().width;
		viewport.height = (float)swapchainExtent.get().height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor = {};
		scissor.offset = { 0, 0 };
		scissor.extent = swapchainExtent.get();

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

		std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts = {
			uboDescriptorSetLayout.get(),
			materialDescriptorSetLayout.get()
		};

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

		auto result = vkCreatePipelineLayout(context.get_device(), &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Pipeline Layout Creation Failed with " + result);
		}

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStagesCreateInfo.size());
		pipelineCreateInfo.pStages = shaderStagesCreateInfo.data();
		pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
		pipelineCreateInfo.pDepthStencilState = nullptr;
		pipelineCreateInfo.pColorBlendState = &colorblendCreateInfo;
		pipelineCreateInfo.pDynamicState = nullptr;
		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = renderPass.get();
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		result = vkCreateGraphicsPipelines(context.get_device(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Graphics Pipeline creation failed with " + std::to_string(result));
		}

		return std::make_tuple(pipelineLayout, graphicsPipeline);
	}

	std::vector<VkFramebuffer> Renderer::createFramebuffers() const
	{
		using std::vector;

		vector<VkFramebuffer> swapchainFramebuffers(swapchainImageViews.size());
		for (size_t i = 0; i < swapchainImageViews.size(); i++)
		{
			VkImageView attachments[] = {
				swapchainImageViews[i]
			};

			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass.get();
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = attachments;
			createInfo.width = swapchainExtent.get().width;
			createInfo.height = swapchainExtent.get().height;
			createInfo.layers = 1;

			auto result = vkCreateFramebuffer(context.get_device(), &createInfo, nullptr, &swapchainFramebuffers[i]);
			if (result != VK_SUCCESS)
			{
				for (size_t j = 0; j < i; j++)
				{
					vkDestroyFramebuffer(context.get_device(), swapchainFramebuffers[i], nullptr);
				}
				throw std::runtime_error("Framebuffer creation failed with " + std::to_string(result));
			}
		}
		return swapchainFramebuffers;
	}

	std::vector<VkCommandBuffer> Renderer::allocateCommandBuffers() const
	{
		std::vector<VkCommandBuffer> commandBuffers(swapchainImages.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = context.get_graphicsCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
		auto result = vkAllocateCommandBuffers(context.get_device(), &allocInfo, commandBuffers.data());
		if (result == VK_SUCCESS)
		{
			return commandBuffers;
		}
		throw std::runtime_error("Command buffer alloc failed with " + std::to_string(result));
	}

	void Renderer::recordCommandBuffers()
	{
		for (int i = 0; i < commandBuffers.size(); i++)
		{
			commandBufferDirty[i] = true;
			rebuildCommandBuffer(i);
		}
	}

	void Renderer::rebuildCommandBuffer(int frame)
	{
		if (commandBufferDirty[frame])
		{
			vkWaitForFences(context.get_device(), 1, &inFlightFences[frame], VK_TRUE, std::numeric_limits<uint64_t>::max());

			VkCommandBufferBeginInfo commandBufferBeginInfo = {};
			commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

			auto result = vkBeginCommandBuffer(commandBuffers[frame], &commandBufferBeginInfo);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Begin Command Buffer failed with " + std::to_string(result));
			}

			VkRenderPassBeginInfo renderpassBeginInfo = {};
			renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderpassBeginInfo.renderPass = renderPass.get();
			renderpassBeginInfo.framebuffer = framebuffers[frame];
			renderpassBeginInfo.renderArea.offset = { 0, 0 };
			renderpassBeginInfo.renderArea.extent = swapchainExtent.get();

			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderpassBeginInfo.clearValueCount = 1;
			renderpassBeginInfo.pClearValues = &clearColor;
			vkCmdBeginRenderPass(commandBuffers[frame], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());

			vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0, 1, &descriptorSets[frame], 0, nullptr);

			for (auto& cmd : renderCommands)
			{
				cmd(commandBuffers[frame], graphicsPipelineLayout.get(), frame);
			}

			vkCmdEndRenderPass(commandBuffers[frame]);

			result = vkEndCommandBuffer(commandBuffers[frame]);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("End Command Buffer failed with " + std::to_string(result));
			}

			commandBufferDirty[frame] = false;
		}
	}

	std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> Renderer::createSyncObjects() const
	{
		using namespace std;

		vector<VkSemaphore> startSems(swapchainImages.size());
		vector<VkSemaphore> endSems(swapchainImages.size());
		vector<VkFence> blockeFences(swapchainImages.size());

		for (auto& sem : startSems)
		{
			sem = util::createSemaphore(context.get_device());
		}

		for (auto& sem : endSems)
		{
			sem = util::createSemaphore(context.get_device());
		}

		for (auto& fence : blockeFences)
		{
			fence = util::createFence(context.get_device());
		}

		return make_tuple(startSems, endSems, blockeFences);
	}

	VkCommandBuffer Renderer::startTransferCommands() const
	{
		VkCommandBuffer commandBuffer;
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = get_transferCommandPool();
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		auto result = vkAllocateCommandBuffers(context.get_device(), &allocInfo, &commandBuffer);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Command buffer alloc failed with " + std::to_string(result));
		}

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Begin Command Buffer failed with " + std::to_string(result));
		}

		return commandBuffer;
	}

	void Renderer::endTransferCommands(VkCommandBuffer commandBuffer) const 
	{
		auto result = vkEndCommandBuffer(commandBuffer);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("End Command Buffer failed with " + std::to_string(result));
		}

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		VkFence fence = util::createFence(context.get_device());
		vkResetFences(context.get_device(), 1, &fence);

		result = vkQueueSubmit(get_transferQueue(), 1, &submitInfo, fence);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Submit Command Buffer failed with " + std::to_string(result));
		}

		vkWaitForFences(context.get_device(), 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
		vkDestroyFence(context.get_device(), fence, nullptr);
		vkFreeCommandBuffers(context.get_device(), get_transferCommandPool(), 1, &commandBuffer);
	}
}
