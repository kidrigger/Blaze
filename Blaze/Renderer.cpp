
#include "Renderer.hpp"
#include "util/DeviceSelection.hpp"
#include "util/files.hpp"
#include "util/createFunctions.hpp"
#include "Datatypes.hpp"
#include "Primitives.hpp"
#include "util/processing.hpp"

#include <algorithm>
#include <string>
#include <stdexcept>

#undef max
#undef min

namespace blaze
{
	const float PI = 3.1415926535897932384626433f;

	void Renderer::renderFrame()
	{
		using namespace std;

		uint32_t imageIndex;
		auto result = vkAcquireNextImageKHR(context.get_device(), swapchain.get_swapchain(), numeric_limits<uint64_t>::max(), imageAvailableSem[currentFrame], VK_NULL_HANDLE, &imageIndex);

		vkWaitForFences(context.get_device(), 1, &inFlightFences[imageIndex], VK_TRUE, numeric_limits<uint64_t>::max());
		rebuildCommandBuffer(imageIndex);
		set_lightUBO(shadowCaster.getLightsData());
		updateUniformBuffer(imageIndex, rendererUBO);
		updateUniformBuffer(imageIndex, settingsUBO);

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

		VkSwapchainKHR swapchains[] = { swapchain.get_swapchain() };
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

	VkRenderPass Renderer::createRenderPass() const
	{
		return util::createRenderPass(context.get_device(), swapchain.get_format(), VK_FORMAT_D32_SFLOAT);
	}

	VkDescriptorSetLayout Renderer::createUBODescriptorSetLayout() const
	{
		std::vector<VkDescriptorSetLayoutBinding> uboLayoutBindings = {
			{
				0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				nullptr
			},
			{
				1,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1,
				VK_SHADER_STAGE_FRAGMENT_BIT,
				nullptr
			}
		};

		return util::createDescriptorSetLayout(context.get_device(), uboLayoutBindings);
	}

	VkDescriptorSetLayout Renderer::createEnvironmentDescriptorSetLayout() const
	{
		std::vector<VkDescriptorSetLayoutBinding> envLayoutBindings;
		for (uint32_t i = 0; i < 4; i++)
		{
			envLayoutBindings.push_back({
					i,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					nullptr
				});
		}

		return util::createDescriptorSetLayout(context.get_device(), envLayoutBindings);
	}

	VkDescriptorSetLayout Renderer::createMaterialDescriptorSetLayout() const
	{
		std::vector<VkDescriptorSetLayoutBinding> samplerLayoutBindings;

		for (uint32_t bindingLocation = 0; bindingLocation < 5; bindingLocation++)
		{
			samplerLayoutBindings.push_back(
				{
					bindingLocation,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					nullptr
				});
		}

		return util::createDescriptorSetLayout(context.get_device(), samplerLayoutBindings);
	};

	VkDescriptorPool Renderer::createDescriptorPool() const
	{
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				1
			}
		};
		return util::createDescriptorPool(context.get_device(), poolSizes, 2u * swapchain.get_imageCount());
	}

	std::vector<VkDescriptorSet> Renderer::createCameraDescriptorSets() const
	{
		std::vector<VkDescriptorSetLayout> layouts(swapchain.get_imageCount(), uboDescriptorSetLayout.get());
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool.get();
		allocInfo.descriptorSetCount = swapchain.get_imageCount();
		allocInfo.pSetLayouts = layouts.data();

		std::vector<VkDescriptorSet> descriptorSets(swapchain.get_imageCount());
		auto result = vkAllocateDescriptorSets(context.get_device(), &allocInfo, descriptorSets.data());
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
		}

		for (uint32_t i = 0; i < swapchain.get_imageCount(); i++)
		{
			VkDescriptorBufferInfo info = {};
			info.buffer = rendererUniformBuffers[i].get_buffer();
			info.offset = 0;
			info.range = sizeof(RendererUniformBufferObject);

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

		for (uint32_t i = 0; i < swapchain.get_imageCount(); i++)
		{
			VkDescriptorBufferInfo info = {};
			info.buffer = settingsUniformBuffers[i].get_buffer();
			info.offset = 0;
			info.range = sizeof(SettingsUniformBufferObject);

			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.descriptorCount = 1;
			write.dstSet = descriptorSets[i];
			write.dstBinding = 1;
			write.dstArrayElement = 0;
			write.pBufferInfo = &info;

			vkUpdateDescriptorSets(context.get_device(), 1, &write, 0, nullptr);
		}

		return descriptorSets;
	}

	std::vector<UniformBuffer<RendererUniformBufferObject>> Renderer::createUniformBuffers(const RendererUniformBufferObject& ubo) const
	{
		std::vector<UniformBuffer<RendererUniformBufferObject>> ubos;
		ubos.reserve(swapchain.get_imageCount());
		for (uint32_t i = 0; i < swapchain.get_imageCount(); i++)
		{
			ubos.emplace_back(context, ubo);
		}
		return std::move(ubos);
	}

	std::vector<UniformBuffer<SettingsUniformBufferObject>> Renderer::createUniformBuffers(const SettingsUniformBufferObject& ubo) const
	{
		std::vector<UniformBuffer<SettingsUniformBufferObject>> ubos;
		ubos.reserve(swapchain.get_imageCount());
		for (uint32_t i = 0; i < swapchain.get_imageCount(); i++)
		{
			ubos.emplace_back(context, ubo);
		}
		return std::move(ubos);
	}

	std::tuple<VkPipelineLayout, VkPipeline, VkPipeline> Renderer::createGraphicsPipeline() const
	{
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

		{
			std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
				uboDescriptorSetLayout.get(),
				materialDescriptorSetLayout.get(),
				environmentDescriptorSetLayout.get(),
				shadowCaster.get_shadowLayout()
			};

			std::vector<VkPushConstantRange> pushConstantRanges;

			{
				VkPushConstantRange pushConstantRange = {};

				pushConstantRange.offset = 0;
				pushConstantRange.size = sizeof(ModelPushConstantBlock);
				pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
				pushConstantRanges.push_back(pushConstantRange);

				pushConstantRange.offset = sizeof(ModelPushConstantBlock);
				pushConstantRange.size = sizeof(MaterialPushConstantBlock);
				pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
				pushConstantRanges.push_back(pushConstantRange);
			}

			pipelineLayout = util::createPipelineLayout(context.get_device(), descriptorSetLayouts, pushConstantRanges);
		}
		
		auto graphicsPipeline = util::createGraphicsPipeline(context.get_device(), pipelineLayout, renderPass.get(), swapchain.get_extent(),
			"shaders/vShader.vert.spv", "shaders/fShader.frag.spv");
		auto skyboxPipeline = util::createGraphicsPipeline(context.get_device(), pipelineLayout, renderPass.get(), swapchain.get_extent(),
			"shaders/vSkybox.vert.spv", "shaders/fSkybox.frag.spv", {}, VK_CULL_MODE_FRONT_BIT, VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

		return std::make_tuple(pipelineLayout, graphicsPipeline, skyboxPipeline);
	}

	std::vector<VkFramebuffer> Renderer::createRenderFramebuffers() const
	{
		using namespace std;

		vector<VkFramebuffer> frameBuffers(swapchain.get_imageCount());
		for (uint32_t i = 0; i < swapchain.get_imageCount(); i++)
		{
			vector<VkImageView> attachments = {
				swapchain.get_imageView(i),
				depthBufferTexture.get_imageView()
			};

			VkFramebufferCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass.get();
			createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			createInfo.pAttachments = attachments.data();
			createInfo.width = swapchain.get_extent().width;
			createInfo.height = swapchain.get_extent().height;
			createInfo.layers = 1;

			auto result = vkCreateFramebuffer(context.get_device(), &createInfo, nullptr, &frameBuffers[i]);
			if (result != VK_SUCCESS)
			{
				for (size_t j = 0; j < i; j++)
				{
					vkDestroyFramebuffer(context.get_device(), frameBuffers[i], nullptr);
				}
				throw runtime_error("Framebuffer creation failed with " + std::to_string(result));
			}
		}
		return frameBuffers;
	}

	std::vector<VkCommandBuffer> Renderer::allocateCommandBuffers() const
	{
		std::vector<VkCommandBuffer> commandBuffers(swapchain.get_imageCount());

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
			rebuildCommandBuffer(i);
		}
	}

	void Renderer::rebuildCommandBuffer(int frame)
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

		shadowCaster.cast(context, commandBuffers[frame], drawables);

		VkRenderPassBeginInfo renderpassBeginInfo = {};
		renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpassBeginInfo.renderPass = renderPass.get();
		renderpassBeginInfo.framebuffer = renderFramebuffers[frame];
		renderpassBeginInfo.renderArea.offset = { 0, 0 };
		renderpassBeginInfo.renderArea.extent = swapchain.get_extent();

		std::array<VkClearValue, 2> clearColor;
		clearColor[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearColor[1].depthStencil = { 1.0f, 0 };
		renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
		renderpassBeginInfo.pClearValues = clearColor.data();
		vkCmdBeginRenderPass(commandBuffers[frame], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());

		vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0, 1, &uboDescriptorSets[frame], 0, nullptr);
		if (environmentDescriptor != VK_NULL_HANDLE)
		{
			vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 2, 1, &environmentDescriptor, 0, nullptr);
		}
		shadowCaster.bind(commandBuffers[frame], graphicsPipelineLayout.get(), 3);

		for (Drawable* cmd : drawables)
		{
			cmd->draw(commandBuffers[frame], graphicsPipelineLayout.get());
		}

		vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.get());
		skyboxCommand(commandBuffers[frame], graphicsPipelineLayout.get(), frame);

		vkCmdEndRenderPass(commandBuffers[frame]);

		gui.draw(commandBuffers[frame], frame);

		result = vkEndCommandBuffer(commandBuffers[frame]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("End Command Buffer failed with " + std::to_string(result));
		}
	}

	std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> Renderer::createSyncObjects() const
	{
		using namespace std;

		vector<VkSemaphore> startSems(swapchain.get_imageCount());
		vector<VkSemaphore> endSems(swapchain.get_imageCount());
		vector<VkFence> blockeFences(swapchain.get_imageCount());

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

	Texture2D Renderer::createDepthBuffer() const
	{ 
		VkFormat format = util::findSupportedFormat(context.get_physicalDevice(),
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		ImageData2D imageData = {};
		imageData.format = format;
		imageData.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		imageData.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageData.height = swapchain.get_extent().height;
		imageData.width = swapchain.get_extent().width;
		imageData.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		imageData.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageData.numChannels = 1;
		imageData.size = swapchain.get_extent().width * swapchain.get_extent().height;
		return Texture2D(context, imageData);
	}

	TextureCube Renderer::createIrradianceCube(VkDescriptorSet environment) const
	{
		struct PCB
		{
			float deltaPhi{ (2.0f * PI) / 180.0f };
			float deltaTheta{ (0.5f * PI) / 64.0f };
		} pcb = {};

		util::Texture2CubemapInfo<PCB> info = {
			"shaders/vIrradianceMultiview.vert.spv",
			"shaders/fIrradiance.frag.spv",
			environment,
			get_environmentLayout(),
			64u,
			pcb
		};

		return util::Process<PCB>::convertDescriptorToCubemap(context, info);
	}

	TextureCube Renderer::createPrefilteredCube(VkDescriptorSet environment) const
	{
		struct PCB
		{
			float roughness;
			float miplevel;
		};

		util::Texture2CubemapInfo<PCB> info = {
			"shaders/vIrradiance.vert.spv",
			"shaders/fPrefilter.frag.spv",
			environment,
			get_environmentLayout(),
			128u,
			{0,0}
		};

		const uint32_t dim = info.cube_side;

		util::Managed<VkPipelineLayout> irPipelineLayout;
		util::Managed<VkPipeline> irPipeline;
		util::Managed<VkRenderPass> irRenderPass;
		util::Managed<VkFramebuffer> irFramebuffer;

		VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

		// Setup the TextureCube
		ImageDataCube idc{};
		idc.height = dim;
		idc.width = dim;
		idc.numChannels = 4;
		idc.size = 4 * 6 * dim * dim;
		idc.layerSize = 4 * dim * dim;
		idc.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		idc.format = format;
		idc.access = VK_ACCESS_TRANSFER_WRITE_BIT;
		TextureCube irradianceMap(context, idc, true);

		ImageData2D id2d{};
		id2d.height = dim;
		id2d.width = dim;
		id2d.numChannels = 4;
		id2d.size = 4 * dim * dim;
		id2d.format = format;
		id2d.usage = id2d.usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		id2d.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		id2d.access = VK_ACCESS_SHADER_WRITE_BIT;
		Texture2D fbColorAttachment(context, id2d, false);

		struct CubePushConstantBlock
		{
			glm::mat4 mvp;
		};

		{
			std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
				info.layout
			};

			std::vector<VkPushConstantRange> pushConstantRanges;
			{
				VkPushConstantRange pushConstantRange = {};
				pushConstantRange.offset = 0;
				pushConstantRange.size = sizeof(CubePushConstantBlock);
				pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
				pushConstantRanges.push_back(pushConstantRange);
				pushConstantRange.offset = sizeof(CubePushConstantBlock);
				pushConstantRange.size = sizeof(PCB);
				pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
				pushConstantRanges.push_back(pushConstantRange);
			}

			irPipelineLayout = util::Managed(util::createPipelineLayout(context.get_device(), descriptorSetLayouts, pushConstantRanges), [dev = context.get_device()](VkPipelineLayout& lay){vkDestroyPipelineLayout(dev, lay, nullptr); });
		}

		irRenderPass = util::Managed(util::createRenderPass(context.get_device(), format), [dev = context.get_device()](VkRenderPass& pass) { vkDestroyRenderPass(dev, pass, nullptr); });
		
		{
			std::vector<VkDynamicState> dynamicStateEnables = {
				VK_DYNAMIC_STATE_VIEWPORT
			};

			VkPipeline tPipeline = util::createGraphicsPipeline(context.get_device(), irPipelineLayout.get(), irRenderPass.get(), { dim,dim },
				info.vert_shader, info.frag_shader, dynamicStateEnables, VK_CULL_MODE_FRONT_BIT);
			irPipeline = util::Managed(tPipeline, [dev = context.get_device()](VkPipeline& pipe) { vkDestroyPipeline(dev, pipe, nullptr); });
		}

		{
			VkFramebuffer fbo = VK_NULL_HANDLE;
			VkFramebufferCreateInfo fbCreateInfo = {};
			fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbCreateInfo.width = dim;
			fbCreateInfo.height = dim;
			fbCreateInfo.layers = 1;
			fbCreateInfo.renderPass = irRenderPass.get();
			fbCreateInfo.attachmentCount = 1;
			fbCreateInfo.pAttachments = &fbColorAttachment.get_imageView();
			vkCreateFramebuffer(context.get_device(), &fbCreateInfo, nullptr, &fbo);
			irFramebuffer = util::Managed(fbo, [dev = context.get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
		}

		auto cube = getUVCube(context);

		glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f);

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X (Outside in - so NEG_X face)
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// NEGATIVE_X (Outside in - so POS_X face)
			glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// POSITIVE_Y
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
			// NEGATIVE_Y
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			// POSITIVE_Z
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// NEGATIVE_Z
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		};

		CubePushConstantBlock pcb{};

		uint32_t totalMips = irradianceMap.get_miplevels();
		uint32_t mipsize = dim;
		auto cmdBuffer = context.startCommandBufferRecord();
		for (uint32_t miplevel = 0; miplevel < totalMips; miplevel++)
		{
			for (int face = 0; face < 6; face++)
			{

				// RENDERPASSES
				VkViewport viewport = {};
				viewport.x = 0.0f;
				viewport.y = static_cast<float>(mipsize);
				viewport.width = static_cast<float>(mipsize);
				viewport.height = -static_cast<float>(mipsize);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

				VkRenderPassBeginInfo renderpassBeginInfo = {};
				renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderpassBeginInfo.renderPass = irRenderPass.get();
				renderpassBeginInfo.framebuffer = irFramebuffer.get();
				renderpassBeginInfo.renderArea.offset = { 0, 0 };
				renderpassBeginInfo.renderArea.extent = { mipsize, mipsize };

				std::vector<VkClearValue> clearColor(1);
				clearColor[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
				renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
				renderpassBeginInfo.pClearValues = clearColor.data();

				vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipeline.get());
				vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipelineLayout.get(), 0, 1, &info.descriptor, 0, nullptr);

				pcb.mvp = proj * matrices[face];
				info.pcb.roughness = (float)miplevel / (float)(totalMips - 1);
				info.pcb.miplevel = static_cast<float>(miplevel);
				vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CubePushConstantBlock), &pcb);
				vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(CubePushConstantBlock), sizeof(PCB), &info.pcb);

				VkDeviceSize offsets = { 0 };

				vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &cube.get_vertexBuffer(), &offsets);
				vkCmdBindIndexBuffer(cmdBuffer, cube.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(cmdBuffer, cube.get_indexCount(), 1, 0, 0, 0);

				vkCmdEndRenderPass(cmdBuffer);

				fbColorAttachment.transferLayout(cmdBuffer,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = face;
				copyRegion.dstSubresource.mipLevel = miplevel;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(mipsize);
				copyRegion.extent.height = static_cast<uint32_t>(mipsize);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(cmdBuffer,
					fbColorAttachment.get_image(), fbColorAttachment.get_imageInfo().imageLayout,
					irradianceMap.get_image(), irradianceMap.get_imageInfo().imageLayout,
					1, &copyRegion);

				fbColorAttachment.transferLayout(cmdBuffer,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			}

			mipsize /= 2;
		}

		irradianceMap.transferLayout(cmdBuffer,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			0, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		context.flushCommandBuffer(cmdBuffer);

		return irradianceMap;
	}

	Texture2D Renderer::createBrdfLut() const
	{
		const uint32_t dim = 512;

		util::Managed<VkPipelineLayout> irPipelineLayout;
		util::Managed<VkPipeline> irPipeline;
		util::Managed<VkRenderPass> irRenderPass;
		util::Managed<VkFramebuffer> irFramebuffer;

		VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

		ImageData2D id2d{};
		id2d.height = dim;
		id2d.width = dim;
		id2d.numChannels = 4;
		id2d.size = 4 * dim * dim;
		id2d.format = format;
		id2d.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		id2d.access = VK_ACCESS_TRANSFER_WRITE_BIT;
		id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		Texture2D lut(context, id2d, false);

		id2d.usage = id2d.usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		id2d.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		id2d.access = VK_ACCESS_SHADER_WRITE_BIT;
		id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		Texture2D fbColorAttachment(context, id2d, false);

		struct CubePushConstantBlock
		{
			glm::mat4 mvp;
		};

		{
			VkPushConstantRange pcr = {};
			pcr.offset = 0;
			pcr.size = sizeof(CubePushConstantBlock);
			pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

			irPipelineLayout = util::Managed(util::createPipelineLayout(context.get_device(), std::vector<VkDescriptorSetLayout>(), std::vector<VkPushConstantRange>{pcr}), [dev = context.get_device()](VkPipelineLayout& lay){vkDestroyPipelineLayout(dev, lay, nullptr); });
		}

		irRenderPass = util::Managed(util::createRenderPass(context.get_device(), format), [dev = context.get_device()](VkRenderPass& pass) { vkDestroyRenderPass(dev, pass, nullptr); });

		{
			auto tPipeline = util::createGraphicsPipeline(context.get_device(), irPipelineLayout.get(), irRenderPass.get(), { dim, dim },
				"shaders/vBrdfLut.vert.spv", "shaders/fBrdfLut.frag.spv", {}, VK_CULL_MODE_FRONT_BIT);
			irPipeline = util::Managed(tPipeline, [dev = context.get_device()](VkPipeline& pipe) { vkDestroyPipeline(dev, pipe, nullptr); });
		}

		{
			VkFramebuffer fbo = VK_NULL_HANDLE;
			VkFramebufferCreateInfo fbCreateInfo = {};
			fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbCreateInfo.width = dim;
			fbCreateInfo.height = dim;
			fbCreateInfo.layers = 1;
			fbCreateInfo.renderPass = irRenderPass.get();
			fbCreateInfo.attachmentCount = 1;
			fbCreateInfo.pAttachments = &fbColorAttachment.get_imageView();
			vkCreateFramebuffer(context.get_device(), &fbCreateInfo, nullptr, &fbo);
			irFramebuffer = util::Managed(fbo, [dev = context.get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
		}

		auto rect = getUVRect(context);

		glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f);

		CubePushConstantBlock pcb{
			glm::mat4(1.0f)
		};

		auto cmdBuffer = context.startCommandBufferRecord();

		// RENDERPASSES

		VkRenderPassBeginInfo renderpassBeginInfo = {};
		renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpassBeginInfo.renderPass = irRenderPass.get();
		renderpassBeginInfo.framebuffer = irFramebuffer.get();
		renderpassBeginInfo.renderArea.offset = { 0, 0 };
		renderpassBeginInfo.renderArea.extent = { dim, dim };

		std::array<VkClearValue, 1> clearColor;
		clearColor[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
		renderpassBeginInfo.pClearValues = clearColor.data();

		vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipeline.get());

		vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CubePushConstantBlock), &pcb);

		VkDeviceSize offsets = { 0 };

		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &rect.get_vertexBuffer(), &offsets);
		vkCmdBindIndexBuffer(cmdBuffer, rect.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmdBuffer, rect.get_indexCount(), 1, 0, 0, 0);

		vkCmdEndRenderPass(cmdBuffer);

		fbColorAttachment.transferLayout(cmdBuffer,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkImageCopy copyRegion = {};

		copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.srcSubresource.baseArrayLayer = 0;
		copyRegion.srcSubresource.mipLevel = 0;
		copyRegion.srcSubresource.layerCount = 1;
		copyRegion.srcOffset = { 0, 0, 0 };

		copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.dstSubresource.baseArrayLayer = 0;
		copyRegion.dstSubresource.mipLevel = 0;
		copyRegion.dstSubresource.layerCount = 1;
		copyRegion.dstOffset = { 0, 0, 0 };

		copyRegion.extent.width = static_cast<uint32_t>(dim);
		copyRegion.extent.height = static_cast<uint32_t>(dim);
		copyRegion.extent.depth = 1;

		vkCmdCopyImage(cmdBuffer,
			fbColorAttachment.get_image(), fbColorAttachment.get_imageInfo().imageLayout,
			lut.get_image(), lut.get_imageInfo().imageLayout,
			1, &copyRegion);

		lut.transferLayout(cmdBuffer,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

		context.flushCommandBuffer(cmdBuffer);

		return lut;
	}
}
