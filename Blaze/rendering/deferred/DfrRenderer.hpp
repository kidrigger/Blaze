
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <core/Texture2D.hpp>
#include <rendering/ARenderer.hpp>
#include <rendering/deferred/DfrLightCaster.hpp>
#include <core/VertexBuffer.hpp>

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
	
	struct HDRTonemapPostProcess
	{
		constexpr static std::string_view vShaderFile = "shaders/postprocess/vHDRTonemap.vert.spv";
		constexpr static std::string_view fShaderFile = "shaders/postprocess/fHDRTonemap.frag.spv";

		spirv::Shader shader;
		spirv::Pipeline pipeline;

		struct PushConstant
		{
			float exposure{4.5f};
			float gamma{2.2f};
			float pad_[2];
		} pushConstant;

		spirv::SetSingleton colorSampler;

		HDRTonemapPostProcess()
		{
		}

		HDRTonemapPostProcess(Context* context, spirv::RenderPass* renderPass, Texture2D* colorOutput);

		void recreate(Context* context, spirv::RenderPass* renderPass, Texture2D* colorOutput);

		void process(VkCommandBuffer cmdBuf, IndexedVertexBuffer<Vertex>& screenRect)
		{
			pipeline.bind(cmdBuf);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.pipelineLayout.get(),
									colorSampler.setIdx, 1, &colorSampler.set.get(), 0, nullptr);
			vkCmdPushConstants(cmdBuf, shader.pipelineLayout.get(), shader.pushConstant.stage, 0,
							   shader.pushConstant.size, &pushConstant);
			vkCmdDrawIndexed(cmdBuf, screenRect.get_indexCount(), 1, 0, 0, 0);
		}
	};

	struct Settings
	{
		enum : int
		{
			POSITION = 0x0,
			NORMAL = 0x1,
			ALBEDO = 0x2,
			OMR = 0x3,
			EMISSION = 0x4,
			RENDER = 0x5,
		} viewRT{RENDER};
	} settings;

	using CameraUBOV = UBOVector<Camera::UBlock>;
	using SettingsUBOV = UBOVector<Settings>;

	Texture2D depthBuffer;
	spirv::RenderPass renderPass;

	struct MRTAttachment
	{
		Texture2D output;

		Texture2D position;
		Texture2D normal;
		Texture2D albedo;
		Texture2D omr;
		Texture2D emission;

		constexpr static uint32_t attachmentCount = 5;

		bool valid() const
		{
			return output.valid() && position.valid() && normal.valid() && albedo.valid() && omr.valid() && emission.valid();
		}
	} mrtAttachment;
	spirv::SetSingleton inputAttachmentSet;

	spirv::Shader mrtShader;
	spirv::Pipeline mrtPipeline;

	spirv::Shader pointLightShader;
	spirv::Pipeline pointLightPipeline;

	spirv::Shader dirLightShader;
	spirv::Pipeline dirLightPipeline;

	vkw::Framebuffer renderFramebuffer;

	CameraUBOV cameraUBOs;
	SettingsUBOV settingsUBOs;
	spirv::SetVector cameraSets;

	IndexedVertexBuffer<Vertex> lightVolume;
	IndexedVertexBuffer<Vertex> lightQuad;

	std::unique_ptr<DfrLightCaster> lightCaster;

	// Post processing

	spirv::RenderPass postProcessRenderPass;
	vkw::FramebufferVector postProcessFramebuffers;

	HDRTonemapPostProcess hdrTonemap;

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
	virtual void setEnvironment(const Bindable* env) override;	// TODO
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
	spirv::SetSingleton createInputAttachmentSet();

	spirv::Shader createPointLightingShader();
	spirv::Pipeline createPointLightingPipeline();

	spirv::Shader createDirLightingShader();
	spirv::Pipeline createDirLightingPipeline();

	vkw::Framebuffer createRenderFramebuffer();

	spirv::SetVector createCameraSets();
	CameraUBOV createCameraUBOs();
	SettingsUBOV createSettingsUBOs();

	// Post process
	spirv::RenderPass createPostProcessRenderPass();
	vkw::FramebufferVector createPostProcessFramebuffers();
};
} // namespace blaze
