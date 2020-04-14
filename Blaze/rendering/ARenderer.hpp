
#pragma once

#include "Renderer.hpp"
#include <core/Camera.hpp>
#include <core/Context.hpp>
#include <core/Swapchain.hpp>
#include <gui/GUI.hpp>
#include <util/Managed.hpp>
#include <util/PackedHandler.hpp>
#include <vkwrap/VkWrap.hpp>

namespace blaze
{
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

	void render();

	[[nodiscard]] DrawList::Handle submit(Drawable* sub)
	{
		return drawables.add(sub);
	}

	bool complete()
	{
		return isComplete;
	}

	virtual ~ARenderer()
	{
	}

protected:
	virtual void update() = 0;

	void recreateSwapchain();
	virtual void recreateSwapchainDependents() = 0;

	std::pair<uint32_t, uint32_t> get_dimensions() const
	{
		int x, y;
		glfwGetWindowSize(context->get_window(), &x, &y);
		return {x, y};
	}

	void rebuildAllCommandBuffers();
	void rebuildCommandBuffer(uint32_t frame);
	// This is ONLY the renderpasses from the renderer
	virtual void recordCommands(uint32_t frame) = 0;

private:
	vkw::SemaphoreVector createSemaphores(uint32_t imageCount) const;
	vkw::FenceVector createFences(uint32_t imageCount) const;
	vkw::CommandBufferVector allocateCommandBuffers(uint32_t imageCount) const;
	void setupPerFrameData(uint32_t numFrames);
};
} // namespace blaze
