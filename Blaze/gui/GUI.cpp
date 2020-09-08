
#include "GUI.hpp"

#include <rendering/ARenderer.hpp>
#include <core/Swapchain.hpp>

#include <thirdparty/optick/optick.h>

namespace blaze
{

void GUI::startFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;

	// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
	// because it would be confusing to have two docking targets within each others.
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetWorkPos());
	ImGui::SetNextWindowSize(viewport->GetWorkSize());
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	window_flags |=
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground;

	// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
	// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
	// all active windows docked into it will lose their parent and become undocked.
	// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
	// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("DockSpace Demo", nullptr, window_flags);
	ImGui::PopStyleVar();

	ImGui::PopStyleVar(2);

	// DockSpace
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}
}

void GUI::endFrame()
{
	ImGui::End();
	ImGui::Render();
	complete = true;

	ImGui::EndFrame();
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
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
	OPTICK_EVENT();
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
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Viewports bad

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
