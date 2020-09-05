#pragma once

#include <core/Context.hpp>
#include <core/Texture2D.hpp>
#include <core/VertexBuffer.hpp>
#include <string>

namespace blaze
{
struct Bloom
{
	constexpr static std::string_view vHighpassShaderFile = "shaders/postprocess/vPostProcessQuad.vert.spv";
	constexpr static std::string_view fHighpassShaderFile = "shaders/postprocess/fHighpass.frag.spv";

	constexpr static std::string_view vBloomShaderFile = "shaders/postprocess/vPostProcessQuad.vert.spv";
	constexpr static std::string_view fBloomShaderFile = "shaders/postprocess/fBloom.frag.spv";

	constexpr static std::string_view vCombineShaderFile = "shaders/postprocess/vPostProcessQuad.vert.spv";
	constexpr static std::string_view fCombineShaderFile = "shaders/postprocess/fCombine.frag.spv";

	bool enabled{true};

	spirv::Framebuffer pingPong[2];
	Texture2D pingPongAttachment[2];
	spirv::Framebuffer outputFB;

	spirv::RenderPass renderpass;

	spirv::Shader highpassShader;
	spirv::Pipeline highpassPipeline;

	spirv::Shader bloomShader;
	spirv::Pipeline bloomPipeline;

	spirv::Shader combineShader;
	spirv::Pipeline combinePipeline;

	spirv::SetSingleton attachSets[2];
	spirv::SetSingleton inputSet;

	VkViewport halfViewport;
	VkViewport viewport;

	struct HighpassSettings
	{
		float threshold{1.2f};
		float tolerance{0.2f};
	} highpassSettings;
	int iterations{5};

	Bloom() noexcept
	{
	}

	Bloom(const Context* context, Texture2D* colorOutput);

	void drawSettings();

	void recreate(const Context* context, Texture2D* colorOutput);
	void process(VkCommandBuffer cmd, IndexedVertexBuffer<Vertex>& quad);
};
} // namespace blaze