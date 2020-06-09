
#pragma once

#include "Node.hpp"
#include <core/Drawable.hpp>
#include <core/Texture2D.hpp>
#include <core/VertexBuffer.hpp>
#include <vector>
#include <vkwrap/VkWrap.hpp>

#include <spirv/PipelineFactory.hpp>

namespace blaze
{
class Model : public Drawable
{
public:
	struct Material
	{
		enum AlphaMode : int
		{
			ALPHA_OPAQUE = 0,
			ALPHA_MASK = 1,
			ALPHA_BLEND = 2,
		};

		struct PCB
		{
			glm::vec4 baseColorFactor{1.0f, 0, 1.0f, 1.0f};		// 16	| 16
			glm::vec4 emissiveColorFactor{1.0f, 0, 1.0f, 1.0f}; // 16	| 32
			float metallicFactor{1.0f};							// 4	| 36
			float roughnessFactor{1.0f};						// 4	| 40
			int baseColorTextureSet{-1};						// 4	| 44
			int physicalDescriptorTextureSet{-1};				// 4	| 48
			int normalTextureSet{-1};							// 4	| 52
			int occlusionTextureSet{-1};						// 4	| 56
			int emissiveTextureSet{-1};							// 4	| 60
			int textureArrIdx{0};								// 4	| 64
			int alphaMode{ALPHA_OPAQUE};						// 4	| 68
			float alphaCutoff{0.5f};							// 4	| 72
		};

		std::vector<Texture2D> diffuse;
		std::vector<Texture2D> metalRough;
		std::vector<Texture2D> normal;
		std::vector<Texture2D> occlusion;
		std::vector<Texture2D> emission;

		std::vector<PCB> pushConstantBlocks;

		spirv::SetSingleton dset;
	};

private:
	Node root;
	std::vector<int> prime_nodes;
	std::vector<Node> nodes;
	std::vector<Primitive> primitives;
	Material material;
	IndexedVertexBuffer<Vertex> vbo;

public:
	/**
	 * @brief Default constructor.
	 */
	Model() noexcept
	{
	}

	/**
	 * @brief Full constructor.
	 *
	 * @param top_level_nodes The indices of nodes that are at the top level and have no parents.
	 * @param nodes The list of nodes in the model.
	 * @param prims The list of primitives in the model.
	 * @param ivb The IndexedVertexBuffer that contains \b all the vertices and indices.
	 * @param mat The material used in the model.
	 */
	Model(const std::vector<int>& top_level_nodes, std::vector<Node>&& nodes, std::vector<Primitive>&& prims,
		   IndexedVertexBuffer<Vertex>&& ivb, Material&& mat) noexcept;

	/**
	 * @name Move Constructors.
	 *
	 * @brief For moving Models.
	 *
	 * Model is a non copyable class. Copy constructors are deleted.
	 *
	 * @{
	 */
	Model(Model&& other) = delete;
	Model& operator=(Model&& other) = delete;
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
	virtual void draw(VkCommandBuffer buf, VkPipelineLayout layout) override;
	virtual void drawGeometry(VkCommandBuffer buf, VkPipelineLayout layout) override;
	virtual void drawOpaque(VkCommandBuffer cb, VkPipelineLayout lay) override;
	virtual void drawAlphaBlended(VkCommandBuffer cb, VkPipelineLayout lay) override;
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
} // namespace blaze
