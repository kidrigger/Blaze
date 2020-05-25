
#pragma once

#include <core/Drawable.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
class ALightCaster
{
public:
	using Handle = uint32_t;

	enum class Type : uint8_t
	{
		INVALID = 0,
		POINT = 1,
		DIRECTIONAL = 2,
		SPOT = 3,
	};

	virtual Handle createPointLight(const glm::vec3& position, float brightness, float radius, bool enableShadow) = 0;
	virtual Handle createDirectionLight(const glm::vec3& direction, float brightness, uint32_t numCascades) = 0;
	virtual void removeLight(Handle handle) = 0;

	virtual uint32_t getMaxPointLights() = 0;
	virtual uint32_t getMaxPointShadows() = 0;
	virtual uint32_t getMaxDirectionLights() = 0;
	virtual uint32_t getMaxDirectionShadows() = 0;

	virtual void setPosition(Handle handle, const glm::vec3& position) = 0;
	virtual void setDirection(Handle handle, const glm::vec3& direction) = 0;
	virtual void setRadius(Handle handle, float radius) = 0;
	virtual void setBrightness(Handle handle, float brightness) = 0;
	virtual bool setShadow(Handle handle, bool hasShadow) = 0;

	virtual void update(uint32_t frame) = 0;
	virtual void cast(VkCommandBuffer cmd, const std::vector<Drawable*>& drawables) = 0;
};
}
