
#pragma once

#include <core/Context.hpp>
#include <core/Drawable.hpp>
#include <core/Texture2D.hpp>
#include <core/UniformBuffer.hpp>
#include <core/StorageBuffer.hpp>
#include <spirv/PipelineFactory.hpp>
#include <core/Camera.hpp>

namespace blaze::dfr
{
/**
 * @class DirectionShadow
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
struct DirectionShadow
{
	Texture2D shadowMap;
	std::vector<spirv::Framebuffer> framebuffer;
	VkViewport viewport;
	VkRect2D scissor;
	uint16_t next;

	struct PCB
	{
		alignas(16) glm::vec3 direction;
		alignas(4) float brightness;
		alignas(4) float p22;
		alignas(4) float p32;
	};

	DirectionShadow(const Context* context, const spirv::RenderPass& renderPass, uint32_t mapResolution,
					uint32_t numCascades) noexcept;
};
/**
 * @endcond
 */

class DirectionLightCaster
{
private:
	constexpr static uint32_t DIRECTION_MAP_RESOLUTION = 1024;

	struct LightData
	{
		alignas(16) glm::vec3 direction;
		alignas(4) float brightness;
		alignas(16) glm::vec4 cascadeSplits;
		alignas(16) glm::mat4 cascadeViewProj[MAX_CSM_SPLITS];
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
	
	constexpr static std::string_view vertShaderFileName = "shaders/forward/vDirectionShadow.vert.spv";
	constexpr static std::string_view fragShaderFileName = "shaders/forward/fDirectionShadow.frag.spv";

	spirv::RenderPass renderPass;
	spirv::Shader shadowShader;
	spirv::Pipeline shadowPipeline;

	SSBODataVector ubos;

	uint32_t shadowCount;
	int freeShadow;
	std::vector<DirectionShadow> shadows;

public:
	DirectionLightCaster(const Context* context, uint32_t numLights, const spirv::SetVector& sets,
						const spirv::SetSingleton& texSet) noexcept;
	void recreate(const Context* context, const spirv::SetVector& sets);
	void update(const Camera* camera, uint32_t frame);

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

	float centerDist(float n, float f, float cosine) const;
	glm::vec4 createCascadeSplits(int numSplits, float nearPlane, float farPlane, float lambda = 0.5f) const;
	void updateLight(const Camera* camera, LightData* light);
};
}
