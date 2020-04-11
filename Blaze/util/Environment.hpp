
#pragma once

#include <Renderer.hpp>

namespace blaze::util
{
TextureCube createIrradianceCube(const Renderer& renderer, VkDescriptorSet environment);
TextureCube createPrefilteredCube(const Renderer& renderer, VkDescriptorSet environment);
Texture2D createBrdfLut(const Context& context);
} // namespace blaze::util