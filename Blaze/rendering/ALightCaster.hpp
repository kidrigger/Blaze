
#pragma once

#include <glm/glm.hpp>

namespace blaze
{
class ALightCaster
{
public:
	using Handle = uint32_t;

	enum class Type : uint8_t
	{
		POINT,
		DIRECTIONAL,
		SPOT,
	};

	virtual Handle createPointLight(const glm::vec3& position, float brightness, bool enableShadow) = 0;
	virtual void removeLight(Handle handle) = 0;

	virtual void setPosition(Handle handle, const glm::vec3& position) = 0;
	virtual void setDirection(Handle handle, const glm::vec3& direction) = 0;
	virtual void setBrightness(Handle handle, float brightness) = 0;
	virtual void setShadow(Handle handle, bool hasShadow) = 0;

	virtual void update(uint32_t frame) = 0;
};
}
