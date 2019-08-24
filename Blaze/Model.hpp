
#pragma once

#include <vector>
#include "DataTypes.hpp"
#include "VertexBuffer.hpp"
#include "util/Managed.hpp"

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

namespace blaze
{
	class Material
	{
		TextureImage diffuse;
		util::Managed<VkDescriptorSet> descriptorSet;

	public:
		Material(TextureImage&& diff)
			: diffuse(std::move(diff))
		{
		}

		Material(Material&& other) noexcept
			: diffuse(std::move(other.diffuse)),
			descriptorSet(std::move(other.descriptorSet))
		{
		}

		Material& operator=(Material&& other) noexcept
		{
			if (this == &other)
			{
				return *this;
			}
			diffuse = std::move(other.diffuse);
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

			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.descriptorCount = 1;
			write.dstSet = newDescriptorSet;
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.pImageInfo = &diffuse.get_imageInfo();;

			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

			descriptorSet = util::Managed(newDescriptorSet, [device, pool](VkDescriptorSet& dset) { vkFreeDescriptorSets(device, pool, 1, &dset); });
		}

		Material(const Material& other) = delete;
		Material& operator=(const Material& other) = delete;

		const VkDescriptorSet& get_descriptorSet() const { return descriptorSet.get(); }
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
				vkCmdDrawIndexed(buf, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
			}
		}
	};

	[[nodiscard]] auto loadModel(const Renderer& renderer, const std::string& name)	// TODO: Remove auto
	{
		using namespace tinygltf;
		using namespace std;

		tinygltf::Model model;
		TinyGLTF loader;
		string err;
		string warn;
		Model blazeModel;

		const string directory = name.substr(0, name.find_last_of('/') + 1);

		const string NORMAL = "NORMAL";
		const string POSITION = "POSITION";
		const string TANGENT = "TANGENT";
		const string TEXCOORD_0 = "TEXCOORD_0";
		const string TEXCOORD_1 = "TEXCOORD_1";
		const string JOINTS_0 = "JOINTS_0";
		const string WEIGHTS_0 = "WEIGHTS_0";

		auto ext = name.substr(name.find_last_of('.') + 1);

		bool ret = false;
		if (ext == "gltf")
		{
			ret = loader.LoadASCIIFromFile(&model, &err, &warn, name);
		}
		else if (ext == "glb")
		{
			ret = loader.LoadBinaryFromFile(&model, &err, &warn, name); // for binary glTF(.glb)
		}

		if (!warn.empty()) {
			cerr << "Warn: " << warn << endl;
		}

		if (!err.empty()) {
			cerr << "Err: " << err << endl;
		}

		if (!ret) {
			cerr << "Failed to parse GTLF" << endl;
		}

		vector<Vertex> vertexBuffer;
		vector<uint32_t> indexBuffer;
		vector<Primitive> primitives;
		vector<Material> materials;

		materials.reserve(model.materials.size());
		for (auto& material : model.materials)
		{
			auto& image = model.images[model.textures[material.pbrMetallicRoughness.baseColorTexture.index].source];
			uint64_t texelCount = static_cast<uint64_t>(image.width) * static_cast<uint64_t>(image.height);
			uint8_t* data = new uint8_t[texelCount * 4];
			if (image.component == 3)
			{
				int offset = 0;
				for (uint64_t i = 0; i < texelCount; i++)
				{
					for (int j = 0; j < 3; j++)
					{
						data[i + j + offset] = image.image[i + j];
					}
					offset++;
				}
			}
			else if (image.component == 4)
			{
				memcpy(data, image.image.data(), texelCount * 4);
			}
			ImageData imgData;
			imgData.data = data;
			imgData.width = image.width;
			imgData.height = image.height;
			imgData.size = image.width * image.height * 4;
			imgData.numChannels = 4;
			materials.emplace_back(TextureImage{ renderer, imgData });
			delete[] data;
		}
		// default material
		{
			ImageData imgData;
			uint32_t* data = new uint32_t[256 * 256];
			memset(data, 0xFF00FFFF, 256 * 256);
			imgData.data = reinterpret_cast<uint8_t*>(data);
			imgData.width = 256;
			imgData.height = 256;
			imgData.size = 256 * 256 * 4;
			imgData.numChannels = 4;
			materials.emplace_back(TextureImage{ renderer, imgData });
			delete[] data;
		}

		const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
		for (int node_index : scene.nodes)
		{
			const auto& node = model.nodes[node_index];
			const auto& mesh = model.meshes[node.mesh];
			for (auto& primitive : mesh.primitives)
			{
				float* posBuffer = nullptr;
				float* normBuffer = nullptr;
				float* tanBuffer = nullptr;
				float* tex0Buffer = nullptr;
				float* tex1Buffer = nullptr;
				float* joint0Buffer = nullptr;
				float* joint1Buffer = nullptr;
				size_t vertexCount = 0;
				size_t indexCount = 0;
				vector<uint32_t> indices;

				{
					const auto& posAccessorIterator = primitive.attributes.find(POSITION);
					bool hasPosAccessor = (posAccessorIterator != primitive.attributes.end());
					assert(hasPosAccessor);
					{
						const auto& posAccessor = model.accessors[posAccessorIterator->second];
						const auto& bufferView = model.bufferViews[posAccessor.bufferView];

						posBuffer = reinterpret_cast<float*>(&model.buffers[bufferView.buffer].data[posAccessor.byteOffset + bufferView.byteOffset]);
						vertexCount = posAccessor.count;
					}

					const auto& normAccessorIterator = primitive.attributes.find(NORMAL);
					bool hasNormAccessor = (normAccessorIterator != primitive.attributes.end());
					if (hasNormAccessor)
					{
						const auto& accessor = model.accessors[normAccessorIterator->second];
						const auto& bufferView = model.bufferViews[accessor.bufferView];

						normBuffer = reinterpret_cast<float*>(&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
					}

					const auto& tex0AccessorIterator = primitive.attributes.find(TEXCOORD_0);
					bool hastex0Accessor = (tex0AccessorIterator != primitive.attributes.end());
					if (hastex0Accessor)
					{
						const auto& accessor = model.accessors[tex0AccessorIterator->second];
						const auto& bufferView = model.bufferViews[accessor.bufferView];

						tex0Buffer = reinterpret_cast<float*>(&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
					}
				}

				if (primitive.indices > -1)
				{
					const auto& accessor = model.accessors[primitive.indices];
					const auto& bufferView = model.bufferViews[accessor.bufferView];
					indexCount = accessor.count;

					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					{
						uint32_t* dices = reinterpret_cast<uint32_t*>(&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
						indices = vector<uint32_t>(dices, dices + indexCount);
					}
					break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						uint16_t* dices = reinterpret_cast<uint16_t*>(&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
						indices = vector<uint32_t>(dices, dices + indexCount);
						assert(indices.size() == indexCount);
						// throw runtime_error("USHORT index not supported");
					}
					break;
					}
				}

				Primitive newPrimitive{ static_cast<uint32_t>(indexBuffer.size()), static_cast<uint32_t>(vertexCount), static_cast<uint32_t>(indexCount), static_cast<uint32_t>((primitive.material >= 0 ? primitive.material : materials.size()-1))};
				primitives.push_back(newPrimitive);

				uint32_t startIndex = static_cast<uint32_t>(vertexBuffer.size());
				for (auto& index : indices)
				{
					indexBuffer.emplace_back(index + startIndex);
				}

				for (size_t i = 0; i < vertexCount; i++)
				{
					vertexBuffer.push_back({
						glm::make_vec3(&posBuffer[3 * i]),
						glm::normalize(normBuffer ? glm::make_vec3(&normBuffer[3 * i]) : glm::vec3(0.0f)),
						tex0Buffer ? glm::make_vec2(&tex0Buffer[2 * i]) : glm::vec2(0.0f) });
				}
			}
		}

		auto ivb = IndexedVertexBuffer(renderer, vertexBuffer, indexBuffer);
		blazeModel = Model(renderer, primitives, materials, std::move(ivb));

		return blazeModel;
	}
}