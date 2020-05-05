
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{

/**
 * @interface Bindable
 *
 * @brief Interface for objects that bind to pipelines.
 */
class Bindable
{
public:
	virtual void bind(VkCommandBuffer buf, VkPipelineLayout lay) const = 0;
};
}
