
#pragma once

#include "VertexBuffer.hpp"
#include "Datatypes.hpp"
#include <vector>

namespace blaze
{
	IndexedVertexBuffer<Vertex> getUVCube(const Context& context);
	IndexedVertexBuffer<Vertex> getUVRect(const Context& context);
}