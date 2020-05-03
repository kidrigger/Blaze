
#pragma once

#include "Datatypes.hpp"
#include <core/Drawable.hpp>
#include <core/Texture2D.hpp>
#include <core/VertexBuffer.hpp>
#include <rendering/Renderer.hpp>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Node.hpp"
#include "Material.hpp"

namespace blaze
{
/**
 * @class Model
 *
 * @brief Holds data from an entire glTF 2.0 model.
 *
 * Contains the entire set of Material, Primitive, and Node as well the actual
 * IndexedVertexBuffer and the root of the Node tree.
 */
class Model : public Drawable
{
private:
	Node root;
	vkw::DescriptorPool descriptorPool;
	std::vector<int> prime_nodes;
	std::vector<Node> nodes;
	std::vector<Primitive> primitives;
	std::vector<Material> materials;
	IndexedVertexBuffer<Vertex> vbo;

public:
	/**
	 * @fn Model()
	 *
	 * @brief Default constructor.
	 *
	 */
	Model() noexcept
	{
	}

	/**
	 * @fn Model(const Renderer& renderer, const std::vector<int>& top_level_nodes, std::vector<Node>& nodes,
	 * std::vector<Primitive>& prims, std::vector<Material>& mats, IndexedVertexBuffer<Vertex>&& ivb)
	 *
	 * @brief Full constructor.
	 *
	 * @param renderer The renderer used for the material.
	 * @param top_level_nodes The indices of nodes that are at the top level and have no parents.
	 * @param nodes The list of nodes in the model.
	 * @param prims The list of primitives in the model.
	 * @param mats The list of materials in the model.
	 * @param ivb The IndexedVertexBuffer that contains \b all the vertices and indices.
	 */
	Model(const Renderer& renderer, const std::vector<int>& top_level_nodes, std::vector<Node>& nodes,
		  std::vector<Primitive>& prims, std::vector<Material>& mats, IndexedVertexBuffer<Vertex>&& ivb) noexcept;

	/**
	 * @name Move Constructors.
	 *
	 * @brief For moving Models.
	 *
	 * Model is a non copyable class. Copy constructors are deleted.
	 *
	 * @{
	 */
	Model(Model&& other) noexcept;
	Model& operator=(Model&& other) noexcept;
	Model(const Model& other) = delete;
	Model& operator=(const Model& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @fn update()
	 *
	 * @brief Update the transformation of the model starting from the root node.
	 */
	void update();

	/**
	 * @name Drawable Implementation
	 *
	 * @brief The implementation of the draw method from Drawable
	 *
	 * @{
	 */
	void draw(VkCommandBuffer buf, VkPipelineLayout layout);
	void drawGeometry(VkCommandBuffer buf, VkPipelineLayout layout);
	/**
	 * @}
	 */

	/**
	 * @name Getters
	 *
	 * @brief Getters for the private members.
	 *
	 * @{
	 */
	Node* get_root()
	{
		return &root;
	}
	uint32_t get_vertexCount() const
	{
		return vbo.get_vertexCount();
	}
	uint32_t get_indexCount() const
	{
		return vbo.get_indexCount();
	}
	/**
	 * @}
	 */

private:
	void update_nodes(int node, int parent = -1);
};

/**
 * @fn loadModel
 *
 * @brief Loads a glTF 2.0 model from the path provided.
 *
 * The model is not cached and additional loads will create additional copies.
 *
 * @param renderer The renderer used for the model.
 * @param name The path of the file from the working directory.
 */
[[nodiscard]] Model loadModel(const Renderer& renderer, const std::string& name);
} // namespace blaze
