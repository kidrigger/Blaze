
#include "FwdRenderer.hpp"

namespace blaze
{
FwdRenderer::FwdRenderer(GLFWwindow* window, bool enableValidationLayers) noexcept
	: ARenderer(window, enableValidationLayers)
{
	renderPass = createRenderpass();

	// Depthbuffer
	depthBuffer = createDepthBuffer();

	// All uniform buffer stuff

	// DescriptorPool

	// Pipeline layouts
	// Pipeline

	// Framebuffers
	renderFramebuffers = createFramebuffers();

	rebuildAllCommandBuffers();
	isComplete = true;
}

void FwdRenderer::update()
{
}

void FwdRenderer::recordCommands(uint32_t frame)
{
	VkRenderPassBeginInfo renderpassBeginInfo = {};
	renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpassBeginInfo.renderPass = renderPass.get();
	renderpassBeginInfo.framebuffer = renderFramebuffers[frame];
	renderpassBeginInfo.renderArea.offset = {0, 0};
	renderpassBeginInfo.renderArea.extent = swapchain->get_extent();

	std::array<VkClearValue, 2> clearColor;
	clearColor[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
	clearColor[1].depthStencil = {1.0f, 0};
	renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
	renderpassBeginInfo.pClearValues = clearColor.data();

	vkCmdBeginRenderPass(commandBuffers[frame], &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Do the systemic thing

	vkCmdEndRenderPass(commandBuffers[frame]);
}

void FwdRenderer::recreateSwapchainDependents()
{
	renderPass = createRenderpass();

	// Depthbuffer
	depthBuffer = createDepthBuffer();

	// All uniform buffer stuff

	// DescriptorPool

	// Pipeline layouts
	// Pipeline

	// Framebuffers
	renderFramebuffers = createFramebuffers();
}

// Custom creation functions
vkw::RenderPass FwdRenderer::createRenderpass() const
{
	return vkw::RenderPass(util::createRenderPass(context->get_device(), swapchain->get_format(), VK_FORMAT_D32_SFLOAT),
						   context->get_device());
}

vkw::FramebufferVector FwdRenderer::createFramebuffers() const
{
	using namespace std;

	vector<VkFramebuffer> frameBuffers(maxFrameInFlight);
	for (uint32_t i = 0; i < maxFrameInFlight; i++)
	{
		vector<VkImageView> attachments = {swapchain->get_imageView(i), depthBuffer.get_imageView()};

		VkFramebufferCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass.get();
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.width = swapchain->get_extent().width;
		createInfo.height = swapchain->get_extent().height;
		createInfo.layers = 1;

		auto result = vkCreateFramebuffer(context->get_device(), &createInfo, nullptr, &frameBuffers[i]);
		if (result != VK_SUCCESS)
		{
			for (size_t j = 0; j < i; j++)
			{
				vkDestroyFramebuffer(context->get_device(), frameBuffers[i], nullptr);
			}
			throw runtime_error("Framebuffer creation failed with " + std::to_string(result));
		}
	}
	return vkw::FramebufferVector(std::move(frameBuffers), context->get_device());
}

Texture2D FwdRenderer::createDepthBuffer() const
{
	VkFormat format = util::findSupportedFormat(
		context->get_physicalDevice(), {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	ImageData2D imageData = {};
	imageData.format = format;
	imageData.access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	imageData.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageData.height = swapchain->get_extent().height;
	imageData.width = swapchain->get_extent().width;
	imageData.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	imageData.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageData.numChannels = 1;
	imageData.size = swapchain->get_extent().width * swapchain->get_extent().height;

	return Texture2D(*context, imageData);
}

} // namespace blaze