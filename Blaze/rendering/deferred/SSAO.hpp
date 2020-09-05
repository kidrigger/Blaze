
#pragma once

#include <core/Texture2D.hpp>
#include <core/UniformBuffer.hpp>
#include <core/VertexBuffer.hpp>
#include <spirv/PipelineFactory.hpp>

namespace blaze
{
struct SSAO
{
private:
	constexpr static std::string_view vShaderFileName = "shaders/deferred/vSSAO.vert.spv";
	constexpr static std::string_view fShaderFileName = "shaders/deferred/fSSAO.frag.spv";

	constexpr static std::string_view vBlurShaderFileName = "shaders/deferred/vSSAO.vert.spv";
	constexpr static std::string_view fBlurShaderFileName = "shaders/deferred/fSSAOBlur.frag.spv";

	constexpr static std::string_view vFilterShaderFileName = "shaders/deferred/vSSAO.vert.spv";
	constexpr static std::string_view fFilterShaderFileName = "shaders/deferred/fSSAOFilter.frag.spv";

public:
	VkViewport halfViewport;
	VkRect2D halfScissor;

	VkViewport viewport;
	VkRect2D scissor;

	Texture2D noise;
	BaseUBO kernel;
	spirv::SetSingleton kernelSet;

	bool enabled{true};

	struct Settings
	{
		float kernelRadius{0.5f};
		float bias{0.025f};
	} settings;

	bool blurEnabled{true};
	int blurCount{3};
	struct SSAOBlurSettings
	{
		int verticalPass{1};
		int depthAware{1};
		float depth{0.02f};
	} blurSettings;

	Texture2D samplingAttachment;
	spirv::Framebuffer samplingFramebuffer;
	spirv::RenderPass renderpass;
	spirv::Shader samplingShader;
	spirv::Pipeline samplingPipeline;
	spirv::SetSingleton samplingPosNormSet;

	Texture2D blurAttachment;
	spirv::Framebuffer blurFramebuffer;
	spirv::Shader blurShader;
	spirv::Pipeline blurPipeline;
	spirv::SetSingleton blurSet;

	spirv::Framebuffer filterFramebuffer;
	spirv::RenderPass filterRenderPass;
	spirv::Shader filterShader;
	spirv::Pipeline filterPipeline;
	spirv::SetSingleton filterSet;

	SSAO(const Context* context, const Texture2D* position, const Texture2D* normal, const Texture2D* omr);

	void recreate(const Context* context, const Texture2D* position, const Texture2D* normal, const Texture2D* omr);

	void process(VkCommandBuffer cmd, spirv::SetVector& cameraSets, uint32_t frame,
				 IndexedVertexBuffer<Vertex>& lightQuad);

	void drawSettings();

private:
	Texture2D createNoise(const Context* context);
	BaseUBO createKernel(const Context* context);
	spirv::SetSingleton createKernelSet(const Context* context);

	Texture2D createSamplingAttachment(const Context* context);
	spirv::RenderPass createSamplingRenderpass(const Context* context);
	spirv::Shader createSamplingShader(const Context* context);
	spirv::Pipeline createSamplingPipeline(const Context* context);
	spirv::Framebuffer createSamplingFramebuffer(const Context* context);
	spirv::SetSingleton createSamplingPosNormSet(const Context* context, const Texture2D* normal, const Texture2D* position);

	spirv::Framebuffer createBlurFramebuffer(const Context* context);
	spirv::Shader createBlurShader(const Context* context);
	spirv::Pipeline createBlurPipeline(const Context* context);
	spirv::SetSingleton createBlurSet(const Context* context, const Texture2D* position);

	spirv::RenderPass createFilterRenderPass(const Context* context, const Texture2D* omr);
	spirv::Framebuffer createFilterFramebuffer(const Context* context, const Texture2D* omr);
	spirv::Shader createFilterShader(const Context* context);
	spirv::Pipeline createFilterPipeline(const Context* context);
	spirv::SetSingleton createFilterSet(const Context* context, const Texture2D* position);
};
} // namespace blaze