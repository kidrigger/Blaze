
#pragma once

#include "Datatypes.hpp"
#include "Renderer.hpp"
#include "Texture2D.hpp"
#include "VertexBuffer.hpp"
#include "util/Managed.hpp"
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Drawable.hpp"

namespace blaze
{
/**
 * @class Material
 *
 * @brief Collection of the material textures and constants and descriptor.
 *
 * Holds the data for material representing the material from glTF 2.0 and
 * holds a descriptor used to bind the entire material at once.
 *
 * A push constant block in the material is used to push indices and constant values
 * such as multipliers and factors.
 */
class Material
{
	Texture2D diffuse;
	Texture2D metallicRoughness;
	Texture2D normal;
	Texture2D occlusion;
	Texture2D emissive;
	MaterialPushConstantBlock pushConstantBlock;
	util::Managed<VkDescriptorSet> descriptorSet;

public:
	/**
	 * @brief Constructor for Material class.
	 *
	 * All the input textures are \b Moved into the class.
	 *
	 * @param pushBlock The MaterialPushConstantBlock required.
	 * @param diff The Albedo Map.
	 * @param norm The Normal Map.
	 * @param metal The PhysicalDescription Map (Metallic/Roughness).
	 * @param ao The Ambient Occlusion Map.
	 * @param em The Emissive Map.
	 */
	Material(MaterialPushConstantBlock pushBlock, Texture2D&& diff, Texture2D&& norm, Texture2D&& metal, Texture2D&& ao,
			 Texture2D&& em);

	/**
	 * @name Move Constructors.
	 *
	 * @brief Set of move constructors for the class.
	 *
	 * Material is a move only class and hence the copy constructors are deleted.
	 *
	 * @{
	 */
	Material(Material&& other) noexcept;
	Material& operator=(Material&& other) noexcept;
	Material(const Material& other) = delete;
	Material& operator=(const Material& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @fn generateDescriptorSet
	 *
	 * @brief Generates the Descriptor Set for the material textures.
	 *
	 * As the pool is not ready during material loading time,
	 * the Model loader lazily generates the sets after the construction of the pool.
	 *
	 * @param device The logical device in use.
	 * @param layout The descriptor set layout of the material descriptor set.
	 * @param pool The descriptor pool to create the set from.
	 *
	 * @note This step is necessary before use.
	 */
	void generateDescriptorSet(VkDevice device, VkDescriptorSetLayout layout, VkDescriptorPool pool);

	/**
	 * @name Getters
	 *
	 * @brief Getters for the private members.
	 *
	 * @{
	 */
	inline const VkDescriptorSet& get_descriptorSet() const
	{
		return descriptorSet.get();
	}
	inline const MaterialPushConstantBlock& get_pushConstantBlock() const
	{
		return pushConstantBlock;
	}
	/**
	 * @}
	 */
};

/**
 * @struct Primitive
 *
 * @brief Denotes the data of a single primitive.
 *
 * The class is used for a singular mesh in a Node that is constructed out of a
 * set of vertices that are kept separately.
 *
 * Each primitive can have its own vertices and material, or can share with other
 * primitives in the Model
 */
struct Primitive
{
	uint32_t firstIndex;
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t material;
	bool hasIndex;

	/**
	 * @brief Constructor
	 */
	Primitive(uint32_t firstIndex, uint32_t vertexCount, uint32_t indexCount, uint32_t material)
		: firstIndex(firstIndex), vertexCount(vertexCount), indexCount(indexCount), material(material),
		  hasIndex(indexCount > 0)
	{
	}
};

/**
 * @struct Node
 *
 * @brief A node in the Model node tree.
 *
 * Each node contains a model transformation that applies to all primitives in
 * the node, as well as the children of the node.
 *
 * Each node must have atleast one child or one primitive under it.
 */
struct Node
{
	glm::vec3 translation;
	glm::quat rotation;
	glm::vec3 scale;
	glm::mat4 localTRS;
	ModelPushConstantBlock pcb;
	std::vector<int> children;

	std::pair<int, int> primitive_range;

	/**
	 * @fn Node()
	 *
	 * @brief Default constructor.
	 *
	 * The default constructor of the Node. Every value is set to their default.
	 */
	Node() noexcept;

	/**
	 * @fn 	Node(const glm::vec3& trans, const glm::quat& rot, const glm::vec3& sc, const std::vector<int>& children,
	 * const std::pair<int, int> primitive_range)
	 *
	 * @brief Constructor that takes individual transform parameters.
	 *
	 * @param trans Translation
	 * @param rot Rotation
	 * @param sc Scale
	 * @param children Indices of the children of the node in the Model.
	 * @param primitive_range The range of indices of primitives under this Node.
	 */
	Node(const glm::vec3& trans, const glm::quat& rot, const glm::vec3& sc, const std::vector<int>& children,
		 const std::pair<int, int> primitive_range) noexcept;

	/**
	 * @fn 	Node(const glm::mat4& TRS, const std::vector<int>& children, const std::pair<int, int> primitive_range)
	 *
	 * @brief Constructor that takes the entire TRS matrix.
	 *
	 * @param TRS Translation Rotation Scale matrix
	 * @param children Indices of the children of the node in the Model.
	 * @param primitive_range The range of indices of primitives under this Node.
	 */
	Node(const glm::mat4& TRS, const std::vector<int>& children, const std::pair<int, int> primitive_range) noexcept;

	/**
	 * @name Move Constructors
	 *
	 * @brief For moving a Node.
	 *
	 * Nodes are not copyable. Hance copy constructors are deleted.
	 *
	 * @{
	 */
	Node(Node&& other) noexcept;
	Node& operator=(Node&& other) noexcept;
	Node(const Node& other) = delete;
	Node& operator=(const Node& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @fn update
	 *
	 * @brief Updates the World transform of the Node.
	 *
	 * Updates the local transform according to changes in the three components.
	 * Updates the world transform according to local transform and parent transform.
	 *
	 * @param parentTRS Parent transform.
	 */
	void update(const glm::mat4 parentTRS = glm::mat4(1.0f));
};

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
	util::Managed<VkDescriptorPool> descriptorPool;
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
