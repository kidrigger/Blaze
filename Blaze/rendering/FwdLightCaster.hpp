
#pragma once

#include <util/PackedHandler.hpp>
#include <core/Context.hpp>
#include <spirv/PipelineFactory.hpp>
#include <core/UniformBuffer.hpp>
#include <core/Bindable.hpp>
#include <rendering/ALightCaster.hpp>

#include <glm/glm.hpp>

#include <set>

namespace blaze
{

class PointLightCaster
{
	struct LightData
	{
		alignas(16) glm::vec3 position;
		alignas(4) float brightness;
		alignas(4) int shadowIdx;
	};

private:
	uint32_t count;
	int16_t freeLight;
	std::vector<LightData> lights;

	UBODataVector ubos;

public:
	PointLightCaster(const Context* context, const spirv::SetVector& sets, uint32_t maxPointLights) noexcept;
	void recreate(const Context* context, const spirv::SetVector& sets);
	void update(uint32_t frame);

	uint16_t createLight(const glm::vec3& position, float brightness, bool enableShadow);
	LightData* getLight(uint16_t idx)
	{
		return &lights[idx];
	}
	void removeLight(uint16_t idx);

	inline uint32_t get_count() const
	{
		return count;
	}

private:
	void bindDataSet(const Context* context, const spirv::SetVector& sets);
};

class FwdLightCaster : public ALightCaster
{
private:
	spirv::SetVector dataSet;

	PointLightCaster pointLights;
	uint8_t pointGeneration;
	std::set<Handle> validHandles;

	struct HandleExposed
	{
		uint8_t type;
		uint8_t gen;
		uint16_t idx;
	};

public:
	FwdLightCaster(const Context* context, spirv::SetVector&& dataSet) noexcept;

	void recreate(const Context* context, spirv::SetVector&& dataSet);

	void bind(VkCommandBuffer buf, VkPipelineLayout lay, uint32_t frame) const;

	// Inherited via ALightCaster
	virtual Handle createPointLight(const glm::vec3& position, float brightness, bool enableShadow) override;
	virtual void removeLight(Handle handle) override;
	virtual void setPosition(Handle handle, const glm::vec3& position) override;
	virtual void setDirection(Handle handle, const glm::vec3& direction) override;
	virtual void setBrightness(Handle handle, float brightness) override;
	virtual void setShadow(Handle handle, bool hasShadow) override;
	virtual void update(uint32_t frame) override;
};
}
