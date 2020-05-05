
#pragma once

#include "Datatypes.hpp"
#include <core/VertexBuffer.hpp>
#include <vector>

namespace blaze
{
/**
 * @fn getUVCube
 *
 * @brief Creates a simple vertex buffer for a Cube
 *
 * @param context the current Vulkan Context
 *
 * @returns IndexedVertexBuffer of Vertex for the Cube
 */
IndexedVertexBuffer<Vertex> getUVCube(const Context* context);

/**
 * @fn getUVRect
 *
 * @brief Creates a simple vertex buffer for a Rect
 *
 * @param context the current Vulkan Context
 *
 * @returns IndexedVertexBuffer of Vertex for the Rect
 */
IndexedVertexBuffer<Vertex> getUVRect(const Context* context);

/**
 * @fn getIcoSphere
 *
 * @brief Creates a simple vertex buffer for an IcoSphere
 *
 * @param context the current Vulkan Context
 *
 * @returns IndexedVertexBuffer of Vertex for the IcoSphere
 */
IndexedVertexBuffer<Vertex> getIcoSphere(const Context* context);
} // namespace blaze
