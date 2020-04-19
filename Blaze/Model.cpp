
#include "Model.hpp"
#include <thirdparty/gltf/tiny_gltf.h>

namespace blaze
{

// Material

Material::Material(MaterialPushConstantBlock pushBlock, Texture2D&& diff, Texture2D&& norm, Texture2D&& metal,
				   Texture2D&& ao, Texture2D&& em)
	: pushConstantBlock(pushBlock), diffuse(std::move(diff)), metallicRoughness(std::move(metal)),
	  normal(std::move(norm)), occlusion(std::move(ao)), emissive(std::move(em))
{
}

Material::Material(Material&& other) noexcept
	: pushConstantBlock(other.pushConstantBlock), diffuse(std::move(other.diffuse)),
	  metallicRoughness(std::move(other.metallicRoughness)), normal(std::move(other.normal)),
	  occlusion(std::move(other.occlusion)), emissive(std::move(other.emissive)),
	  descriptorSet(std::move(other.descriptorSet))
{
}

Material& Material::operator=(Material&& other) noexcept
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

void Material::generateDescriptorSet(VkDevice device, VkDescriptorSetLayout layout, VkDescriptorPool pool)
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

	std::array<VkDescriptorImageInfo, 5> imageInfos = {diffuse.get_imageInfo(), metallicRoughness.get_imageInfo(),
													   normal.get_imageInfo(), occlusion.get_imageInfo(),
													   emissive.get_imageInfo()};

	std::array<VkWriteDescriptorSet, 5> writes{};

	int idx = 0;
	for (VkWriteDescriptorSet& write : writes)
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

	descriptorSet = util::Managed(
		newDescriptorSet, [device, pool](VkDescriptorSet& dset) { vkFreeDescriptorSets(device, pool, 1, &dset); });
}

// Node

Node::Node() noexcept
	: translation(0.0f), rotation(1.0f, 0.0f, 0.0f, 0.0f), scale(1.0f), primitive_range(0, 0), localTRS(1.0f)
{
}

Node::Node(const glm::vec3& trans, const glm::quat& rot, const glm::vec3& sc, const std::vector<int>& children,
		   const std::pair<int, int> primitive_range) noexcept
	: translation(trans), rotation(rot), scale(sc), children(children), primitive_range(primitive_range)
{
	localTRS =
		glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
}

Node::Node(const glm::mat4& TRS, const std::vector<int>& children, const std::pair<int, int> primitive_range) noexcept
	: localTRS(TRS), children(children), primitive_range(primitive_range)
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
	descriptorPool = Managed(createDescriptorPool(device, poolSizes, static_cast<uint32_t>(materials.size())),
							 [device](VkDescriptorPool& pool) { vkDestroyDescriptorPool(device, pool, nullptr); });

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

// loader

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

	if (!warn.empty())
	{
		cerr << "Warn: " << warn << endl;
	}

	if (!err.empty())
	{
		cerr << "Err: " << err << endl;
	}

	if (!ret)
	{
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
		auto toRGBA = [](uint8_t* data, tinygltf::Image& image, uint64_t texelCount) {
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
				pushConstantBlock.physicalDescriptorTextureSet =
					material.pbrMetallicRoughness.metallicRoughnessTexture.texCoord;

				auto& image =
					model.images[model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source];
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

		materials.emplace_back(pushConstantBlock, Texture2D{renderer.get_context(), diffuseImageData, true},
							   Texture2D{renderer.get_context(), normalImageData, true},
							   Texture2D{renderer.get_context(), metallicRoughnessImageData, true},
							   Texture2D{renderer.get_context(), occlusionImageData, true},
							   Texture2D{renderer.get_context(), emissiveImageData, true});

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

		materials.emplace_back(pushConstantBlock, Texture2D{renderer.get_context(), imgData},
							   Texture2D{renderer.get_context(), imgData}, Texture2D{renderer.get_context(), imgData},
							   Texture2D{renderer.get_context(), imgData}, Texture2D{renderer.get_context(), imgData});
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
			node_range = std::make_pair(static_cast<int>(primitives.size()),
										static_cast<int>(primitives.size() + mesh.primitives.size()));

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

						posBuffer = reinterpret_cast<float*>(
							&model.buffers[bufferView.buffer].data[posAccessor.byteOffset + bufferView.byteOffset]);
						vertexCount = posAccessor.count;
					}

					const auto& normAccessorIterator = primitive.attributes.find(NORMAL);
					bool hasNormAccessor = (normAccessorIterator != primitive.attributes.end());
					if (hasNormAccessor)
					{
						const auto& accessor = model.accessors[normAccessorIterator->second];
						const auto& bufferView = model.bufferViews[accessor.bufferView];

						normBuffer = reinterpret_cast<float*>(
							&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
					}

					const auto& tex0AccessorIterator = primitive.attributes.find(TEXCOORD_0);
					bool hastex0Accessor = (tex0AccessorIterator != primitive.attributes.end());
					if (hastex0Accessor)
					{
						const auto& accessor = model.accessors[tex0AccessorIterator->second];
						const auto& bufferView = model.bufferViews[accessor.bufferView];

						tex0Buffer = reinterpret_cast<float*>(
							&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
					}

					const auto& tex1AccessorIterator = primitive.attributes.find(TEXCOORD_1);
					bool hastex1Accessor = (tex1AccessorIterator != primitive.attributes.end());
					if (hastex1Accessor)
					{
						const auto& accessor = model.accessors[tex1AccessorIterator->second];
						const auto& bufferView = model.bufferViews[accessor.bufferView];

						tex1Buffer = reinterpret_cast<float*>(
							&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
					}
				}

				if (primitive.indices > -1)
				{
					const auto& accessor = model.accessors[primitive.indices];
					const auto& bufferView = model.bufferViews[accessor.bufferView];
					indexCount = accessor.count;

					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
						uint32_t* dices = reinterpret_cast<uint32_t*>(
							&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
						indices = vector<uint32_t>(dices, dices + indexCount);

						assert(indices.size() == indexCount);
					}
					break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
						uint16_t* dices = reinterpret_cast<uint16_t*>(
							&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
						indices = vector<uint32_t>(dices, dices + indexCount);

						assert(indices.size() == indexCount);
						// throw runtime_error("USHORT index not supported");
					}
					break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
						uint8_t* dices = reinterpret_cast<uint8_t*>(
							&model.buffers[bufferView.buffer].data[accessor.byteOffset + bufferView.byteOffset]);
						indices = vector<uint32_t>(dices, dices + indexCount);

						assert(indices.size() == indexCount);
						// throw runtime_error("USHORT index not supported");
					}
					break;
					default: {
						assert(false && "This shouldn't be possible");
					}
					break;
					}
				}
				Primitive newPrimitive{
					static_cast<uint32_t>(indexBuffer.size()), static_cast<uint32_t>(vertexCount),
					static_cast<uint32_t>(indexCount),
					static_cast<uint32_t>((primitive.material >= 0 ? primitive.material : materials.size() - 1))};
				primitives.push_back(newPrimitive);

				uint32_t startIndex = static_cast<uint32_t>(vertexBuffer.size());
				for (auto& index : indices)
				{
					indexBuffer.emplace_back(index + startIndex);
				}

				for (size_t i = 0; i < vertexCount; i++)
				{
					vertexBuffer.push_back(
						{glm::make_vec3(&posBuffer[3 * i]),
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
		nodes.emplace_back(glm::translate(glm::mat4(1.0f), T) * glm::mat4_cast(R) * glm::scale(glm::mat4(1.0f), S) * M,
						   node.children, node_range);
	}

	const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];

	auto ivb = IndexedVertexBuffer(renderer.get_context(), indexBuffer, vertexBuffer);
	blazeModel = Model(renderer, scene.nodes, nodes, primitives, materials, std::move(ivb));

	return blazeModel;
}
} // namespace blaze
