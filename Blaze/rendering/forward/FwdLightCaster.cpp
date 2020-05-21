
#include "FwdLightCaster.hpp"

#include <iostream>

#undef max
#undef min

namespace blaze
{
// FwdLightCaster

FwdLightCaster::FwdLightCaster(const Context* context, const spirv::Shader* shader, uint32_t frames) noexcept
{
	auto set = shader->getSetWithUniform("lights");
	auto texSet = shader->getSetWithUniform("shadows");

	dataSet = context->get_pipelineFactory()->createSets(*set, frames);
	textureSet = context->get_pipelineFactory()->createSet(*texSet);
	pointLights = std::make_unique<PointLightCaster>(context, dataSet, textureSet);
}

void FwdLightCaster::recreate(const Context* context, const spirv::Shader* shader, uint32_t frames)
{
	auto set = shader->getSetWithUniform("lights");
	dataSet = context->get_pipelineFactory()->createSets(*set, frames);
	pointLights->recreate(context, dataSet);
}

void FwdLightCaster::bind(VkCommandBuffer buf, VkPipelineLayout lay, uint32_t frame) const
{
	vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, lay, dataSet.setIdx, 1, &dataSet[frame], 0, nullptr);
	vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, lay, textureSet.setIdx, 1, &textureSet[frame], 0, nullptr);
}

FwdLightCaster::Handle FwdLightCaster::createPointLight(const glm::vec3& position, float brightness, float radius,
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

	pointGeneration = (pointGeneration + 1) % UINT16_MAX;

	Handle handle = reinterpret_cast<Handle&>(exposed);
	validHandles.insert(handle);

	return handle;
}

void FwdLightCaster::setPosition(Handle handle, const glm::vec3& position)
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

void FwdLightCaster::setDirection(Handle handle, const glm::vec3& direction)
{
	throw std::invalid_argument(std::string(__FUNCTION__) + "Unimplemented");
}

void FwdLightCaster::setBrightness(Handle handle, float brightness)
{
	HandleExposed exposed = reinterpret_cast<HandleExposed&>(handle);
	Type type = static_cast<Type>(exposed.type);
	switch (type)
	{
	case Type::POINT: {
		pointLights->getLight(exposed.idx)->brightness = brightness;
	};
	break;
	default:
		throw std::invalid_argument("Unimplemented");
	}
}

void FwdLightCaster::setShadow(Handle handle, bool hasShadow)
{
	std::cerr << __FUNCTION__ << " is Unimplemented NL: " << pointLights->get_count() << std::endl;
}

void FwdLightCaster::setRadius(Handle handle, float radius)
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

void FwdLightCaster::removeLight(Handle handle)
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
	default:
		throw std::invalid_argument("Only point lights are supported so far");
	}

	validHandles.erase(handle);
}

void FwdLightCaster::update(uint32_t frame)
{
	pointLights->update(frame);
}

uint32_t FwdLightCaster::getMaxPointLights()
{
	return pointLights->getMaxLights();
}

uint32_t FwdLightCaster::getMaxPointShadows()
{
	return pointLights->getMaxShadows();
}
void FwdLightCaster::cast(VkCommandBuffer cmd, const std::vector<Drawable*>& drawables)
{
	pointLights->cast(cmd, drawables);
}
} // namespace blaze