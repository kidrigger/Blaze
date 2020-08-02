
#pragma once

#include <core/Context.hpp>
#include <core/Drawable.hpp>
#include <core/TextureCube.hpp>
#include <core/StorageBuffer.hpp>
#include <core/UniformBuffer.hpp>
#include <spirv/PipelineFactory.hpp>

namespace blaze::dfr
{
/**
 * @class PointShadow
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
struct PointShadow
{
	TextureCube shadowMap;
	spirv::Framebuffer framebuffer;
	VkRect2D scissor;
	VkViewport viewport;
	uint16_t next;

	struct PCB
	{
		alignas(16) glm::vec3 position;
		alignas(4) float radius;
		alignas(4) float p22;
		alignas(4) float p32;
	};

	PointShadow(const Context* context, const spirv::RenderPass& renderPass, uint32_t mapResolution) noexcept;
};
/**
 * @endcond
 */

class PointLightCaster
{
private:
	constexpr static uint32_t OMNI_MAP_RESOLUTION = 512;

	struct LightData
	{
		alignas(16) glm::vec3 position;
		alignas(4) float brightness;
		alignas(4) float radius;
		alignas(4) int shadowIdx;
	};

	uint32_t maxLights;
	uint32_t maxShadows;

	uint32_t count;
	int16_t freeLight;
	std::vector<LightData> lights;

	constexpr static std::string_view dataUniformName = "lights";
	constexpr static std::string_view textureUniformName = "shadows";

	constexpr static std::string_view vertShaderFileName = "shaders/forward/vPointShadow.vert.spv";
	constexpr static std::string_view fragShaderFileName = "shaders/forward/fPointShadow.frag.spv";

	spirv::RenderPass renderPass;
	spirv::Shader shadowShader;
	spirv::Pipeline shadowPipeline;

	spirv::SetSingleton viewSet;
	UBO<CubemapUBlock> viewUBO;

	SSBODataVector ubos;

	uint32_t shadowCount;
	int freeShadow;
	std::vector<PointShadow> shadows;

public:
	PointLightCaster(const Context* context, uint32_t numLights, const spirv::SetVector& sets, const spirv::SetSingleton& texSet) noexcept;
	void recreate(const Context* context, const spirv::SetVector& sets);
	void update(uint32_t frame);

	uint16_t createLight(const glm::vec3& position, float brightness, float radius, bool enableShadow);
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

	struct LightIterator
	{
		int index;
		LightData* data;
		int end;

		LightIterator& operator++()
		{
			do
			{
				data++;
				index++;
			} while (data->brightness < 0 && index < end);
			return *this;
		}

		bool valid()
		{
			return index < end;
		}

	private:
		LightIterator(std::vector<LightData>& lights)
		{
			end = static_cast<int>(lights.size());
			data = lights.data();
			index = end;
			int i = 0;
			for (auto& light : lights)
			{
				if (light.brightness > 0)
				{
					index = i;
					break;
				}
				i++;
			}
		}

		friend class PointLightCaster;
	};

	LightIterator getLightIterator()
	{
		return LightIterator(lights);
	}

private:
	void bindDataSet(const Context* context, const spirv::SetVector& sets);
	void bindTextureSet(const Context* context, const spirv::SetSingleton& set);
	spirv::RenderPass createRenderPass(const Context* context);
	spirv::Shader createShader(const Context* context);
	spirv::Pipeline createPipeline(const Context* context);
};
} // namespace blaze