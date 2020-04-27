
#include "ForwardRenderer.hpp"

#include <Datatypes.hpp>
#include <Primitives.hpp>
#include <util/DeviceSelection.hpp>
#include <util/createFunctions.hpp>
#include <util/files.hpp>
#include <util/processing.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

#undef max
#undef min

namespace blaze
{

void ForwardRenderer::renderFrame()
{
	using namespace std;

	uint32_t imageIndex;
	auto result =
		vkAcquireNextImageKHR(context.get_device(), swapchain.get_swapchain(), numeric_limits<uint64_t>::max(),
							  imageAvailableSem[currentFrame], VK_NULL_HANDLE, &imageIndex);

	vkWaitForFences(context.get_device(), 1, &inFlightFences[imageIndex], VK_TRUE, numeric_limits<uint64_t>::max());
	set_cameraUBO(camera->getUbo());
	lightSystem.update(camera);
	set_lightUBO(lightSystem.getLightsData());
	updateUniformBuffer(imageIndex, rendererUBO);
	updateUniformBuffer(imageIndex, settingsUBO);
	rebuildCommandBuffer(imageIndex);

	if (result != VK_SUCCESS)
	{
		throw runtime_error("Image acquire failed with " + to_string(result));
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {imageAvailableSem[currentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

	VkSemaphore signalSemaphores[] = {renderFinishedSem[currentFrame]};
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

	VkSwapchainKHR swapchains[] = {swapchain.get_swapchain()};
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

	currentFrame = (currentFrame + 1) % maxFrameInFlight;
}

VkRenderPass ForwardRenderer::createRenderPass() const
{
	return util::createRenderPass(context.get_device(), swapchain.get_format(), VK_FORMAT_D32_SFLOAT);
}

VkDescriptorSetLayout ForwardRenderer::createUBODescriptorSetLayout() const
{
	std::vector<VkDescriptorSetLayoutBinding> uboLayoutBindings = {
		{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
		{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

	return util::createDescriptorSetLayout(context.get_device(), uboLayoutBindings);
}

VkDescriptorSetLayout ForwardRenderer::createEnvironmentDescriptorSetLayout() const
{
	std::vector<VkDescriptorSetLayoutBinding> envLayoutBindings;
	for (uint32_t i = 0; i < 4; i++)
	{
		envLayoutBindings.push_back(
			{i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
	}

	return util::createDescriptorSetLayout(context.get_device(), envLayoutBindings);
}

VkDescriptorSetLayout ForwardRenderer::createMaterialDescriptorSetLayout() const
{
	std::vector<VkDescriptorSetLayoutBinding> samplerLayoutBindings;

	for (uint32_t bindingLocation = 0; bindingLocation < 5; bindingLocation++)
	{
		samplerLayoutBindings.push_back(
			{bindingLocation, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
	}

	return util::createDescriptorSetLayout(context.get_device(), samplerLayoutBindings);
};

VkDescriptorPool ForwardRenderer::createDescriptorPool() const
{
	std::vector<VkDescriptorPoolSize> poolSizes = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2u * swapchain.get_imageCount()}};
	return util::createDescriptorPool(context.get_device(), poolSizes, swapchain.get_imageCount());
}

std::vector<VkDescriptorSet> ForwardRenderer::createCameraDescriptorSets() const
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
		VkDescriptorBufferInfo info = rendererUniformBuffers[i].get_descriptorInfo();

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
		VkDescriptorBufferInfo info = settingsUniformBuffers[i].get_descriptorInfo();

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

std::vector<UBO<RendererUBlock>> ForwardRenderer::createUniformBuffers(
	const RendererUBlock& ubo) const
{
	std::vector<UBO<RendererUBlock>> ubos;
	ubos.reserve(swapchain.get_imageCount());
	for (uint32_t i = 0; i < swapchain.get_imageCount(); i++)
	{
		ubos.emplace_back(&context, ubo);
	}
	return std::move(ubos);
}

std::vector<UBO<SettingsUBlock>> ForwardRenderer::createUniformBuffers(
	const SettingsUBlock& ubo) const
{
	std::vector<UBO<SettingsUBlock>> ubos;
	ubos.reserve(swapchain.get_imageCount());
	for (uint32_t i = 0; i < swapchain.get_imageCount(); i++)
	{
		ubos.emplace_back(&context, ubo);
	}
	return std::move(ubos);
}

std::tuple<VkPipelineLayout, VkPipeline, VkPipeline> ForwardRenderer::createGraphicsPipeline() const
{
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	{
		std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
			uboDescriptorSetLayout.get(), materialDescriptorSetLayout.get(), environmentDescriptorSetLayout.get(),
			lightSystem.get_shadowLayout()};

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

	auto graphicsPipeline =
		util::createGraphicsPipeline(context.get_device(), pipelineLayout, renderPass.get(), swapchain.get_extent(),
									 "shaders/vShader.vert.spv", "shaders/fShader.frag.spv");
	auto skyboxPipeline = util::createGraphicsPipeline(
		context.get_device(), pipelineLayout, renderPass.get(), swapchain.get_extent(), "shaders/vSkybox.vert.spv",
		"shaders/fSkybox.frag.spv", {}, VK_CULL_MODE_FRONT_BIT, VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

	return std::make_tuple(pipelineLayout, graphicsPipeline, skyboxPipeline);
}

std::vector<VkFramebuffer> ForwardRenderer::createRenderFramebuffers() const
{
	using namespace std;

	vector<VkFramebuffer> frameBuffers(swapchain.get_imageCount());
	for (uint32_t i = 0; i < swapchain.get_imageCount(); i++)
	{
		vector<VkImageView> attachments = {swapchain.get_imageView(i), depthBufferTexture.get_imageView()};

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

std::vector<VkCommandBuffer> ForwardRenderer::allocateCommandBuffers() const
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

void ForwardRenderer::recordCommandBuffers()
{
	for (int i = 0; i < commandBuffers.size(); i++)
	{
		rebuildCommandBuffer(i);
	}
}

void ForwardRenderer::rebuildCommandBuffer(int frame)
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

	lightSystem.cast(context, camera, commandBuffers[frame], drawables.get_data());

	VkRenderPassBeginInfo renderpassBeginInfo = {};
	renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpassBeginInfo.renderPass = renderPass.get();
	renderpassBeginInfo.framebuffer = renderFramebuffers[frame];
	renderpassBeginInfo.renderArea.offset = {0, 0};
	renderpassBeginInfo.renderArea.extent = swapchain.get_extent();

	std::array<VkClearValue, 2> clearColor;
	clearColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
	clearColor[1].depthStencil = {1.0f, 0};
	renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
	renderpassBeginInfo.pClearValues = clearColor.data();
	vkCmdBeginRenderPass(commandBuffers[frame], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.get());

	vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 0, 1,
							&uboDescriptorSets[frame], 0, nullptr);
	if (environmentDescriptor != VK_NULL_HANDLE)
	{
		vkCmdBindDescriptorSets(commandBuffers[frame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout.get(), 2,
								1, &environmentDescriptor, 0, nullptr);
	}
	lightSystem.bind(commandBuffers[frame], graphicsPipelineLayout.get(), 3);

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

std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> ForwardRenderer::createSyncObjects() const
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

Texture2D ForwardRenderer::createDepthBuffer() const
{
	VkFormat format = util::findSupportedFormat(
		context.get_physicalDevice(), {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

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

ForwardRenderer::ForwardRenderer(GLFWwindow* window, bool enableValidationLayers) noexcept : context(window, enableValidationLayers)
{
	using namespace std;
	using namespace util;

	skyboxCommand = [](VkCommandBuffer cb, VkPipelineLayout lay, uint32_t frameCount) {};

	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		ForwardRenderer* renderer = reinterpret_cast<ForwardRenderer*>(glfwGetWindowUserPointer(window));
		renderer->windowResized = true;
	});

	try
	{

		swapchain = Swapchain(&context);

		lightSystem = LightSystem(context);

		depthBufferTexture = createDepthBuffer();

		renderPass = vkw::RenderPass(createRenderPass(), context.get_device());

		rendererUniformBuffers = createUniformBuffers(rendererUBO);
		settingsUniformBuffers = createUniformBuffers(settingsUBO);
		uboDescriptorSetLayout = vkw::DescriptorSetLayout(createUBODescriptorSetLayout(), context.get_device());
		environmentDescriptorSetLayout = vkw::DescriptorSetLayout(createEnvironmentDescriptorSetLayout(), context.get_device());
		materialDescriptorSetLayout = vkw::DescriptorSetLayout(createMaterialDescriptorSetLayout(), context.get_device());

		descriptorPool = vkw::DescriptorPool(createDescriptorPool(), context.get_device());
		uboDescriptorSets = createCameraDescriptorSets();

		{
			auto [gPipelineLayout, gPipeline, sbPipeline] = createGraphicsPipeline();
			graphicsPipelineLayout = vkw::PipelineLayout(gPipelineLayout, context.get_device());
			graphicsPipeline = vkw::Pipeline(gPipeline, context.get_device());
			skyboxPipeline = vkw::Pipeline(sbPipeline, context.get_device());
		}

		renderFramebuffers = ManagedVector(createRenderFramebuffers(), [dev = context.get_device()](VkFramebuffer& fb) {
			vkDestroyFramebuffer(dev, fb, nullptr);
		});
		commandBuffers = ManagedVector<VkCommandBuffer, false>(
			allocateCommandBuffers(),
			[dev = context.get_device(), pool = context.get_graphicsCommandPool()](vector<VkCommandBuffer>& buf) {
				vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(buf.size()), buf.data());
			});

		maxFrameInFlight = static_cast<uint32_t>(commandBuffers.size());

		{
			auto [startSems, endSems, fences] = createSyncObjects();
			imageAvailableSem = vkw::SemaphoreVector(std::move(startSems), context.get_device());
			renderFinishedSem = vkw::SemaphoreVector(std::move(endSems), context.get_device());
			inFlightFences = vkw::FenceVector(std::move(fences), context.get_device());
		}

		gui = GUI(&context, &swapchain);

		recordCommandBuffers();

		isComplete = true;
	}
	catch (std::exception& e)
	{
		std::cerr << "RENDERER_CREATION_FAILED: " << e.what() << std::endl;
		isComplete = false;
	}
}

ForwardRenderer::ForwardRenderer(ForwardRenderer&& other) noexcept
	: isComplete(other.isComplete), context(std::move(other.context)), gui(std::move(other.gui)),
	  swapchain(std::move(other.swapchain)), depthBufferTexture(std::move(other.depthBufferTexture)),
	  renderPass(std::move(other.renderPass)), uboDescriptorSetLayout(std::move(other.uboDescriptorSetLayout)),
	  environmentDescriptorSetLayout(std::move(other.environmentDescriptorSetLayout)),
	  materialDescriptorSetLayout(std::move(other.materialDescriptorSetLayout)),
	  descriptorPool(std::move(other.descriptorPool)), uboDescriptorSets(std::move(other.uboDescriptorSets)),
	  rendererUniformBuffers(std::move(other.rendererUniformBuffers)),
	  settingsUniformBuffers(std::move(other.settingsUniformBuffers)), rendererUBO(other.rendererUBO),
	  settingsUBO(other.settingsUBO), graphicsPipelineLayout(std::move(other.graphicsPipelineLayout)),
	  graphicsPipeline(std::move(other.graphicsPipeline)), skyboxPipeline(std::move(other.skyboxPipeline)),
	  renderFramebuffers(std::move(other.renderFramebuffers)), commandBuffers(std::move(other.commandBuffers)),
	  imageAvailableSem(std::move(other.imageAvailableSem)), renderFinishedSem(std::move(other.renderFinishedSem)),
	  inFlightFences(std::move(other.inFlightFences)), skyboxCommand(std::move(other.skyboxCommand)),
	  drawables(std::move(other.drawables)), environmentDescriptor(other.environmentDescriptor),
	  lightSystem(std::move(other.lightSystem))
{
}

ForwardRenderer& ForwardRenderer::operator=(ForwardRenderer&& other) noexcept
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
	lightSystem = std::move(other.lightSystem);
	return *this;
}

void ForwardRenderer::recreateSwapchain()
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
		swapchain.recreate(&context);

		depthBufferTexture = createDepthBuffer();

		renderPass = vkw::RenderPass(createRenderPass(), context.get_device());

		rendererUniformBuffers = createUniformBuffers(rendererUBO);
		settingsUniformBuffers = createUniformBuffers(settingsUBO);
		descriptorPool = vkw::DescriptorPool(createDescriptorPool(), context.get_device());
		uboDescriptorSets = createCameraDescriptorSets();

		{
			auto [gPipelineLayout, gPipeline, sbPipeline] = createGraphicsPipeline();
			graphicsPipelineLayout = vkw::PipelineLayout(gPipelineLayout, context.get_device());
			graphicsPipeline = vkw::Pipeline(gPipeline, context.get_device());
			skyboxPipeline = vkw::Pipeline(sbPipeline, context.get_device());
		}

		renderFramebuffers = ManagedVector(createRenderFramebuffers(), [dev = context.get_device()](VkFramebuffer& fb) {
			vkDestroyFramebuffer(dev, fb, nullptr);
		});
		commandBuffers = ManagedVector<VkCommandBuffer, false>(
			allocateCommandBuffers(),
			[dev = context.get_device(), pool = context.get_graphicsCommandPool()](std::vector<VkCommandBuffer>& buf) {
				vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(buf.size()), buf.data());
			});

		gui.recreate(&context, &swapchain);

		recordCommandBuffers();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}
} // namespace blaze
