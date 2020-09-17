
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/Texture2D.hpp>
#include <rendering/ARenderer.hpp>
#include <rendering/deferred/DfrLightCaster.hpp>
#include <core/VertexBuffer.hpp>
#include <rendering/postprocess/HdrTonemap.hpp>
#include <rendering/postprocess/Bloom.hpp>

#include "SSAO.hpp"

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
	constexpr static std::string_view vMRTShaderFileName = "shaders/deferred/vMRT.vert.spv";
	constexpr static std::string_view fMRTShaderFileName = "shaders/deferred/fMRT.frag.spv";

	constexpr static std::string_view vLightingShaderFileName = "shaders/deferred/vLighting.vert.spv";
	constexpr static std::string_view fLightingShaderFileName = "shaders/deferred/fLighting.frag.spv";

	constexpr static std::string_view vDirLightingShaderFileName = "shaders/deferred/vDirLighting.vert.spv";
	constexpr static std::string_view fDirLightingShaderFileName = "shaders/deferred/fDirLighting.frag.spv";

	constexpr static std::string_view vTransparencyShaderFileName = "shaders/deferred/vTransparency.vert.spv";
	constexpr static std::string_view fTransparencyShaderFileName = "shaders/deferred/fTransparency.frag.spv";

	constexpr static std::string_view vLightVisShaderFileName = "shaders/deferred/vLightVis.vert.spv";
	constexpr static std::string_view fLightVisShaderFileName = "shaders/deferred/fLightVis.frag.spv";

	struct Settings
	{
		int enableIBL{1};
		enum : int
		{
			RENDER = 0x0,
			POSITION = 0x1,
			NORMAL = 0x2,
			ALBEDO = 0x3,
			AO = 0x4,
			METALLIC = 0x5,
			ROUGHNESS = 0x6,
			EMISSION = 0x7,
			IBL = 0x8,
		} viewRT{RENDER};
		int useVertexNormals{0};
		float modRoughness{-1.0f};

		void draw();
	} settings;

	using CameraUBOV = UBOVector<Camera::UBlock>;
	using SettingsUBOV = UBOVector<Settings>;

	Texture2D depthBuffer;

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
	};

	// MRT
	spirv::RenderPass mrtRenderPass;
	MRTAttachment mrtAttachment;
	spirv::Framebuffer mrtFramebuffer;

	spirv::SetSingleton lightInputSet;

	spirv::Shader mrtShader;
	spirv::Pipeline mrtPipeline;

	// SSAO
	std::unique_ptr<SSAO> ssao;

	// Lighting
	spirv::RenderPass lightingRenderPass;
	Texture2D lightingAttachment;
	spirv::Framebuffer lightingFramebuffer;

	spirv::Shader pointLightShader;
	spirv::Pipeline pointLightPipeline;

	spirv::Shader dirLightShader;
	spirv::Pipeline dirLightPipeline;

	// Transparency
	spirv::Shader forwardShader;
	spirv::Pipeline forwardPipeline;

	// Data
	CameraUBOV cameraUBOs;
	SettingsUBOV settingsUBOs;
	spirv::SetVector cameraSets;

	// Lights
	IndexedVertexBuffer<Vertex> lightVolume;
	IndexedVertexBuffer<Vertex> lightQuad;

	bool visualizeLights;

	std::unique_ptr<DfrLightCaster> lightCaster;

	spirv::SetSingleton environmentSet;

	spirv::Shader lightVisShader;
	spirv::Pipeline lightVisPipeline;

	// Bloom
	Bloom bloom;

	// Post processing

	spirv::RenderPass postProcessRenderPass;
	std::vector<spirv::Framebuffer> postProcessFramebuffers;

	HDRTonemap hdrTonemap;

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
	virtual void drawSettings() override;

protected:
	virtual void update(uint32_t frame) override;
	virtual void recordCommands(uint32_t frame) override;
	virtual void recreateSwapchainDependents() override;

private:
	spirv::RenderPass createMRTRenderpass();
	spirv::RenderPass createLightingRenderpass();
	Texture2D createDepthBuffer() const;
	Texture2D createLightingAttachment() const;

	spirv::Shader createMRTShader();
	spirv::Pipeline createMRTPipeline();
	MRTAttachment createMRTAttachment();
	spirv::SetSingleton createLightingInputSet();


	// Lighting
	spirv::Shader createPointLightingShader();
	spirv::Pipeline createPointLightingPipeline();

	spirv::Shader createDirLightingShader();
	spirv::Pipeline createDirLightingPipeline();

	spirv::Shader createLightVisShader();
	spirv::Pipeline createLightVisPipeline();

	spirv::Framebuffer createRenderFramebuffer();
	spirv::Framebuffer createLightingFramebuffer();

	spirv::SetVector createCameraSets();
	CameraUBOV createCameraUBOs();
	SettingsUBOV createSettingsUBOs();

	// Transparency
	spirv::Shader createForwardShader();
	spirv::Pipeline createForwardPipeline();

	// Post process
	spirv::RenderPass createPostProcessRenderPass();
	std::vector<spirv::Framebuffer> createPostProcessFramebuffers();

	// Inherited via ARenderer
	virtual spirv::SetSingleton* get_environmentSet() override;
};
} // namespace blaze
