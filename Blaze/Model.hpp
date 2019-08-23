
#pragma once

#include <vector>
#include "DataTypes.hpp"
#include "VertexBuffer.hpp"

#define GLFW_INCLUDE_VULKAN
#include <glfw/glfw3.h>

namespace blaze
{
	struct Primitive
	{
		uint32_t firstIndex;
		uint32_t vertexCount;
		uint32_t indexCount;
		bool hasIndex;
	};

	struct Model
	{
		std::vector<Primitive> primitives;
		IndexedVertexBuffer<Vertex> vbo;

		void operator()(VkCommandBuffer buf, VkPipelineLayout layout)
		{
			VkBuffer vbufs[] = { vbo.get_vertexBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(buf, 0, 1, vbufs, offsets);
			vkCmdBindIndexBuffer(buf, vbo.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
			for (auto& primitive : primitives)
			{
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

				Primitive newPrimitive;
				newPrimitive.firstIndex = static_cast<uint32_t>(indexBuffer.size());
				newPrimitive.vertexCount = static_cast<uint32_t>(vertexCount);
				newPrimitive.indexCount = static_cast<uint32_t>(indexCount);
				newPrimitive.hasIndex = indexCount > 0;
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

		blazeModel.primitives = std::move(primitives);
		blazeModel.vbo = IndexedVertexBuffer(renderer, vertexBuffer, indexBuffer);

		return blazeModel;
	}
}