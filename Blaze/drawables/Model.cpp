
#include "Model.hpp"

namespace blaze
{
// Model

Model::Model(const Renderer& renderer, const std::vector<int>& top_level_nodes, std::vector<Node>& nodes,
			 std::vector<Primitive>& prims, std::vector<Material>& mats, IndexedVertexBuffer<Vertex>&& ivb) noexcept
	: prime_nodes(top_level_nodes), nodes(std::move(nodes)), primitives(std::move(prims)), materials(std::move(mats)),
	  vbo(std::move(ivb)), root(glm::mat4(1.0f), top_level_nodes, std::make_pair<int, int>(0, 0))
{
	using namespace util;

	VkDevice device = renderer.get_device();
	VkDescriptorSetLayout layout = renderer.get_materialLayout();

	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;
	std::vector<VkDescriptorPoolSize> poolSizes = {poolSize};
	descriptorPool =
		vkw::DescriptorPool(createDescriptorPool(device, poolSizes, static_cast<uint32_t>(materials.size())), device);

	for (auto& material : materials)
	{
		material.generateDescriptorSet(device, layout, descriptorPool.get());
	}
}

Model::Model(Model&& other) noexcept
	: root(std::move(other.root)), primitives(std::move(other.primitives)), prime_nodes(std::move(other.prime_nodes)),
	  nodes(std::move(other.nodes)), materials(std::move(other.materials)), vbo(std::move(other.vbo)),
	  descriptorPool(std::move(other.descriptorPool))
{
}

Model& Model::operator=(Model&& other) noexcept
{
	if (this == &other)
	{
		return *this;
	}
	root = std::move(other.root);
	primitives = std::move(other.primitives);
	prime_nodes = std::move(other.prime_nodes);
	nodes = std::move(other.nodes);
	materials = std::move(other.materials);
	vbo = std::move(other.vbo);
	descriptorPool = std::move(other.descriptorPool);
	return *this;
}

void Model::update()
{
	root.update();
	for (int i : prime_nodes)
	{
		update_nodes(i);
	}
}

void Model::draw(VkCommandBuffer buf, VkPipelineLayout layout)
{
	vbo.bind(buf);
	for (auto& node : nodes)
	{
		vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelPushConstantBlock), &node.pcb);
		for (int i = node.primitive_range.first; i < node.primitive_range.second; i++)
		{
			auto& primitive = primitives[i];
			vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1,
									&materials[primitive.material].get_descriptorSet(), 0, nullptr);
			vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(ModelPushConstantBlock),
							   sizeof(MaterialPushConstantBlock),
							   &materials[primitive.material].get_pushConstantBlock());
			vkCmdDrawIndexed(buf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
		}
	}
}

void Model::drawGeometry(VkCommandBuffer buf, VkPipelineLayout layout)
{
	vbo.bind(buf);
	for (auto& node : nodes)
	{
		vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelPushConstantBlock), &node.pcb);
		for (int i = node.primitive_range.first; i < node.primitive_range.second; i++)
		{
			auto& primitive = primitives[i];
			vkCmdDrawIndexed(buf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
		}
	}
}

void Model::update_nodes(int node, int parent)
{
	if (parent == -1)
	{
		nodes[node].update(root.pcb.model);
	}
	else
	{
		nodes[node].update(nodes[parent].pcb.model);
	}
	for (int child : nodes[node].children)
	{
		update_nodes(child, node);
	}
}
} // namespace blaze
