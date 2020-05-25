
#pragma once

#include <core/Context.hpp>
#include <core/Drawable.hpp>
#include <core/Texture2D.hpp>
#include <core/UniformBuffer.hpp>
#include <spirv/PipelineFactory.hpp>

namespace blaze
{
/**
 * @class DirectionShadow2
 *
 * @brief Encapsulates the attachments and framebuffer for a point light shadow.
 *
 * Contains the shadowMap (D32) along with the framebuffer and
 * viewport configured.
 *
 * @note Not supposed to be used externally.
 *
 * @cond PRIVATE
 */
struct DirectionShadow2
{
	Texture2D shadowMap;
	vkw::Framebuffer framebuffer;
	VkViewport viewport;
	uint16_t next;

	struct PCB
	{
		alignas(16) glm::vec3 position;
		alignas(4) float radius;
		alignas(4) float p22;
		alignas(4) float p32;
	};

	DirectionShadow2(const Context* context, VkRenderPass renderPass, uint32_t mapResolution, uint32_t numCascades) noexcept;
};
/**
 * @endcond
 */

class DirectionLightCaster
{
private:
	constexpr static uint32_t DIRECTION_MAP_RESOLUTION = 512;

	struct LightData
	{
		alignas(16) glm::vec3 direction;
		alignas(4) float brightness;
		alignas(4) int numCascades;
		alignas(4) int shadowIdx;
	};

	uint32_t maxLights;
	uint32_t maxShadows;

	uint32_t count;
	int16_t freeLight;
	std::vector<LightData> lights;

	constexpr static std::string_view dataUniformName = "dirLights";
	constexpr static std::string_view textureUniformName = "dirShadows";

	spirv::RenderPass renderPass;
	spirv::Shader shadowShader;
	spirv::Pipeline shadowPipeline;

	spirv::SetSingleton viewSet;
	UBO<CubemapUBlock> viewUBO;
	spirv::SetVector shadowInfoSet;

	UBODataVector ubos;

	uint32_t shadowCount;
	int freeShadow;
	std::vector<DirectionShadow2> shadows;

public:
	DirectionLightCaster(const Context* context, const spirv::SetVector& sets,
						const spirv::SetSingleton& texSet) noexcept;
	void recreate(const Context* context, const spirv::SetVector& sets);
	void update(uint32_t frame);

	uint16_t createLight(const glm::vec3& direction, float brightness, uint32_t numCascades);
	void removeLight(uint16_t idx);
	bool setShadow(uint16_t idx, bool enableShadow);

	int createShadow();
	void removeShadow(int idx);

	LightData* getLight(uint16_t idx)
	{
		return &lights[idx];
	}

	inline uint32_t get_count() const
	{
		return count;
	}

	inline uint32_t getMaxLights() const
	{
		return maxLights;
	}

	inline uint32_t getMaxShadows() const
	{
		return maxShadows;
	}

	void cast(VkCommandBuffer cmd, const std::vector<Drawable*>& drawables);

private:
	void bindDataSet(const Context* context, const spirv::SetVector& sets);
	void bindTextureSet(const Context* context, const spirv::SetSingleton& set);
	spirv::RenderPass createRenderPass(const Context* context);
	spirv::Shader createShader(const Context* context);
	spirv::Pipeline createPipeline(const Context* context);
};
}
