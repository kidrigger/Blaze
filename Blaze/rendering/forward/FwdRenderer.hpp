
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <rendering/ARenderer.hpp>
#include <vkwrap/VkWrap.hpp>

#include <spirv/PipelineFactory.hpp>
#include <core/VertexBuffer.hpp>

#include <core/Bindable.hpp>

#include <rendering/forward/FwdLightCaster.hpp>

namespace blaze
{
/**
 * @brief Forward rendering specialization of ARenderer
 *
 * This class is able to setup a PBR/IBL forward renderer.
 */
class FwdRenderer final : public ARenderer
{
private:
	constexpr static std::string_view vertShaderFileName = "shaders/forward/vPBR.vert.spv";
	constexpr static std::string_view fragShaderFileName = "shaders/forward/fPBR.frag.spv";

	constexpr static std::string_view vertSkyboxShaderFileName = "shaders/forward/vSkybox.vert.spv";
	constexpr static std::string_view fragSkyboxShaderFileName = "shaders/forward/fSkybox.frag.spv";

	struct SettingsUBlock
	{
		alignas(4) float exposure;
		alignas(4) float gamma;
		alignas(4) int enableIBL;
	} settings{
		4.5f,
		2.2f,
		1,
	};

	using CameraUBOV = UBOVector<Camera::UBlock>;
	using SettingsUBOV = UBOVector<SettingsUBlock>;

	Texture2D depthBuffer;
	spirv::RenderPass renderPass;
	std::vector<spirv::Framebuffer> renderFramebuffers;
	
	spirv::Shader shader;
	spirv::Pipeline pipeline;
	spirv::Shader skyboxShader;
	spirv::Pipeline skyboxPipeline;

	CameraUBOV cameraUBOs;
	SettingsUBOV settingsUBOs;
	spirv::SetVector cameraSets;

	spirv::SetSingleton environmentSet;
	IndexedVertexBuffer<Vertex> skyboxCube;

	std::unique_ptr<FwdLightCaster> lightCaster;

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

	// Inherited via ARenderer
	virtual const spirv::Shader* get_shader() const override;
	virtual FwdLightCaster* get_lightCaster() override;
	virtual void drawSettings() override;

	~FwdRenderer()
	{
		clearCommandBuffers();
	}

protected:
	virtual void update(uint32_t frame) override;
	virtual void recordCommands(uint32_t frame) override;
	virtual void recreateSwapchainDependents() override;

	virtual spirv::SetSingleton* get_environmentSet() override;

private:
	spirv::RenderPass createRenderpass();
	Texture2D createDepthBuffer() const;
	std::vector<spirv::Framebuffer> createFramebuffers();
	spirv::Shader createShader();
	spirv::Pipeline createPipeline();
	spirv::Shader createSkyboxShader();
	spirv::Pipeline createSkyboxPipeline();
	spirv::SetVector createCameraSets();
	CameraUBOV createCameraUBOs();
	SettingsUBOV createSettingsUBOs();
};
} // namespace blaze
