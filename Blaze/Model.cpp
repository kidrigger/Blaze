
#include "Model.hpp"
#include <gltf/tiny_gltf.h>

namespace blaze {

	[[nodiscard]] Model loadModel(const Renderer& renderer, const std::string& name)
	{
		using namespace std;

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
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
		vector<Node> nodes;
		vector<Primitive> primitives;
		vector<Material> materials;

		materials.reserve(model.materials.size());
		for (auto& material : model.materials)
		{
			auto toRGBA = [](uint8_t* data, tinygltf::Image& image, uint64_t texelCount)
			{
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
			};

			ImageData2D imgData;
			uint32_t* data = new uint32_t[256 * 256];
			memset(data, 0xFF00FFFF, 256 * 256);
			imgData.data = reinterpret_cast<uint8_t*>(data);
			imgData.width = 256;
			imgData.height = 256;
			imgData.size = 256 * 256 * 4;
			imgData.numChannels = 4;

			MaterialPushConstantBlock pushConstantBlock = {};
			ImageData2D diffuseImageData = imgData;
			ImageData2D normalImageData = imgData;
			ImageData2D metallicRoughnessImageData = imgData;
			ImageData2D occlusionImageData = imgData;
			ImageData2D emissiveImageData = imgData;

			{
				pushConstantBlock.baseColorFactor = glm::make_vec4(material.pbrMetallicRoughness.baseColorFactor.data());

				if (material.pbrMetallicRoughness.baseColorTexture.index < 0)
				{
					pushConstantBlock.baseColorTextureSet = -1;
				}
				else
				{
					pushConstantBlock.baseColorTextureSet = material.pbrMetallicRoughness.baseColorTexture.texCoord;

					auto& image = model.images[model.textures[material.pbrMetallicRoughness.baseColorTexture.index].source];
					uint64_t texelCount = static_cast<uint64_t>(image.width) * static_cast<uint64_t>(image.height);
					uint8_t* data = new uint8_t[texelCount * 4];

					toRGBA(data, image, texelCount);

					diffuseImageData.data = data;
					diffuseImageData.width = image.width;
					diffuseImageData.height = image.height;
					diffuseImageData.size = image.width * image.height * 4;
					diffuseImageData.numChannels = 4;
				}
			}

			{
				if (material.normalTexture.index < 0)
				{
					pushConstantBlock.normalTextureSet = -1;
				}
				else
				{
					pushConstantBlock.normalTextureSet = material.normalTexture.texCoord;

					auto& image = model.images[model.textures[material.normalTexture.index].source];
					uint64_t texelCount = static_cast<uint64_t>(image.width) * static_cast<uint64_t>(image.height);
					uint8_t* data = new uint8_t[texelCount * 4];

					toRGBA(data, image, texelCount);

					normalImageData.data = data;
					normalImageData.width = image.width;
					normalImageData.height = image.height;
					normalImageData.size = image.width * image.height * 4;
					normalImageData.numChannels = 4;
				}
			}

			{
				pushConstantBlock.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
				pushConstantBlock.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);

				if (material.pbrMetallicRoughness.metallicRoughnessTexture.index < 0)
				{
					pushConstantBlock.physicalDescriptorTextureSet = -1;
				}
				else
				{
					pushConstantBlock.physicalDescriptorTextureSet = material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;

					auto& image = model.images[model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source];
					uint64_t texelCount = static_cast<uint64_t>(image.width) * static_cast<uint64_t>(image.height);
					uint8_t* data = new uint8_t[texelCount * 4];

					toRGBA(data, image, texelCount);

					metallicRoughnessImageData.data = data;
					metallicRoughnessImageData.width = image.width;
					metallicRoughnessImageData.height = image.height;
					metallicRoughnessImageData.size = image.width * image.height * 4;
					metallicRoughnessImageData.numChannels = 4;
				}
			}

			{
				if (material.occlusionTexture.index < 0)
				{
					pushConstantBlock.occlusionTextureSet = -1;
				}
				else
				{
					pushConstantBlock.occlusionTextureSet = material.occlusionTexture.texCoord;

					auto& image = model.images[model.textures[material.occlusionTexture.index].source];
					uint64_t texelCount = static_cast<uint64_t>(image.width) * static_cast<uint64_t>(image.height);
					uint8_t* data = new uint8_t[texelCount * 4];

					toRGBA(data, image, texelCount);

					occlusionImageData.data = data;
					occlusionImageData.width = image.width;
					occlusionImageData.height = image.height;
					occlusionImageData.size = image.width * image.height * 4;
					occlusionImageData.numChannels = 4;
				}
			}

			{
				if (material.emissiveTexture.index < 0)
				{
					pushConstantBlock.emissiveTextureSet = -1;
				}
				else
				{
					pushConstantBlock.emissiveTextureSet = material.emissiveTexture.texCoord;

					pushConstantBlock.emissiveColorFactor = glm::make_vec4(material.emissiveFactor.data());

					auto& image = model.images[model.textures[material.emissiveTexture.index].source];
					uint64_t texelCount = static_cast<uint64_t>(image.width) * static_cast<uint64_t>(image.height);
					uint8_t* data = new uint8_t[texelCount * 4];

					toRGBA(data, image, texelCount);

					emissiveImageData.data = data;
					emissiveImageData.width = image.width;
					emissiveImageData.height = image.height;
					emissiveImageData.size = image.width * image.height * 4;
					emissiveImageData.numChannels = 4;
				}
			}

			materials.emplace_back(
				pushConstantBlock,
				Texture2D{ renderer.get_context(), diffuseImageData, true },
				Texture2D{ renderer.get_context(), normalImageData, true },
				Texture2D{ renderer.get_context(), metallicRoughnessImageData, true },
				Texture2D{ renderer.get_context(), occlusionImageData, true },
				Texture2D{ renderer.get_context(), emissiveImageData, true });

			if (diffuseImageData.data != imgData.data)
			{
				delete[] diffuseImageData.data;
			}
			if (normalImageData.data != imgData.data)
			{
				delete[] normalImageData.data;
			}
			if (metallicRoughnessImageData.data != imgData.data)
			{
				delete[] metallicRoughnessImageData.data;
			}
			if (occlusionImageData.data != imgData.data)
			{
				delete[] occlusionImageData.data;
			}
			if (emissiveImageData.data != imgData.data)
			{
				delete[] emissiveImageData.data;
			}
			delete[] imgData.data;
		}
		// default material
		{
			ImageData2D imgData;
			uint32_t* data = new uint32_t[256 * 256];
			memset(data, 0xFF00FFFF, 256 * 256);
			imgData.data = reinterpret_cast<uint8_t*>(data);
			imgData.width = 256;
			imgData.height = 256;
			imgData.size = 256 * 256 * 4;
			imgData.numChannels = 4;

			MaterialPushConstantBlock pushConstantBlock = {};

			materials.emplace_back(
				pushConstantBlock,
				Texture2D{ renderer.get_context(), imgData },
				Texture2D{ renderer.get_context(), imgData },
				Texture2D{ renderer.get_context(), imgData },
				Texture2D{ renderer.get_context(), imgData },
				Texture2D{ renderer.get_context(), imgData });
			delete[] data;
		}

		for (const auto& node : model.nodes)
		{
			std::pair<int, int> node_range;
			if (node.mesh < 0)
			{
				node_range = std::make_pair(0, 0);
			}
			else
			{
				const auto& mesh = model.meshes[node.mesh];
				node_range = std::make_pair(static_cast<int>(primitives.size()), static_cast<int>(primitives.size() + mesh.primitives.size()));

				for (auto& primitive : mesh.primitives)
				{
					float* posBuffer = nullptr;
					float* normBuffer = nullptr;
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

						const auto& tex1AccessorIterator = primitive.attributes.find(TEXCOORD_1);
						bool hastex1Accessor = (tex1AccessorIterator != primitive.attributes.end());
						if (hastex1Accessor)
						{
							const auto& accessor = model.accessors[tex1AccessorIterator->second];
							const auto& bufferView = model.bufferViews[accessor.bufferView];

							tex1Buffer = reinterpret_cast<float*>(&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
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

							assert(indices.size() == indexCount);
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
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						{
							uint8_t* dices = reinterpret_cast<uint8_t*>(&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
							indices = vector<uint32_t>(dices, dices + indexCount);

							assert(indices.size() == indexCount);
							// throw runtime_error("USHORT index not supported");
						}
						break;
						default:
						{
							assert(false && "This shouldn't be possible");
						}
						break;
						}
					}
					Primitive newPrimitive{ static_cast<uint32_t>(indexBuffer.size()), static_cast<uint32_t>(vertexCount), static_cast<uint32_t>(indexCount), static_cast<uint32_t>((primitive.material >= 0 ? primitive.material : materials.size() - 1)) };
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
							glm::normalize(normBuffer ? glm::make_vec3(&normBuffer[3 * i]) : glm::vec3(0.0f, 0.0f, 1.0f)),
							tex0Buffer ? glm::make_vec2(&tex0Buffer[2 * i]) : glm::vec2(0.0f),
							tex1Buffer ? glm::make_vec3(&tex1Buffer[2 * i]) : glm::vec2(0.0f)});
					}
				}
			}

			glm::vec3 T(0.0f);
			glm::quat R(1.0f, 0.0f, 0.0f, 0.0f);
			glm::vec3 S(1.0f);
			glm::mat4 M(1.0f);
			if (node.translation.size() == 3)
			{
				T = glm::make_vec3(node.translation.data());
			}
			if (node.rotation.size() == 4)
			{
				R = glm::make_quat(node.rotation.data());
			}
			if (node.scale.size() == 3)
			{
				S = glm::make_vec3(node.scale.data());
			}
			if (node.matrix.size() == 16)
			{
				M = glm::make_mat4(node.matrix.data());
			}
			nodes.emplace_back(glm::translate(glm::mat4(1.0f), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0f), S) * M, node.children, node_range);
			
		}

		const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];

		auto ivb = IndexedVertexBuffer(renderer.get_context(), vertexBuffer, indexBuffer);
		blazeModel = Model(renderer, scene.nodes, nodes, primitives, materials, std::move(ivb));

		return blazeModel;
	}
}
