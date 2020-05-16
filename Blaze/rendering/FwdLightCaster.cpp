
#include "FwdLightCaster.hpp"

#include <iostream>

#undef max
#undef min

namespace blaze
{
PointLightCaster::PointLightCaster(const Context* context, const spirv::SetVector& sets,
								   uint32_t maxPointLights) noexcept
	: lights(maxPointLights), ubos(context, maxPointLights * sizeof(LightData), sets.size()), freeLight(0), count(0)
{
	int i = -1;
	for (auto& light : lights)
	{
		light.position = glm::vec3(0);
		light.brightness = static_cast<float>(i);
		light.shadowIdx = -1;
		i--;
	}
	i = std::numeric_limits<int>::min();
	lights.back().brightness = static_cast<int>(i);

	bindDataSet(context, sets);
}

void PointLightCaster::recreate(const Context* context, const spirv::SetVector& sets)
{
	ubos = UBODataVector(context, lights.size() * sizeof(LightData), sets.size());

	for (int i = 0; i < sets.size(); ++i)
	{
		update(i);
	}

	bindDataSet(context, sets);
}

void PointLightCaster::update(uint32_t frame)
{
	ubos[frame].writeData(lights.data(), lights.size() * sizeof(LightData));
}

uint16_t PointLightCaster::createLight(const glm::vec3& position, float brightness, bool enableShadow)
{
	if (freeLight < 0)
	{
		return (uint16_t)-1;
	}

	LightData* pLight = &lights[freeLight];
	uint16_t idx = static_cast<uint16_t>(freeLight);

	freeLight = -static_cast<int>(pLight->brightness);

	pLight->position = position;
	pLight->brightness = brightness;
	pLight->shadowIdx = -1;
	// Shadow TODO

	count++;

	return idx;
}

void PointLightCaster::removeLight(uint16_t idx)
{
	assert(idx < lights.size());

	LightData* pLight = &lights[idx];

	assert(pLight->brightness > 0);

	pLight->brightness = -static_cast<float>(freeLight);
	pLight->position = glm::vec3(0.0f);
	pLight->shadowIdx = -1;

	count--;
	freeLight = idx;
}

void PointLightCaster::bindDataSet(const Context* context, const spirv::SetVector& sets)
{
	const spirv::UniformInfo* unif = nullptr;
	for (auto& uniform : sets.info)
	{
		if (uniform.name == "lights")
		{
			unif = &uniform;
			break;
		}
	}
	assert(unif);

	for (uint32_t i = 0; i < sets.size(); i++)
	{
		VkDescriptorBufferInfo info = ubos[i].get_descriptorInfo();

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = unif->type;
		write.descriptorCount = unif->arrayLength;
		write.dstSet = sets[i];
		write.dstBinding = unif->binding;
		write.dstArrayElement = 0;
		write.pBufferInfo = &info;

		vkUpdateDescriptorSets(context->get_device(), 1, &write, 0, nullptr);
	}
}

// FwdLightCaster

FwdLightCaster::FwdLightCaster(const Context* context, spirv::SetVector&& dataSet) noexcept
	: pointLights(context, dataSet, MAX_POINT_LIGHTS)
{
	this->dataSet = std::move(dataSet);
}

void FwdLightCaster::recreate(const Context* context, spirv::SetVector&& dataSet)
{
	this->dataSet = std::move(dataSet);
	pointLights.recreate(context, this->dataSet);
}

void FwdLightCaster::bind(VkCommandBuffer buf, VkPipelineLayout lay, uint32_t frame) const
{
	vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, lay, dataSet.setIdx, 1, &dataSet[frame], 0, nullptr);
}

FwdLightCaster::Handle FwdLightCaster::createPointLight(const glm::vec3& position, float brightness, bool enableShadow)
{
	uint16_t idx = pointLights.createLight(position, brightness, enableShadow);
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
		pointLights.getLight(exposed.idx)->position = position;
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
		pointLights.getLight(exposed.idx)->brightness = brightness;
	};
	break;
	default:
		throw std::invalid_argument("Unimplemented");
	}
}

void FwdLightCaster::setShadow(Handle handle, bool hasShadow)
{
	std::cerr << __FUNCTION__ << " is Unimplemented NL: " << pointLights.get_count() << std::endl;
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
		pointLights.removeLight(idx);
		break;
	default:
		throw std::invalid_argument("Only point lights are supported so far");
	}

	validHandles.erase(handle);
}

void FwdLightCaster::update(uint32_t frame)
{
	pointLights.update(frame);
}


} // namespace blaze