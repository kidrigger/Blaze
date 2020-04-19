
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <rendering/ARenderer.hpp>
#include <vkwrap/VkWrap.hpp>

#include <spirv/PipelineFactory.hpp>

namespace blaze
{
class FwdRenderer final : public ARenderer
{
private:
	spirv::PipelineFactory pipelineFactory;
	Texture2D depthBuffer;
	spirv::RenderPass renderPass;
	vkw::FramebufferVector renderFramebuffers;
	spirv::Shader shader;
	spirv::Pipeline pipeline;

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
	spirv::RenderPass createRenderpass();
	Texture2D createDepthBuffer() const;
	vkw::FramebufferVector createFramebuffers() const;
	spirv::Shader createShader();
	spirv::Pipeline createPipeline();
};
} // namespace blaze