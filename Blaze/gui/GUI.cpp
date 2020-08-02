
#include "GUI.hpp"

#include <rendering/ARenderer.hpp>
#include <core/Swapchain.hpp>

namespace blaze
{

void GUI::startFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void GUI::endFrame()
{
	ImGui::Render();
	complete = true;
}

void GUI::recreate(const Context* context, const Swapchain* swapchain)
{
	width = swapchain->get_extent().width;
	height = swapchain->get_extent().height;

	framebuffers.clear();
	for (auto& iv : swapchain->get_imageViews())
	{
		framebuffers.push_back(
			context->get_pipelineFactory()->createFramebuffer(renderPass, swapchain->get_extent(), {iv}));
	}
}

void GUI::draw(VkCommandBuffer cmdBuffer, int frameCount)
{
	if (complete)
	{
		renderPass.begin(cmdBuffer, framebuffers[frameCount]);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
		
		renderPass.end(cmdBuffer);
	}
}

GUI::GUI(const Context* context, const Swapchain* swapchain) noexcept
	: width(swapchain->get_extent().width), height(swapchain->get_extent().height), valid(false)
{
	std::vector<VkDescriptorPoolSize> pool_sizes = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
													{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
													{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
													{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
													{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
													{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
													{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
													{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
													{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
													{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
													{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
	descriptorPool =
		vkw::DescriptorPool(util::createDescriptorPool(context->get_device(), pool_sizes, 1000), context->get_device());

	spirv::AttachmentFormat format = {};
	format.format = swapchain->get_format();
	format.sampleCount = VK_SAMPLE_COUNT_1_BIT;
	format.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	format.loadStoreConfig = spirv::LoadStoreConfig(spirv::LoadStoreConfig::LoadAction::CONTINUE,
													spirv::LoadStoreConfig::StoreAction::PRESENT);

	VkAttachmentReference colorRef = {};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	std::vector<VkSubpassDescription> subpass(1);
	subpass[0].pDepthStencilAttachment = nullptr;
	subpass[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass[0].inputAttachmentCount = 0;
	subpass[0].pInputAttachments = nullptr;
	subpass[0].colorAttachmentCount = 1;
	subpass[0].pColorAttachments = &colorRef;
	subpass[0].preserveAttachmentCount = 0;
	subpass[0].pPreserveAttachments = nullptr;
	subpass[0].pResolveAttachments = nullptr;
	subpass[0].flags = 0;

	renderPass = context->get_pipelineFactory()->createRenderPass({format}, subpass);
	VkClearValue clearValue = {};
	clearValue.color = {0.0f, 0.0f, 0.0f, 0.0f};
	renderPass.clearValues = {clearValue};

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForVulkan(context->get_window(), true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = context->get_instance();
	init_info.PhysicalDevice = context->get_physicalDevice();
	init_info.Device = context->get_device();
	init_info.QueueFamily = context->get_queueFamilyIndices().graphicsIndex.value();
	init_info.Queue = context->get_graphicsQueue();
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = descriptorPool.get();
	init_info.Allocator = nullptr;
	init_info.MinImageCount = std::max(2u, swapchain->get_imageCount());
	init_info.ImageCount = swapchain->get_imageCount();
	init_info.CheckVkResultFn = VK_ASSERT;
	ImGui_ImplVulkan_Init(&init_info, renderPass.get());

	auto cmdBuffer = context->startCommandBufferRecord();
	ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
	context->flushCommandBuffer(cmdBuffer);

	for (auto& iv : swapchain->get_imageViews())
	{
		framebuffers.push_back(
			context->get_pipelineFactory()->createFramebuffer(renderPass, swapchain->get_extent(), {iv}));
	}

	valid = true;
}

GUI::GUI(GUI&& other) noexcept
	: width(other.width), height(other.height), descriptorPool(std::move(other.descriptorPool)),
	  renderPass(std::move(other.renderPass)), framebuffers(std::move(other.framebuffers)), valid(other.valid)
{
	other.valid = false;
}

GUI& GUI::operator=(GUI&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	width = other.width;
	height = other.height;
	descriptorPool = std::move(other.descriptorPool);
	renderPass = std::move(other.renderPass);
	framebuffers = std::move(other.framebuffers);
	valid = other.valid;
	other.valid = false;
	return *this;
}

GUI::~GUI()
{
	if (valid)
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
}
} // namespace blaze
