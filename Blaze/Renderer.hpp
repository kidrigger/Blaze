
#pragma once

#include <Datatypes.hpp>
#include <Drawable.hpp>
#include <GUI.hpp>
#include <LightSystem.hpp>
#include <Swapchain.hpp>
#include <Texture2D.hpp>
#include <TextureCube.hpp>
#include <UniformBuffer.hpp>
#include <core/Context.hpp>
#include <util/Managed.hpp>
#include <util/createFunctions.hpp>

#include <map>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
const float PI = 3.1415926535897932384626433f;
using RenderCommand = std::function<void(VkCommandBuffer buf, VkPipelineLayout layout, uint32_t frameCount)>;

class Renderer
{
public:

	virtual void submit(Drawable* drawable) = 0;
	virtual void renderFrame() = 0;

	virtual void set_environmentDescriptor(VkDescriptorSet envDS) = 0;
	virtual void set_skyboxCommand(const RenderCommand& cmd) = 0;
	virtual void set_cameraUBO(const CameraUniformBufferObject& ubo) = 0;
	virtual void set_camera(Camera* cam) = 0;
	virtual void set_lightUBO(const LightsUniformBufferObject& ubo) = 0;
	virtual void set_settingsUBO(const SettingsUniformBufferObject& ubo) = 0;

	virtual const VkDescriptorSetLayout& get_materialLayout() const = 0;
	virtual const VkDescriptorSetLayout& get_environmentLayout() const = 0;
	virtual LightSystem& get_lightSystem() = 0;

	// Context forwarding
	virtual VkDevice get_device() const = 0;
	virtual const Context& get_context() const = 0;

	virtual bool complete() const = 0;
};

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

	util::Managed<VkRenderPass> renderPass;

	util::Managed<VkDescriptorSetLayout> uboDescriptorSetLayout;
	util::Managed<VkDescriptorSetLayout> environmentDescriptorSetLayout;
	util::Managed<VkDescriptorSetLayout> materialDescriptorSetLayout;

	util::Managed<VkDescriptorPool> descriptorPool;
	util::UnmanagedVector<VkDescriptorSet> uboDescriptorSets;

	std::vector<UniformBuffer<RendererUniformBufferObject>> rendererUniformBuffers;
	RendererUniformBufferObject rendererUBO{};
	std::vector<UniformBuffer<SettingsUniformBufferObject>> settingsUniformBuffers;
	SettingsUniformBufferObject settingsUBO{};

	util::Managed<VkPipelineLayout> graphicsPipelineLayout;
	util::Managed<VkPipeline> graphicsPipeline;
	util::Managed<VkPipeline> skyboxPipeline;

	util::ManagedVector<VkFramebuffer> renderFramebuffers;
	util::ManagedVector<VkCommandBuffer, false> commandBuffers;

	util::ManagedVector<VkSemaphore> imageAvailableSem;
	util::ManagedVector<VkSemaphore> renderFinishedSem;
	util::ManagedVector<VkFence> inFlightFences;

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

	void set_cameraUBO(const CameraUniformBufferObject& ubo)
	{
		memcpy(&rendererUBO, &ubo, sizeof(ubo));
	}

	void set_camera(Camera* cam)
	{
		camera = cam;
	}

	void set_lightUBO(const LightsUniformBufferObject& ubo)
	{
		memcpy(&rendererUBO.dirLightTransform, &ubo, sizeof(ubo));
	}

	void set_settingsUBO(const SettingsUniformBufferObject& ubo)
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
	std::vector<UniformBuffer<RendererUniformBufferObject>> createUniformBuffers(
		const RendererUniformBufferObject& ubo) const;
	std::vector<UniformBuffer<SettingsUniformBufferObject>> createUniformBuffers(
		const SettingsUniformBufferObject& ubo) const;
	std::tuple<VkPipelineLayout, VkPipeline, VkPipeline> createGraphicsPipeline() const;
	std::vector<VkFramebuffer> createRenderFramebuffers() const;
	std::vector<VkCommandBuffer> allocateCommandBuffers() const;
	std::tuple<std::vector<VkSemaphore>, std::vector<VkSemaphore>, std::vector<VkFence>> createSyncObjects() const;
	void recordCommandBuffers();
	void rebuildCommandBuffer(int frame);

	Texture2D createDepthBuffer() const;

	void updateUniformBuffer(int frame, const RendererUniformBufferObject& ubo)
	{
		rendererUniformBuffers[frame].write(context, ubo);
	}

	void updateUniformBuffer(int frame, const SettingsUniformBufferObject& ubo)
	{
		settingsUniformBuffers[frame].write(context, ubo);
	}
};
} // namespace blaze
