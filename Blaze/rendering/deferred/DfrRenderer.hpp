
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/Texture2D.hpp>
#include <rendering/ARenderer.hpp>
#include <rendering/deferred/DfrLightCaster.hpp>

namespace blaze
{
/**
 * @brief Deferred rendering specialization of ARenderer
 *
 * This class will be able to setup a PBR/IBL deferred renderer.
 */
class DfrRenderer final : public ARenderer
{
private:
	constexpr static std::string_view vertShaderFileName = "shaders/deferred/vMRT.vert.spv";
	constexpr static std::string_view fragShaderFileName = "shaders/deferred/fMRT.frag.spv";

	using CameraUBOV = UBOVector<Camera::UBlock>;

	struct DeferredSettings
	{
		enum
		{
			POSITION,
			NORMAL,
			ALBEDO,
			OMR,
			EMISSION,
		} viewRT{POSITION};
	} dsettings;

	Texture2D depthBuffer;
	spirv::RenderPass renderPass;

	struct MRTAttachment
	{
		Texture2D position;
		Texture2D normal;
		Texture2D albedo;
		Texture2D omr;
		Texture2D emission;

		constexpr static uint32_t attachmentCount = 5;

		bool valid() const
		{
			return position.valid() && normal.valid() && albedo.valid() && omr.valid() && emission.valid();
		}
	} mrtAttachment;

	spirv::Shader mrtShader;
	spirv::Pipeline mrtPipeline;
	vkw::FramebufferVector framebuffers;

	CameraUBOV cameraUBOs;
	spirv::SetVector cameraSets;

	std::unique_ptr<DfrLightCaster> lightCaster;

public:
	/**
	 * @fn DfrRenderer()
	 *
	 * @brief Default empty constructor.
	 */
	DfrRenderer() noexcept
	{
	}

	/**
	 * @brief Actual constructor
	 *
	 * @param window The GLFW window pointer.
	 * @param enableValidationLayers Enable for Debugging.
	 */
	DfrRenderer(GLFWwindow* window, bool enableValidationLayers = true) noexcept;

	/**
	 * @name Constructors.
	 *
	 * @brief Pointer use only.
	 * @{
	 */
	DfrRenderer(DfrRenderer&& other) = delete;
	DfrRenderer& operator=(DfrRenderer&& other) = delete;
	DfrRenderer(const DfrRenderer& other) = delete;
	DfrRenderer& operator=(const DfrRenderer& other) = delete;
	/**
	 * @}
	 */

	// Inherited via ARenderer
	virtual const spirv::Shader* get_shader() const override;
	virtual ALightCaster* get_lightCaster() override;
	virtual void setEnvironment(const Bindable* env) override;
	virtual void drawSettings() override;

protected:
	virtual void update(uint32_t frame) override;
	virtual void recordCommands(uint32_t frame) override;
	virtual void recreateSwapchainDependents() override;

private:
	spirv::RenderPass createRenderpass();
	Texture2D createDepthBuffer() const;
	spirv::Shader createMRTShader();
	spirv::Pipeline createMRTPipeline();
	MRTAttachment createMRTAttachment();
	vkw::FramebufferVector createFramebuffers();

	spirv::SetVector createCameraSets();
	CameraUBOV createCameraUBOs();
};
} // namespace blaze
