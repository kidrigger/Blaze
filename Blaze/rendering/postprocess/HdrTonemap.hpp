
#pragma once

#include <string_view>
#include <core/Context.hpp>
#include <core/Texture2D.hpp>
#include <core/VertexBuffer.hpp>
#include <gui/GUI.hpp>

namespace blaze
{
struct HDRTonemap
{
	constexpr static std::string_view vShaderFile = "shaders/postprocess/vPostProcessQuad.vert.spv";
	constexpr static std::string_view fShaderFile = "shaders/postprocess/fHDRTonemap.frag.spv";

	spirv::Shader shader;
	spirv::Pipeline pipeline;

	struct PushConstant
	{
		float exposure{4.5f};
		float gamma{2.2f};
		float enable{1};
		float pad_;
	} pushConstant;

	spirv::SetSingleton colorSampler;

	HDRTonemap()
	{
	}

	HDRTonemap(Context* context, spirv::RenderPass* renderPass, Texture2D* colorOutput);

	void drawSettings()
	{
		if (ImGui::CollapsingHeader("Tonemap Settings##HDRPostProcess"))
		{
			ImGui::DragFloat("Exposure", &pushConstant.exposure, 0.1f, 1.0f, 10.0f);
			ImGui::DragFloat("Gamma", &pushConstant.gamma, 0.1f, 1.0f, 4.0f);
		}
	}

	void recreate(Context* context, spirv::RenderPass* renderPass, Texture2D* colorOutput);

	void process(VkCommandBuffer cmdBuf, IndexedVertexBuffer<Vertex>& screenRect)
	{
		pipeline.bind(cmdBuf);
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.pipelineLayout.get(),
								colorSampler.setIdx, 1, &colorSampler.set.get(), 0, nullptr);
		vkCmdPushConstants(cmdBuf, shader.pipelineLayout.get(), shader.pushConstant.stage, 0, shader.pushConstant.size,
						   &pushConstant);
		screenRect.bind(cmdBuf);
		vkCmdDrawIndexed(cmdBuf, screenRect.get_indexCount(), 1, 0, 0, 0);
	}
};
} // namespace blaze