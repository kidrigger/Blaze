
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vkwrap/VkWrap.hpp>

#include "Pipeline.hpp"

namespace blaze::spirv
{
struct DescriptorSet
{
	VkDescriptorSet set;
	Shader::Set::FormatID formatID;
};
} // namespace blaze::spirv
