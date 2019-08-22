
#pragma once

#include <stb_image.h>
#include <tiny_gltf.h>
#include <Datatypes.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

namespace blaze::util
{

	[[nodiscard]] auto loadModel(const std::string& name)	// TODO: Remove auto
	{
		using namespace tinygltf;

		tinygltf::Model model;
		TinyGLTF loader;
		std::string err;
		std::string warn;

		const std::string directory = name.substr(0, name.find_last_of('/')+1);

		const std::string NORMAL = "NORMAL";
		const std::string POSITION = "POSITION";
		const std::string TANGENT = "TANGENT";
		const std::string TEXCOORD_0 = "TEXCOORD_0";
		const std::string TEXCOORD_1 = "TEXCOORD_1";
		const std::string JOINTS_0 = "JOINTS_0";
		const std::string WEIGHTS_0 = "WEIGHTS_0";

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
			std::cerr << "Warn: " << warn << std::endl;
		}

		if (!err.empty()) {
			std::cerr << "Err: " << err << std::endl;
		}

		if (!ret) {
			std::cerr << "Failed to parse GTLF" << std::endl;
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
				std::vector<uint32_t> indices;
				size_t vertexCount;
				size_t indexCount;

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

					const auto& mat = model.materials[primitive.material];
					const auto& texInfo = mat.pbrMetallicRoughness.baseColorTexture;
					auto& diffuseTex = model.textures[texInfo.index];
					auto& diffuseImg = model.images[diffuseTex.source];
					// diffuseImageData = loadImage(directory + diffuseImg.uri);
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
						indices = std::vector<uint32_t>(dices, dices + indexCount);
					}
					break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						uint16_t* dices = reinterpret_cast<uint16_t*>(&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
						indices = std::vector<uint32_t>(dices, dices + indexCount);
						assert(indices.size() == indexCount);
						// throw std::runtime_error("USHORT index not supported");
					}
					break;
					}
				}

				std::vector<Vertex> vertices;
				vertices.reserve(vertexCount);
				for (size_t i = 0; i < vertexCount; i++)
				{
					vertices.push_back({
						glm::make_vec3(&posBuffer[3 * i]),
						glm::normalize(normBuffer ? glm::make_vec3(&normBuffer[3 * i]) : glm::vec3(0.0f)),
						tex0Buffer ? glm::make_vec2(&tex0Buffer[2 * i]) : glm::vec2(0.0f) });
				}

				// nodes.push_back({ vertices, indices, });
			}
		}
		// return nodes;
	}
}