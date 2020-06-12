
#include "DfrLightCaster.hpp"

#include <iostream>

#undef max
#undef min

namespace blaze
{
// DfrLightCaster

DfrLightCaster::DfrLightCaster() noexcept
{
}

void DfrLightCaster::recreate()
{
}

void DfrLightCaster::bind(VkCommandBuffer buf, VkPipelineLayout lay, uint32_t frame) const
{
}

DfrLightCaster::Handle DfrLightCaster::createPointLight(const glm::vec3& position, float brightness, float radius,
														bool enableShadow)
{
	return 0;
}

void DfrLightCaster::setPosition(Handle handle, const glm::vec3& position)
{
}

void DfrLightCaster::setDirection(Handle handle, const glm::vec3& direction)
{
}

void DfrLightCaster::setBrightness(Handle handle, float brightness)
{
}

bool DfrLightCaster::setShadow(Handle handle, bool hasShadow)
{
	return false;
}

void DfrLightCaster::setRadius(Handle handle, float radius)
{
}

void DfrLightCaster::removeLight(Handle handle)
{
}

void DfrLightCaster::update(const Camera* camera, uint32_t frame)
{
}

uint32_t DfrLightCaster::getMaxPointLights()
{
	return 0;
}

uint32_t DfrLightCaster::getMaxPointShadows()
{
	return 0;
}

void DfrLightCaster::cast(VkCommandBuffer cmd, const std::vector<Drawable*>& drawables)
{
}


DfrLightCaster::Handle DfrLightCaster::createDirectionLight(const glm::vec3& direction, float brightness,
															uint32_t numCascades)
{
	return 0;
}

uint32_t DfrLightCaster::getMaxDirectionLights()
{
	return 0;
}

uint32_t DfrLightCaster::getMaxDirectionShadows()
{
	return 0;
}
} // namespace blaze