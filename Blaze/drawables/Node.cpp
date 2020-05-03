
#include "Node.hpp"

namespace blaze
{

Node::Node() noexcept
	: translation(0.0f), rotation(1.0f, 0.0f, 0.0f, 0.0f), scale(1.0f), primitive_range(0, 0), localTRS(1.0f), pcb{}
{
}

Node::Node(const glm::vec3& trans, const glm::quat& rot, const glm::vec3& sc, const std::vector<int>& children,
		   const std::pair<int, int> primitive_range) noexcept
	: translation(trans), rotation(rot), scale(sc), children(children), primitive_range(primitive_range), pcb{}
{
	localTRS =
		glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

Node::Node(const glm::mat4& TRS, const std::vector<int>& children, const std::pair<int, int> primitive_range) noexcept
	: localTRS(TRS), children(children), primitive_range(primitive_range), pcb{}
{
	glm::vec3 skew;
	glm::vec4 pers;
	glm::decompose(TRS, scale, rotation, translation, skew, pers);
}

Node::Node(Node&& other) noexcept
	: translation(other.translation), rotation(other.rotation), scale(other.scale), localTRS(other.localTRS),
	  pcb(other.pcb), children(std::move(other.children)), primitive_range(std::move(other.primitive_range))
{
}

Node& Node::operator=(Node&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	translation = other.translation;
	rotation = other.rotation;
	scale = other.scale;
	localTRS = other.localTRS;
	pcb = other.pcb;
	children = std::move(other.children);
	primitive_range = std::move(other.primitive_range);
	return *this;
}

void Node::update(const glm::mat4 parentTRS)
{
	localTRS =
		glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
	pcb.model = parentTRS * localTRS;
}
} // namespace blaze
