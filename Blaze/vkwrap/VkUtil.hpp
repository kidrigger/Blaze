
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{

inline VkViewport createViewport(VkExtent2D extent)
{
	float h = static_cast<float>(extent.height);
	float w = static_cast<float>(extent.width);

	VkViewport viewport = {};
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0.0f;
	viewport.y = h;
	viewport.width = w;
	viewport.height = -h;

	return viewport;
}

inline VkRect2D createScissor(VkExtent2D extent)
{
	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = extent;

	return scissor;
}

inline std::tuple<VkViewport, VkRect2D> createViewportScissor(VkExtent2D extent)
{

	return std::make_tuple(createViewport(extent), createScissor(extent));
}
} // namespace blaze