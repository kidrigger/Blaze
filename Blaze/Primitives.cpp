
#include "Primitives.hpp"

namespace blaze
{
	IndexedVertexBuffer<Vertex> getUVCube(const Context& context)
	{
		// Hello vertex, my old code
		std::vector<Vertex> vertices = {
			{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.2f}, {1.0f, 1.0f}},
			{{0.5f, -0.5f, -0.5f}, {1.0f, 0.2f, 0.2f}, {1.0f, 0.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.2f, 0.2f, 0.2f}, {0.0f, 0.0f}},
			{{-0.5f, 0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}, {0.0f, 1.0f}},
			{{-0.5f, -0.5f, 0.5f}, {0.2f, 0.2f, 1.0f}, {0.0f, 0.0f}},
			{{0.5f, -0.5f, 0.5f}, {1.0f, 0.2f, 1.0f}, {1.0f, 0.0f}},
			{{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.5f}, {0.2f, 1.0f, 1.0f}, {0.0f, 1.0f}}
		};
		std::vector<uint32_t> indices = {
			0, 1, 2,
			0, 2, 3,
			4, 5, 6,
			4, 6, 7,
			1, 0, 6,
			1, 6, 5,
			7, 6, 0,
			7, 0, 3,
			7, 3, 2,
			7, 2, 4,
			4, 2, 1,
			4, 1, 5
		};
		return IndexedVertexBuffer(context, vertices, indices);
	}

	IndexedVertexBuffer<Vertex> getUVRect(const Context& context)
	{
		// Hello vertex, my old code
		std::vector<Vertex> vertices = {
			{{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.2f}, {1.0f, 1.0f}},
			{{1.0f, -1.0f, 0.0f}, {1.0f, 0.2f, 0.2f}, {1.0f, 0.0f}},
			{{-1.0f, -1.0f, 0.0f}, {0.2f, 0.2f, 0.2f}, {0.0f, 0.0f}},
			{{-1.0f, 1.0f, 0.0f}, {0.2f, 1.0f, 0.2f}, {0.0f, 1.0f}}
		};
		std::vector<uint32_t> indices = {
			0, 1, 2,
			0, 2, 3,
		};
		return IndexedVertexBuffer(context, vertices, indices);
	}
}
