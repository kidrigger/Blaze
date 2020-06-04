
#pragma once

#include "Renderer.hpp"
#include <core/Bindable.hpp>
#include <core/Camera.hpp>
#include <core/Context.hpp>
#include <core/Swapchain.hpp>
#include <gui/GUI.hpp>
#include <spirv/PipelineFactory.hpp>
#include <util/Managed.hpp>
#include <util/PackedHandler.hpp>
#include <vkwrap/VkWrap.hpp>

namespace blaze
{
class ALightCaster;
/**
 * @brief The abstract base renderer class.
 *
 * ARenderer is responsible for the basic renderer functions such as setting up the
 * context, swapchain, gui, etc.
 * It handles the presentation and the setup of synchronization objects, drawables etc.
 *
 * Beyond that the individual functionality must be programmed in by extending in a derived class.
 */
class ARenderer
{
protected:
	uint32_t maxFrameInFlight{2};
	bool isComplete{false};
	bool windowResized{false};

	uint32_t currentFrame{0};

	std::unique_ptr<Context> context;
	std::unique_ptr<Swapchain> swapchain;
	std::unique_ptr<GUI> gui;
	Camera* camera{nullptr};

	vkw::CommandBufferVector commandBuffers;

	vkw::SemaphoreVector imageAvailableSem;
	vkw::SemaphoreVector renderFinishedSem;
	vkw::FenceVector inFlightFences;

	using DrawList = util::PackedHandler<Drawable*>;
	DrawList drawables;

public:
	ARenderer() noexcept
	{
	}

	ARenderer(GLFWwindow* window, bool enableValidationLayers = true) noexcept;

	ARenderer(ARenderer&& other) = delete;
	ARenderer& operator=(ARenderer&& other) = delete;
	ARenderer(const ARenderer& other) = delete;
	ARenderer& operator=(const ARenderer& other) = delete;

    /**
     * @brief Renders the next frame.
     */
	void render();

    /**
     * @brief Allows setting up of the environment descriptor as a bindable.
     */
	virtual void setEnvironment(const Bindable* env) = 0;

    /**
     * @brief Returns the currently used primary shader.
     */
	virtual const spirv::Shader& get_shader() const = 0;

	/**
	 * @brief Manages all settings for the renderer
	 */
	virtual void drawSettings() = 0;

    /**
     * @brief Sets the pointer to the camera in use.
     */
	void set_camera(Camera* p_camera)
	{
		camera = p_camera;
	}

	// Light Controls
	virtual ALightCaster* get_lightCaster() = 0;

	const Context* get_context() const
	{
		return context.get();
	}

	spirv::PipelineFactory* get_pipelineFactory()
	{
		return context->get_pipelineFactory();
	}

    /**
     * @brief Submits a drawable into the handler.
     */
	[[nodiscard]] DrawList::Handle submit(Drawable* sub)
	{
		return drawables.add(sub);
	}

    /**
     * @brief Checks if the renderer is complete.
     */
	bool complete()
	{
		return isComplete;
	}

    /**
     * @brief Wait for device idle.
     */
	void waitIdle()
	{
		vkDeviceWaitIdle(context->get_device());
	}

	virtual ~ARenderer()
	{
	}

protected:
	void recreateSwapchain();

	std::pair<uint32_t, uint32_t> get_dimensions() const
	{
		int x, y;
		glfwGetWindowSize(context->get_window(), &x, &y);
		return {x, y};
	}

	void clearCommandBuffers();
	void rebuildAllCommandBuffers();
	void rebuildCommandBuffer(uint32_t frame);

	virtual void update(uint32_t frame) = 0;
	// This is ONLY the renderpasses from the renderer
	virtual void recordCommands(uint32_t frame) = 0;
	virtual void recreateSwapchainDependents() = 0;

private:
	vkw::SemaphoreVector createSemaphores(uint32_t imageCount) const;
	vkw::FenceVector createFences(uint32_t imageCount) const;
	vkw::CommandBufferVector allocateCommandBuffers(uint32_t imageCount) const;
	void setupPerFrameData(uint32_t numFrames);
};
} // namespace blaze
