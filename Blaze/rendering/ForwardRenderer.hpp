
#pragma once

#include <rendering/Renderer.hpp>
#include <Datatypes.hpp>
#include <Drawable.hpp>
#include <gui/GUI.hpp>
#include <LightSystem.hpp>
#include <core/Swapchain.hpp>
#include <Texture2D.hpp>
#include <TextureCube.hpp>
#include <UniformBuffer.hpp>
#include <core/Context.hpp>
#include <util/Managed.hpp>
#include <util/createFunctions.hpp>
#include <vkwrap/VkWrap.hpp>

#include <map>
#include <string>
#include <vector>

namespace blaze
{

/**
 * @class ForwardRenderer
 *
 * @brief Contains entire rendering logic.
 *
 * This class contains the main drawing logic and all the related components.
 * This is a forward renderer that supports PBR rendering.
 */
class ForwardRenderer : public Renderer
{
private:
	uint32_t max_frames_in_flight{2};
	bool isComplete{false};
	bool windowResized{false};

	Context context;
	Swapchain swapchain;
	GUI gui;
	LightSystem lightSystem;
	Camera* camera;

	vkw::RenderPass renderPass;

	vkw::DescriptorSetLayout uboDescriptorSetLayout;
	vkw::DescriptorSetLayout environmentDescriptorSetLayout;
	vkw::DescriptorSetLayout materialDescriptorSetLayout;

	vkw::DescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> uboDescriptorSets;

	std::vector<UBO<RendererUBlock>> rendererUniformBuffers;
	RendererUBlock rendererUBO{};
	std::vector<UBO<SettingsUBlock>> settingsUniformBuffers;
	SettingsUBlock settingsUBO{};

	vkw::PipelineLayout graphicsPipelineLayout;
	vkw::Pipeline graphicsPipeline;
	vkw::Pipeline skyboxPipeline;

	util::ManagedVector<VkFramebuffer> renderFramebuffers;
	util::ManagedVector<VkCommandBuffer, false> commandBuffers;

	vkw::SemaphoreVector imageAvailableSem;
	vkw::SemaphoreVector renderFinishedSem;
	vkw::FenceVector inFlightFences;

	std::vector<Drawable*> drawables;
	VkDescriptorSet environmentDescriptor{VK_NULL_HANDLE};
	RenderCommand skyboxCommand;

	Texture2D depthBufferTexture;

	uint32_t currentFrame{0};

public:
	/**
	 * @fn ForwardRenderer()
	 *
	 * @brief Default empty constructor.
	 */
	ForwardRenderer() noexcept
	{
	}

	/**
	 * @fn ForwardRenderer(GLFWwindow* window, bool enableValidationLayers = true)
	 *
	 * @brief Actual constructor
	 *
	 * @param window The GLFW window pointer.
	 * @param enableValidationLayers Enable for Debugging.
	 */
	ForwardRenderer(GLFWwindow* window, bool enableValidationLayers = true) noexcept;

	/**
	 * @name Move Constructors.
	 *
	 * @brief Move only, Copy is deleted.
	 * @{
	 */
	ForwardRenderer(ForwardRenderer&& other) noexcept;
	ForwardRenderer& operator=(ForwardRenderer&& other) noexcept;
	ForwardRenderer(const ForwardRenderer& other) = delete;
	ForwardRenderer& operator=(const ForwardRenderer& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @fn renderFrame()
	 *
	 * @brief Actually rendering a frame and submitting to the present queue.
	 */
	void renderFrame();

	/**
	 * @name Getters
	 *
	 * @brief Getters for private members.
	 *
	 * @{
	 */
	std::pair<uint32_t, uint32_t> get_dimensions() const
	{
		int x, y;
		glfwGetWindowSize(context.get_window(), &x, &y);
		return {x, y};
	}
	const VkFormat& get_swapchainFormat() const
	{
		return swapchain.get_format();
	}
	const VkExtent2D& get_swapchainExtent() const
	{
		return swapchain.get_extent();
	}
	size_t get_swapchainImageCount() const
	{
		return swapchain.get_imageCount();
	}
	const VkImageView& get_swapchainImageView(uint32_t index) const
	{
		return swapchain.get_imageView(index);
	}
	const std::vector<VkImageView>& get_swapchainImageViews() const
	{
		return swapchain.get_imageViews();
	}
	const VkRenderPass& get_renderPass() const
	{
		return renderPass.get();
	}
	const VkPipeline& get_graphicsPipeline() const
	{
		return graphicsPipeline.get();
	}
	size_t get_framebufferCount() const
	{
		return renderFramebuffers.size();
	}
	const VkFramebuffer& get_framebuffer(size_t index) const
	{
		return renderFramebuffers[index];
	}
	size_t get_commandBufferCount() const
	{
		return commandBuffers.size();
	}
	const VkCommandBuffer& get_commandBuffer(size_t index) const
	{
		return commandBuffers[index];
	}
	const VkDescriptorSetLayout& get_uboLayout() const
	{
		return uboDescriptorSetLayout.get();
	}
	const VkDescriptorSetLayout& get_materialLayout() const
	{
		return materialDescriptorSetLayout.get();
	}
	const VkDescriptorSetLayout& get_environmentLayout() const
	{
		return environmentDescriptorSetLayout.get();
	}
	LightSystem& get_lightSystem()
	{
		return lightSystem;
	}

	// Context forwarding
	VkDevice get_device() const
	{
		return context.get_device();
	}
	VkPhysicalDevice get_physicalDevice() const
	{
		return context.get_physicalDevice();
	}
	VkQueue get_transferQueue() const
	{
		return context.get_transferQueue();
	}
	VkCommandPool get_transferCommandPool() const
	{
		return context.get_transferCommandPool();
	}
	const Context& get_context() const
	{
		return context;
	}
	/**
	 * @}
	 */

	/**
	 * @fn submit
	 *
	 * @brief Submits a Drawable to draw to screen.
	 */
	void submit(Drawable* cmd)
	{
		drawables.push_back(cmd);
	}

	/**
	 * @name Setters
	 *
	 * @brief Setters for various properties.
	 *
	 * @{
	 */
	void set_environmentDescriptor(VkDescriptorSet envDS)
	{
		environmentDescriptor = envDS;
	}

	void set_skyboxCommand(const RenderCommand& cmd)
	{
		skyboxCommand = cmd;
	}

	void set_cameraUBO(const CameraUBlock& ubo)
	{
		memcpy(&rendererUBO, &ubo, sizeof(ubo));
	}

	void set_camera(Camera* cam)
	{
		camera = cam;
	}

	void set_lightUBO(const LightsUBlock& ubo)
	{
		memcpy(&rendererUBO.dirLightTransform, &ubo, sizeof(ubo));
	}

	void set_settingsUBO(const SettingsUBlock& ubo)
	{
		memcpy(&settingsUBO, &ubo, sizeof(ubo));
	}
	/**
	 * @}
	 */

	/**
	 * @fn complete()
	 *
	 * @brief Checks if the renderer is complete.
	 *
	 * A ForwardRenderer is considered complete if and only if every member is initialized.
	 *
	 * @returns \a true If the renderer sucessfully initialized
	 * @returns \a false If the renderer was empty or had initialization errors.
	 */
	bool complete() const
	{
		return isComplete;
	}

private:
	void recreateSwapchain();

	VkRenderPass createRenderPass() const;
	VkDescriptorSetLayout createUBODescriptorSetLayout() const;
	VkDescriptorSetLayout createEnvironmentDescriptorSetLayout() const;
	VkDescriptorSetLayout createMaterialDescriptorSetLayout() const;
	VkDescriptorPool createDescriptorPool() const;
	std::vector<VkDescriptorSet> createCameraDescriptorSets() const;
	std::vector<UBO<RendererUBlock>> createUniformBuffers(
		const RendererUBlock& ubo) const;
	std::vector<UBO<SettingsUBlock>> createUniformBuffers(
		const SettingsUBlock& ubo) const;
	std::tuple<VkPipelineLayout, VkPipeline, VkPipeline> createGraphicsPipeline() const;
	std::vector<VkFramebuffer> createRenderFramebuffers() const;
	std::vector<VkCommandBuffer> allocateCommandBuffers() const;
	std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> createSyncObjects() const;
	void recordCommandBuffers();
	void rebuildCommandBuffer(int frame);

	Texture2D createDepthBuffer() const;

	void updateUniformBuffer(int frame, const RendererUBlock& ubo)
	{
		rendererUniformBuffers[frame].write(ubo);
	}

	void updateUniformBuffer(int frame, const SettingsUBlock& ubo)
	{
		settingsUniformBuffers[frame].write(ubo);
	}
};
} // namespace blaze