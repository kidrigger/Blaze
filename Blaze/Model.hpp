
#pragma once

#include <vector>
#include "DataTypes.hpp"
#include "VertexBuffer.hpp"
#include "Texture.hpp"
#include "util/Managed.hpp"
#include "Renderer.hpp"

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

namespace blaze
{
	class Material
	{
		TextureImage diffuse;
		TextureImage metallicRoughness;
		TextureImage normal;
		TextureImage occlusion;
		TextureImage emissive;
		MaterialPushConstantBlock pushConstantBlock;
		util::Managed<VkDescriptorSet> descriptorSet;

	public:
		Material(MaterialPushConstantBlock pushBlock, TextureImage&& diff, TextureImage&& norm, TextureImage&& metal, TextureImage&& ao, TextureImage&& em)
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
			int idx = 0;
			std::array<VkWriteDescriptorSet, 5> writes = {};
			for(auto& write : writes)
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

	class Model
	{
	private:
		util::Managed<VkDescriptorPool> descriptorPool;
		std::vector<Primitive> primitives;
		std::vector<Material> materials;
		IndexedVertexBuffer<Vertex> vbo;

	public:
		Model()
		{
		}

		Model(const Renderer& renderer, std::vector<Primitive>& prims, std::vector<Material>& mats, IndexedVertexBuffer<Vertex>&& ivb)
			: primitives(std::move(prims)), materials(std::move(mats)), vbo(std::move(ivb))
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
			materials = std::move(other.materials);
			vbo = std::move(other.vbo);
			descriptorPool = std::move(other.descriptorPool);
			return *this;
		}

		Model(const Model& other) = delete;
		Model& operator=(const Model& other) = delete;

		void draw(VkCommandBuffer buf, VkPipelineLayout layout)
		{
			VkBuffer vbufs[] = { vbo.get_vertexBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(buf, 0, 1, vbufs, offsets);
			vkCmdBindIndexBuffer(buf, vbo.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
			for (auto& primitive : primitives)
			{
				vkCmdBindDescriptorSets(buf, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &materials[primitive.material].get_descriptorSet(), 0, nullptr);
				vkCmdPushConstants(buf, layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MaterialPushConstantBlock), &materials[primitive.material].get_pushConstantBlock());
				vkCmdDrawIndexed(buf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	};

	[[nodiscard]] Model loadModel(const Renderer& renderer, const std::string& name);
}
