
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <rendering/ARenderer.hpp>
#include <vkwrap/VkWrap.hpp>

namespace blaze
{
class FwdRenderer final : public ARenderer
{
private:
	vkw::RenderPass renderPass;
	Texture2D depthBuffer;
	vkw::FramebufferVector renderFramebuffers;

public:
	/**
	 * @fn FwdRenderer()
	 *
	 * @brief Default empty constructor.
	 */
	FwdRenderer() noexcept
	{
	}

	/**
	 * @fn FwdRenderer(GLFWwindow* window, bool enableValidationLayers = true)
	 *
	 * @brief Actual constructor
	 *
	 * @param window The GLFW window pointer.
	 * @param enableValidationLayers Enable for Debugging.
	 */
	FwdRenderer(GLFWwindow* window, bool enableValidationLayers = true) noexcept;

	/**
	 * @name Constructors.
	 *
	 * @brief Pointer use only.
	 * @{
	 */
	FwdRenderer(FwdRenderer&& other) = delete;
	FwdRenderer& operator=(FwdRenderer&& other) = delete;
	FwdRenderer(const FwdRenderer& other) = delete;
	FwdRenderer& operator=(const FwdRenderer& other) = delete;
	/**
	 * @}
	 */

	~FwdRenderer()
	{
		clearCommandBuffers();
	}

protected:
	void update();
	void recordCommands(uint32_t frame);
	void recreateSwapchainDependents();

private:
	vkw::RenderPass createRenderpass() const;
	Texture2D createDepthBuffer() const;
	vkw::FramebufferVector createFramebuffers() const;
};
} // namespace blaze