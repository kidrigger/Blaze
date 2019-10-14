
#pragma once

#include <vector>
#include "DataTypes.hpp"
#include "VertexBuffer.hpp"
#include "Texture.hpp"
#include "util/Managed.hpp"
#include "Renderer.hpp"

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace blaze
{
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
		Material(MaterialPushConstantBlock pushBlock, Texture2D&& diff, Texture2D&& norm, Texture2D&& metal, Texture2D&& ao, Texture2D&& em)
			: pushConstantBlock(pushBlock),
			diffuse(std::move(diff)),
			metallicRoughness(std::move(metal)),
			normal(std::move(norm)),
			occlusion(std::move(ao)),
			emissive(std::move(em))
		{
		}

		Material(Material&& other) noexcept
			: pushConstantBlock(other.pushConstantBlock),
			diffuse(std::move(other.diffuse)),
			metallicRoughness(std::move(other.metallicRoughness)),
			normal(std::move(other.normal)),
			occlusion(std::move(other.occlusion)),
			emissive(std::move(other.emissive)),
			descriptorSet(std::move(other.descriptorSet))
		{
		}

		Material& operator=(Material&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			pushConstantBlock = std::move(other.pushConstantBlock);
			diffuse = std::move(other.diffuse);
			metallicRoughness = std::move(other.metallicRoughness);
			normal = std::move(other.normal);
			occlusion = std::move(other.occlusion);
			emissive = std::move(other.emissive);
			descriptorSet = std::move(other.descriptorSet);
			return *this;
		}

		void generateDescriptorSet(VkDevice device, VkDescriptorSetLayout layout, VkDescriptorPool pool)
		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = pool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &layout;

			VkDescriptorSet newDescriptorSet;
			auto result = vkAllocateDescriptorSets(device, &allocInfo, &newDescriptorSet);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
			}

			std::array<VkDescriptorImageInfo, 5> imageInfos = {
				diffuse.get_imageInfo(),
				metallicRoughness.get_imageInfo(),
				normal.get_imageInfo(),
				occlusion.get_imageInfo(),
				emissive.get_imageInfo()
			};

			std::array<VkWriteDescriptorSet, 5> writes{};
			
			int idx = 0;
			for(VkWriteDescriptorSet& write : writes)
			{
				write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				write.descriptorCount = 1;
				write.dstSet = newDescriptorSet;
				write.dstBinding = idx;
				write.dstArrayElement = 0;
				write.pImageInfo = &imageInfos[idx];

				idx++;
			}

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

			descriptorSet = util::Managed(newDescriptorSet, [device, pool](VkDescriptorSet& dset) { vkFreeDescriptorSets(device, pool, 1, &dset); });
		}

		Material(const Material& other) = delete;
		Material& operator=(const Material& other) = delete;

		const VkDescriptorSet& get_descriptorSet() const { return descriptorSet.get(); }
		const MaterialPushConstantBlock& get_pushConstantBlock() const { return pushConstantBlock; }
	};

	struct Primitive
	{
		uint32_t firstIndex;
		uint32_t vertexCount;
		uint32_t indexCount;
		uint32_t material;
		bool hasIndex;

		Primitive(uint32_t firstIndex, uint32_t vertexCount, uint32_t indexCount, uint32_t material)
			: firstIndex(firstIndex), vertexCount(vertexCount), indexCount(indexCount), material(material), hasIndex(indexCount > 0)
		{
		}
	};

	struct Node
	{
		glm::vec3 translation;
		glm::quat rotation;
		glm::vec3 scale;
		glm::mat4 localTRS;
		ModelPushConstantBlock pcb;
		std::vector<int> children;

		std::pair<int, int> primitive_range;

		Node() :
			translation(0.0f), rotation(1.0f, 0.0f, 0.0f, 0.0f), scale(1.0f), primitive_range(0, 0), localTRS(1.0f)
		{
		}

		Node(const glm::vec3& trans, const glm::quat& rot, const glm::vec3& sc, const std::vector<int>& children, const std::pair<int, int> primitive_range) :
			translation(trans), rotation(rot), scale(sc), children(children), primitive_range(primitive_range)
		{
			localTRS = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
		}

		Node(const glm::mat4& TRS, const std::vector<int>& children, const std::pair<int, int> primitive_range) :
			localTRS(TRS), children(children), primitive_range(primitive_range)
		{
			glm::vec3 skew;
			glm::vec4 pers;
			glm::decompose(TRS, scale, rotation, translation, skew, pers);
		}

		Node(const Node& other) = default;
		Node& operator=(const Node& other) = default;

		void update(const glm::mat4 parentTRS = glm::mat4(1.0f))
		{
			localTRS = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
			pcb.model = parentTRS * localTRS;
		}
	};

	class Model
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
		Model()
		{
		}

		Model(const Renderer& renderer, const std::vector<int>& top_level_nodes, std::vector<Node>& nodes, std::vector<Primitive>& prims, std::vector<Material>& mats, IndexedVertexBuffer<Vertex>&& ivb)
			: prime_nodes(top_level_nodes),
			nodes(std::move(nodes)),
			primitives(std::move(prims)),
			materials(std::move(mats)),
			vbo(std::move(ivb)),
			root(glm::mat4(1.0f), top_level_nodes, std::make_pair<int, int>(0, 0))
		{
			using namespace util;

			VkDevice device = renderer.get_device();
			VkDescriptorSetLayout layout = renderer.get_materialLayout();

			VkDescriptorPoolSize poolSize = {};
			poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			poolSize.descriptorCount = 1;
			std::vector<VkDescriptorPoolSize> poolSizes = { poolSize };
			descriptorPool = Managed(createDescriptorPool(device, poolSizes, static_cast<uint32_t>(materials.size())), [device](VkDescriptorPool& pool) { vkDestroyDescriptorPool(device, pool, nullptr); });
			
			for (auto& material : materials)
			{
				material.generateDescriptorSet(device, layout, descriptorPool.get());
			}
		}

		Model(Model&& other) noexcept
			: primitives(std::move(other.primitives)),
			prime_nodes(std::move(other.prime_nodes)),
			nodes(std::move(other.nodes)),
			materials(std::move(other.materials)),
			vbo(std::move(other.vbo)),
			descriptorPool(std::move(other.descriptorPool))
		{
		}

		Model& operator=(Model&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			primitives = std::move(other.primitives);
			prime_nodes = std::move(other.prime_nodes);
			nodes = std::move(other.nodes);
			materials = std::move(other.materials);
			vbo = std::move(other.vbo);
			descriptorPool = std::move(other.descriptorPool);
			return *this;
		}

		Model(const Model& other) = delete;
		Model& operator=(const Model& other) = delete;

		void update()
		{
			root.update();
			for (int i : prime_nodes)
			{
				update_nodes(i);
			}
		}

		void draw(VkCommandBuffer buf, VkPipelineLayout layout)
		{
			VkBuffer vbufs[] = { vbo.get_vertexBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(buf, 0, 1, vbufs, offsets);
			vkCmdBindIndexBuffer(buf, vbo.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
			for (auto& node : nodes)
			{
				vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ModelPushConstantBlock), &node.pcb);
				for (int i = node.primitive_range.first; i < node.primitive_range.second; i++)
				{
					auto& primitive = primitives[i];
					vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &materials[primitive.material].get_descriptorSet(), 0, nullptr);
					vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(ModelPushConstantBlock), sizeof(MaterialPushConstantBlock), &materials[primitive.material].get_pushConstantBlock());
					vkCmdDrawIndexed(buf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
				}
			}
		}

		Node* get_root() { return &root; }

	private:
		void update_nodes(int node, int parent = -1)
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
	};

	[[nodiscard]] Model loadModel(const Renderer& renderer, const std::string& name);
}
