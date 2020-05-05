
#pragma once

#include <core/Context.hpp>
#include <rendering/ARenderer.hpp>
#include <core/Bindable.hpp>

namespace blaze::util
{
TextureCube createIrradianceCube(const Context* context, VkDescriptorSetLayout envLayout, VkDescriptorSet environment);
TextureCube createPrefilteredCube(const Context* context, VkDescriptorSetLayout envLayout, VkDescriptorSet environment);
Texture2D createBrdfLut(const Context* context);

/**
 * @brief Holder for all the environment texture maps and descriptor set.
 *
 * The Environment textures for current renderers are the PBR/IBL maps.
 */
class Environment : public Bindable
{
public:
	TextureCube skybox;
	TextureCube irradianceMap;
	TextureCube prefilteredMap;
	Texture2D brdfLut;
	spirv::SetSingleton set;

	Environment()
	{
	}

	Environment(ARenderer* renderer, TextureCube&& skybox);

	void bind(VkCommandBuffer cmdBuf, VkPipelineLayout lay) const
	{
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, lay, set.setIdx, 1, &set.get(), 0, nullptr);
	}
};
} // namespace blaze::util
