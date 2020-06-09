
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace blaze
{
/**
 * @interface Drawable
 *
 * @brief Interface for object that can be drawn by the renderer.
 *
 * Renderer contains two kinds of draws:
 * \arg Full: Using material info.
 * \arg Geometry: Only using the vertex position.
 */
class Drawable
{
public:
	/**
	 * @fn draw(VkCommandBuffer cb, VkPipelineLayout lay)
	 *
	 * @brief Should draw the model with all the material and maps included.
	 *
	 * This method is used during the primary render and should bind
	 * all the required buffers and textures and push material PCB.
	 *
	 * @param cb The command buffer to record to.
	 * @param lay The pipeline layout to bind descriptors.
	 */
	virtual void draw(VkCommandBuffer cb, VkPipelineLayout lay) = 0;

	/**
	 * @brief Should draw the model with only the OPAQUE and MASK material and maps.
	 *
	 * This method is used during the primary render and should bind
	 * all the required buffers and textures and push material PCB.
	 *
	 * @param cb The command buffer to record to.
	 * @param lay The pipeline layout to bind descriptors.
	 */
	virtual void drawOpaque(VkCommandBuffer cb, VkPipelineLayout lay) = 0;

	/**
	 * @brief Should draw the model with only the BLEND material and maps.
	 *
	 * This method is used during the primary render and should bind
	 * all the required buffers and textures and push material PCB.
	 *
	 * @param cb The command buffer to record to.
	 * @param lay The pipeline layout to bind descriptors.
	 */
	virtual void drawAlphaBlended(VkCommandBuffer cb, VkPipelineLayout lay) = 0;

	/**
	 * @fn drawGeometry(VkCommandBuffer cb, VkPipelineLayout lay)
	 *
	 * @brief Should draw only the geometry and not bind any materials.
	 *
	 * @note SKIPS TRANSPARENCY
	 *
	 * This method is used for casting shadows and dowsn't require any information
	 * beyond the position attribute of the Vertex and a model transformation PCB.
	 *
	 * @param cb The command buffer to record to.
	 * @param lay The pipeline layout to bind descriptors.
	 */
	virtual void drawGeometry(VkCommandBuffer cb, VkPipelineLayout lay) = 0;
};
} // namespace blaze
