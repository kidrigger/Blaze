
#pragma once

#include <core/Context.hpp>
#include <spirv/PipelineFactory.hpp>
#include <core/Bindable.hpp>
#include <core/Texture2D.hpp>
#include <core/TextureCube.hpp>

namespace blaze::util
{

/**
 * @brief Holder for all the environment texture maps and descriptor set.
 *
 * The Environment textures for current renderers are the PBR/IBL maps.
 */
class Environment
{
public:
	TextureCube skybox;
	TextureCube irradianceMap;
	TextureCube prefilteredMap;
	Texture2D brdfLut;

	Environment()
	{
	}

	Environment(const Context* context, TextureCube&& skybox, spirv::SetSingleton* environment);

private:
	TextureCube createIrradianceCube(const Context* context, spirv::SetSingleton* environment);
	TextureCube createPrefilteredCube(const Context* context, spirv::SetSingleton* environment);
	Texture2D createBrdfLut(const Context* context);
};
} // namespace blaze::util
