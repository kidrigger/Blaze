
#pragma once

#include <core/Context.hpp>
#include <core/Swapchain.hpp>
#include <gui/GUI.hpp>
#include <core/Camera.hpp>
#include "Renderer.hpp"
#include <util/Managed.hpp>
#include <vkwrap/VkWrap.hpp>

namespace blaze
{
class ARenderer
{
protected:
	uint32_t max_frames_in_flight{2};
	bool isComplete{false};
	bool windowResized{false};

	uint32_t currentFrame{0};

	std::unique_ptr<Context> context;
	std::unique_ptr<Swapchain> swapchain;
	std::unique_ptr<GUI> gui;
	Camera* camera{nullptr};

	util::ManagedVector<VkCommandBuffer, false> commandBuffers;

	vkw::SemaphoreVector imageAvailableSem;
	vkw::SemaphoreVector renderFinishedSem;
	vkw::FenceVector inFlightFences;

	std::vector<Drawable*> drawables;

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

	bool complete()
	{
		return isComplete;
	}

	virtual ~ARenderer()
	{
	}

protected:
	virtual void renderFrame() = 0;

	void recreateSwapchain();
	virtual void recreateSwapchainDependents() = 0;

	std::pair<uint32_t, uint32_t> get_dimensions() const
	{
		int x, y;
		glfwGetWindowSize(context->get_window(), &x, &y);
		return {x, y};
	}

	void rebuildAllCommandBuffers();

	// This is ONLY the renderpasses from the renderer
	virtual void rebuildCommandBuffer(VkCommandBuffer cmd) = 0;

private:
	vkw::SemaphoreVector createSemaphores(uint32_t imageCount) const;
	vkw::FenceVector createFences(uint32_t imageCount) const;
	std::vector<VkCommandBuffer> allocateCommandBuffers(uint32_t imageCount) const;
};
} // namespace blaze
