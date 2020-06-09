
#pragma once

#include <Datatypes.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>

namespace blaze
{
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
	bool isAlphaBlending;

	/**
	 * @brief Constructor
	 */
	Primitive(uint32_t firstIndex, uint32_t vertexCount, uint32_t indexCount, uint32_t material, bool blendAlpha)
		: firstIndex(firstIndex), vertexCount(vertexCount), indexCount(indexCount), material(material),
		  hasIndex(indexCount > 0), isAlphaBlending(blendAlpha)
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
	glm::mat4 pcb;
	std::vector<int> children;

	std::pair<int, int> primitive_range;
	int numOpaque;

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
		 const std::pair<int, int> primitive_range, int numOpaque) noexcept;

	/**
	 * @fn 	Node(const glm::mat4& TRS, const std::vector<int>& children, const std::pair<int, int> primitive_range)
	 *
	 * @brief Constructor that takes the entire TRS matrix.
	 *
	 * @param TRS Translation Rotation Scale matrix
	 * @param children Indices of the children of the node in the Model.
	 * @param primitive_range The range of indices of primitives under this Node.
	 */
	Node(const glm::mat4& TRS, const std::vector<int>& children, const std::pair<int, int> primitive_range,
		 int numOpaque) noexcept;

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
} // namespace blaze
