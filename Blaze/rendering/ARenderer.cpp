
#include "ARenderer.hpp"

namespace blaze
{

ARenderer::ARenderer(GLFWwindow* window, bool enableValidationLayers) noexcept
{
	using namespace std;
	using namespace util;

	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, [](GLFWwindow* window, int width, int height) {
		ARenderer* renderer = reinterpret_cast<ARenderer*>(glfwGetWindowUserPointer(window));
		renderer->windowResized = true;
	});

	context = make_unique<Context>(window, enableValidationLayers);
	swapchain = make_unique<Swapchain>(*context);

	commandBuffers = ManagedVector<VkCommandBuffer, false>(
		allocateCommandBuffers(swapchain->get_imageCount()),
		[dev = context->get_device(), pool = context->get_graphicsCommandPool()](std::vector<VkCommandBuffer>& buf) {
			vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(buf.size()), buf.data());
		});

	max_frames_in_flight = static_cast<uint32_t>(commandBuffers.size());

	imageAvailableSem =
		ManagedVector(createSemaphores(swapchain->get_imageCount()),
					  [dev = context->get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
	renderFinishedSem =
		ManagedVector(createSemaphores(swapchain->get_imageCount()),
					  [dev = context->get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
	inFlightFences = ManagedVector(createFences(swapchain->get_imageCount()),
								   [dev = context->get_device()](VkFence& sem) { vkDestroyFence(dev, sem, nullptr); });

	gui = make_unique<GUI>(*context, swapchain->get_extent(), swapchain->get_format(), swapchain->get_imageViews());
}

void ARenderer::render()
{
	using namespace std;

	uint32_t imageIndex;
	auto result =
		vkAcquireNextImageKHR(context->get_device(), swapchain->get_swapchain(), numeric_limits<uint64_t>::max(),
							  imageAvailableSem[currentFrame], VK_NULL_HANDLE, &imageIndex);
	vkWaitForFences(context->get_device(), 1, &inFlightFences[imageIndex], VK_TRUE, numeric_limits<uint64_t>::max());

	renderFrame();

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

	vkResetFences(context->get_device(), 1, &inFlightFences[imageIndex]);

	result = vkQueueSubmit(context->get_graphicsQueue(), 1, &submitInfo, inFlightFences[imageIndex]);
	if (result != VK_SUCCESS)
	{
		throw runtime_error("Queue Submit failed with " + to_string(result));
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = {swapchain->get_swapchain()};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(context->get_presentQueue(), &presentInfo);

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

std::vector<VkSemaphore> ARenderer::createSemaphores(uint32_t imageCount) const
{
	std::vector<VkSemaphore> sems(imageCount);
	for (auto& sem : sems)
	{
		sem = util::createSemaphore(context->get_device());
	}

	return sems;
}

std::vector<VkFence> ARenderer::createFences(uint32_t imageCount) const
{
	std::vector<VkFence> fences(imageCount);
	for (auto& fence : fences)
	{
		fence = util::createFence(context->get_device());
	}

	return fences;
}

std::vector<VkCommandBuffer> ARenderer::allocateCommandBuffers(uint32_t imageCount) const
{
	std::vector<VkCommandBuffer> commandBuffers(imageCount);

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = context->get_graphicsCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
	auto result = vkAllocateCommandBuffers(context->get_device(), &allocInfo, commandBuffers.data());
	if (result == VK_SUCCESS)
	{
		return commandBuffers;
	}
	throw std::runtime_error("Command buffer alloc failed with " + std::to_string(result));
}

void ARenderer::recreateSwapchain()
{
	try
	{
		vkDeviceWaitIdle(context->get_device());
		auto [width, height] = get_dimensions();
		while (width == 0 || height == 0)
		{
			std::tie(width, height) = get_dimensions();
			glfwWaitEvents();
		}

		using namespace util;
		swapchain->recreate(*context);

		commandBuffers = ManagedVector<VkCommandBuffer, false>(
			allocateCommandBuffers(swapchain->get_imageCount()),
			[dev = context->get_device(), pool = context->get_graphicsCommandPool()](std::vector<VkCommandBuffer>& buf) {
				vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(buf.size()), buf.data());
			});

		max_frames_in_flight = static_cast<uint32_t>(commandBuffers.size());

		imageAvailableSem = ManagedVector(
			createSemaphores(swapchain->get_imageCount()),
			[dev = context->get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
		renderFinishedSem = ManagedVector(
			createSemaphores(swapchain->get_imageCount()),
			[dev = context->get_device()](VkSemaphore& sem) { vkDestroySemaphore(dev, sem, nullptr); });
		inFlightFences = ManagedVector(
			createFences(swapchain->get_imageCount()),
			[dev = context->get_device()](VkFence& sem) { vkDestroyFence(dev, sem, nullptr); });

		// Recreate all number based
		recreateSwapchainDependents();

		gui->recreate(*context, swapchain->get_extent(), swapchain->get_imageViews());

		// Record commands
		rebuildAllCommandBuffers();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}
}


void ARenderer::rebuildAllCommandBuffers()
{
	for (int frame = 0; frame < commandBuffers.size(); frame++)
	{
		vkWaitForFences(context->get_device(), 1, &inFlightFences[frame], VK_TRUE, std::numeric_limits<uint64_t>::max());

		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		auto result = vkBeginCommandBuffer(commandBuffers[frame], &commandBufferBeginInfo);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Begin Command Buffer failed with " + std::to_string(result));
		}

		rebuildCommandBuffer(commandBuffers[frame]);

		gui->draw(commandBuffers[frame], frame);

		result = vkEndCommandBuffer(commandBuffers[frame]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("End Command Buffer failed with " + std::to_string(result));
		}
	}
}

} // namespace blaze
