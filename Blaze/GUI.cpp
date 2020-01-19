
#include "GUI.hpp"

#include "Renderer.hpp"

bool blaze::GUI::complete = false;

namespace blaze {
	
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

    void GUI::recreate(const Context& context, const VkExtent2D& size, const std::vector<VkImageView>& swapchainImageViews)
    {
        width = size.width;
        height = size.height;
        framebuffers = util::ManagedVector(createSwapchainFramebuffers(context.get_device(), swapchainImageViews), [dev = context.get_device()](VkFramebuffer& fb) { vkDestroyFramebuffer(dev, fb, nullptr); });
    }

    void GUI::draw(VkCommandBuffer cmdBuffer, int frameCount)
    {
        if (complete)
        {
            VkClearValue clearValue = {};
            clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

            VkRenderPassBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            info.renderPass = renderPass.get();
            info.framebuffer = framebuffers[frameCount];
            info.renderArea.extent.width = width;
            info.renderArea.extent.height = height;
            info.clearValueCount = 1;
            info.pClearValues = &clearValue;
            vkCmdBeginRenderPass(cmdBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
            vkCmdEndRenderPass(cmdBuffer);
        }
    }

    GUI::GUI(const Context& context, const VkExtent2D& size, const VkFormat& format, const std::vector<VkImageView>& swapchainImageViews) noexcept
        : width(size.width),
        height(size.height),
        valid(false)
    {
        std::vector<VkDescriptorPoolSize> pool_sizes =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        descriptorPool = util::Managed(util::createDescriptorPool(context.get_device(), pool_sizes, 1000), [dev = context.get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });

        {
            auto rpass = util::createRenderPass(context.get_device(), format, VK_FORMAT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ATTACHMENT_LOAD_OP_LOAD);
            renderPass = util::Managed(rpass, [dev = context.get_device()](VkRenderPass& pass) { vkDestroyRenderPass(dev, pass, nullptr); });
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(context.get_window(), true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = context.get_instance();
        init_info.PhysicalDevice = context.get_physicalDevice();
        init_info.Device = context.get_device();
        init_info.QueueFamily = context.get_queueFamilyIndices().graphicsIndex.value();
        init_info.Queue = context.get_graphicsQueue();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = descriptorPool.get();
        init_info.Allocator = nullptr;
        init_info.MinImageCount = std::max(2u, static_cast<uint32_t>(swapchainImageViews.size()));
        init_info.ImageCount = static_cast<uint32_t>(swapchainImageViews.size());
        init_info.CheckVkResultFn = VK_ASSERT;
        ImGui_ImplVulkan_Init(&init_info, renderPass.get());

        auto cmdBuffer = context.startCommandBufferRecord();
        ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer);
        context.flushCommandBuffer(cmdBuffer);

        framebuffers = util::ManagedVector(createSwapchainFramebuffers(context.get_device(), swapchainImageViews), [dev = context.get_device()](VkFramebuffer& fb) { vkDestroyFramebuffer(dev, fb, nullptr); });
        
        valid = true;
    }

    GUI::GUI(GUI&& other) noexcept
        : width(other.width),
        height(other.height),
        descriptorPool(std::move(other.descriptorPool)),
        renderPass(std::move(other.renderPass)),
        framebuffers(std::move(other.framebuffers)),
        valid(other.valid)
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

    std::vector<VkFramebuffer> GUI::createSwapchainFramebuffers(VkDevice device, const std::vector<VkImageView>& swapchainImageViews) const
    {
        using namespace std;

        vector<VkFramebuffer> frameBuffers(swapchainImageViews.size());
        for (size_t i = 0; i < swapchainImageViews.size(); i++)
        {
            vector<VkImageView> attachments = {
                swapchainImageViews[i]
            };

            VkFramebufferCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            createInfo.renderPass = renderPass.get();
            createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            createInfo.pAttachments = attachments.data();
            createInfo.width = width;
            createInfo.height = height;
            createInfo.layers = 1;

            auto result = vkCreateFramebuffer(device, &createInfo, nullptr, &frameBuffers[i]);
            if (result != VK_SUCCESS)
            {
                for (size_t j = 0; j < i; j++)
                {
                    vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
                }
                throw runtime_error("Framebuffer creation failed with " + std::to_string(result));
            }
        }
        return frameBuffers;
    }
}
