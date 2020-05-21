
#pragma once

#include <core/Bindable.hpp>
#include <core/Context.hpp>
#include <core/UniformBuffer.hpp>
#include <rendering/ALightCaster.hpp>
#include <spirv/PipelineFactory.hpp>
#include <util/PackedHandler.hpp>

#include "PointLightCaster.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <set>

namespace blaze
{
class FwdLightCaster : public ALightCaster
{
private:
	spirv::SetVector dataSet;
	spirv::SetSingleton textureSet;

	std::unique_ptr<PointLightCaster> pointLights;
	uint8_t pointGeneration;
	std::set<Handle> validHandles;

	struct HandleExposed
	{
		uint8_t type;
		uint8_t gen;
		uint16_t idx;
	};

public:
	FwdLightCaster(const Context* context, const spirv::Shader* shader, uint32_t frames) noexcept;

	void recreate(const Context* context, const spirv::Shader* shader, uint32_t frames);

	void bind(VkCommandBuffer buf, VkPipelineLayout lay, uint32_t frame) const;

	// Inherited via ALightCaster
	virtual Handle createPointLight(const glm::vec3& position, float brightness, float radius,
									bool enableShadow) override;
	virtual void removeLight(Handle handle) override;
	virtual void setPosition(Handle handle, const glm::vec3& position) override;
	virtual void setDirection(Handle handle, const glm::vec3& direction) override;
	virtual void setBrightness(Handle handle, float brightness) override;
	virtual void setShadow(Handle handle, bool hasShadow) override;
	virtual void setRadius(Handle handle, float radius) override;
	virtual void update(uint32_t frame) override;
	virtual uint32_t getMaxPointLights() override;
	virtual uint32_t getMaxPointShadows() override;

	// Inherited via ALightCaster
	virtual void cast(VkCommandBuffer cmd, const std::vector<Drawable*>& drawables) override;
};
} // namespace blaze
