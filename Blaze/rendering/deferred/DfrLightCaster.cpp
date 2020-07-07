
#include "DfrLightCaster.hpp"

#include <iostream>

#undef max
#undef min

namespace blaze
{
// DfrLightCaster

DfrLightCaster::DfrLightCaster(const Context* context, const spirv::Shader* shader, uint32_t frames) noexcept
{
	auto set = shader->getSetWithUniform("lights");
	auto texSet = shader->getSetWithUniform("shadows");

	dataSet = context->get_pipelineFactory()->createSets(*set, frames);
	textureSet = context->get_pipelineFactory()->createSet(*texSet);
	pointLights = std::make_unique<dfr::PointLightCaster>(context, 16u, dataSet, textureSet);
	// directionLights = std::make_unique<fwd::DirectionLightCaster>(context, dataSet, textureSet);
}

void DfrLightCaster::recreate(const Context* context, const spirv::Shader* shader, uint32_t frames)
{
	auto set = shader->getSetWithUniform("lights");
	dataSet = context->get_pipelineFactory()->createSets(*set, frames);
	pointLights->recreate(context, dataSet);
	// directionLights->recreate(context, dataSet);
}

void DfrLightCaster::bind(VkCommandBuffer buf, VkPipelineLayout lay, uint32_t frame) const
{
	vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, lay, dataSet.setIdx, 1, &dataSet[frame], 0, nullptr);
	vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, lay, textureSet.setIdx, 1, &textureSet[frame], 0,
							nullptr);
}

dfr::PointLightCaster::LightIterator DfrLightCaster::getPointLightIterator()
{
	return pointLights->getLightIterator();
}

DfrLightCaster::Handle DfrLightCaster::createPointLight(const glm::vec3& position, float brightness, float radius,
														bool enableShadow)
{
	uint16_t idx = pointLights->createLight(position, brightness, radius, enableShadow);
	if (idx == UINT16_MAX)
	{
		return 0;
	}

	HandleExposed exposed = {
		static_cast<uint8_t>(Type::POINT),
		pointGeneration,
		idx,
	};

	pointGeneration = pointGeneration + 1;

	Handle handle = reinterpret_cast<Handle&>(exposed);
	validHandles.insert(handle);

	return handle;
}

void DfrLightCaster::setPosition(Handle handle, const glm::vec3& position)
{
	HandleExposed exposed = reinterpret_cast<HandleExposed&>(handle);
	Type type = static_cast<Type>(exposed.type);
	switch (type)
	{
	case Type::POINT: {
		pointLights->getLight(exposed.idx)->position = position;
	};
	break;
	case Type::DIRECTIONAL: {
		throw std::invalid_argument("Can't set position of directional light");
	}
	break;
	default:
		throw std::invalid_argument("Unimplemented");
	}
}

void DfrLightCaster::setDirection(Handle handle, const glm::vec3& direction)
{
	HandleExposed exposed = reinterpret_cast<HandleExposed&>(handle);
	Type type = static_cast<Type>(exposed.type);
	switch (type)
	{
	case Type::POINT: {
		throw std::invalid_argument("Can't set direction of point light");
	};
	break;
	case Type::DIRECTIONAL: {
		// directionLights->getLight(exposed.idx)->direction = glm::normalize(direction);
	}
	break;
	default:
		throw std::invalid_argument("Unimplemented");
	}
}

void DfrLightCaster::setBrightness(Handle handle, float brightness)
{
	assert(brightness >= 0.0f);

	HandleExposed exposed = reinterpret_cast<HandleExposed&>(handle);
	Type type = static_cast<Type>(exposed.type);
	switch (type)
	{
	case Type::POINT: {
		pointLights->getLight(exposed.idx)->brightness = brightness;
	};
	break;
	case Type::DIRECTIONAL: {
		// directionLights->getLight(exposed.idx)->brightness = brightness;
	}
	break;
	default:
		throw std::invalid_argument("Unimplemented");
	}
}

bool DfrLightCaster::setShadow(Handle handle, bool hasShadow)
{
	HandleExposed exposed = reinterpret_cast<HandleExposed&>(handle);
	Type type = static_cast<Type>(exposed.type);
	switch (type)
	{
	case Type::POINT: {
		return pointLights->setShadow(exposed.idx, hasShadow);
	};
	break;
	case Type::DIRECTIONAL: {
		// return directionLights->setShadow(exposed.idx, hasShadow);
	}
	break;
	default:
		throw std::invalid_argument("Unimplemented");
	}
}

void DfrLightCaster::setRadius(Handle handle, float radius)
{
	HandleExposed exposed = reinterpret_cast<HandleExposed&>(handle);
	Type type = static_cast<Type>(exposed.type);
	switch (type)
	{
	case Type::POINT: {
		pointLights->getLight(exposed.idx)->radius = radius;
	};
	break;
	case Type::DIRECTIONAL: {
		throw std::invalid_argument("Can't set radius of directional light");
	}
	break;
	default:
		throw std::invalid_argument("Unimplemented");
	}
}

void DfrLightCaster::removeLight(Handle handle)
{
	if (validHandles.find(handle) == validHandles.end())
	{
		return;
	}

	HandleExposed exposed = reinterpret_cast<HandleExposed&>(handle);
	uint32_t idx = static_cast<uint32_t>(exposed.idx);
	Type type = static_cast<Type>(exposed.type);
	switch (type)
	{
	case Type::POINT:
		pointLights->removeLight(idx);
		break;
	case Type::DIRECTIONAL:
		// directionLights->removeLight(idx);
		break;
	default:
		throw std::invalid_argument("Only point lights are supported so far");
	}

	validHandles.erase(handle);
}

void DfrLightCaster::update(const Camera* camera, uint32_t frame)
{
	pointLights->update(frame);
	// directionLights->update(camera, frame);
}

uint32_t DfrLightCaster::getMaxPointLights()
{
	return pointLights->getMaxLights();
}

uint32_t DfrLightCaster::getMaxPointShadows()
{
	return pointLights->getMaxShadows();
}

void DfrLightCaster::cast(VkCommandBuffer cmd, const std::vector<Drawable*>& drawables)
{
	pointLights->cast(cmd, drawables);
	// directionLights->cast(cmd, drawables);
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