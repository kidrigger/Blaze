
#pragma once

#include <Datatypes.hpp>
#include <Drawable.hpp>
#include <LightSystem.hpp>
#include <core/Context.hpp>
#include <util/Managed.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
const float PI = 3.1415926535897932384626433f;
using RenderCommand = std::function<void(VkCommandBuffer buf, VkPipelineLayout layout, uint32_t frameCount)>;

class Renderer
{
public:

	virtual void submit(Drawable* drawable) = 0;
	virtual void renderFrame() = 0;

	virtual void set_environmentDescriptor(VkDescriptorSet envDS) = 0;
	virtual void set_skyboxCommand(const RenderCommand& cmd) = 0;
	virtual void set_cameraUBO(const CameraUniformBufferObject& ubo) = 0;
	virtual void set_camera(Camera* cam) = 0;
	virtual void set_lightUBO(const LightsUniformBufferObject& ubo) = 0;
	virtual void set_settingsUBO(const SettingsUniformBufferObject& ubo) = 0;

	virtual const VkDescriptorSetLayout& get_materialLayout() const = 0;
	virtual const VkDescriptorSetLayout& get_environmentLayout() const = 0;
	virtual LightSystem& get_lightSystem() = 0;

	// Context forwarding
	virtual VkDevice get_device() const = 0;
	virtual const Context& get_context() const = 0;

	virtual bool complete() const = 0;
};

} // namespace blaze