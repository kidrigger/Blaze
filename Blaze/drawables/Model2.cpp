
#include "Model2.hpp"

namespace blaze
{
// Model

Model2::Model2(const std::vector<int>& top_level_nodes, std::vector<Node>&& nodes, std::vector<Primitive>&& prims,
			   IndexedVertexBuffer<Vertex>&& ivb, Material&& mat) noexcept
	: prime_nodes(top_level_nodes), nodes(std::move(nodes)), primitives(std::move(prims)), vbo(std::move(ivb)),
	  root(glm::mat4(1.0f), top_level_nodes, std::make_pair<int, int>(0, 0)), material(std::move(mat))
{
	using namespace util;
}

void Model2::update()
{
	root.update();
	for (int i : prime_nodes)
	{
		update_nodes(i);
	}
}

void Model2::draw(VkCommandBuffer buf, VkPipelineLayout layout)
{
	vbo.bind(buf);

	vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, material.dset.setIdx,
							static_cast<uint32_t>(material.dset.size()), &material.dset.get(), 0, nullptr);
	for (auto& node : nodes)
	{
		vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						   sizeof(ModelPushConstantBlock), &node.pcb);
		for (int i = node.primitive_range.first; i < node.primitive_range.second; i++)
		{
			auto& primitive = primitives[i];
			vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							   sizeof(ModelPushConstantBlock), sizeof(Material::PCB),
							   &material.pushConstantBlocks[primitive.material]);
			vkCmdDrawIndexed(buf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
		}
	}
}

void Model2::drawGeometry(VkCommandBuffer buf, VkPipelineLayout layout)
{
	vbo.bind(buf);
	for (auto& node : nodes)
	{
		vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
						   sizeof(ModelPushConstantBlock), &node.pcb);
		for (int i = node.primitive_range.first; i < node.primitive_range.second; i++)
		{
			auto& primitive = primitives[i];
			vkCmdDrawIndexed(buf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
		}
	}
}

void Model2::update_nodes(int node, int parent)
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
