
#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <vk_mem_alloc.h>

namespace blaze
{
	struct Vertex
	{
		alignas(16) glm::vec3 position;
		alignas(16) glm::vec3 normal;
		alignas(16) glm::vec2 texCoord0;
		alignas(16) glm::vec2 texCoord1;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDesc = {};
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(Vertex);
			bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDesc;
		}

		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions()
		{
			std::vector<VkVertexInputAttributeDescription> attributeDescs;
			uint32_t idx = 0;
			{
				VkVertexInputAttributeDescription attributeDesc = {};
				attributeDesc.binding = 0;
				attributeDesc.location = idx++;
				attributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
				attributeDesc.offset = offsetof(Vertex, position);
				attributeDescs.push_back(attributeDesc);

				attributeDesc.binding = 0;
				attributeDesc.location = idx++;
				attributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
				attributeDesc.offset = offsetof(Vertex, normal);
				attributeDescs.push_back(attributeDesc);

				attributeDesc.binding = 0;
				attributeDesc.location = idx++;
				attributeDesc.format = VK_FORMAT_R32G32_SFLOAT;
				attributeDesc.offset = offsetof(Vertex, texCoord0);
				attributeDescs.push_back(attributeDesc);

				attributeDesc.binding = 0;
				attributeDesc.location = idx++;
				attributeDesc.format = VK_FORMAT_R32G32_SFLOAT;
				attributeDesc.offset = offsetof(Vertex, texCoord1);
				attributeDescs.push_back(attributeDesc);
			}

			return attributeDescs;
		}
	};

	struct CameraUniformBufferObject
	{
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
		alignas(16) glm::vec3 viewPos;
		int numLights;
		alignas(16) glm::vec4 lightPos[16];
	};

	struct MaterialPushConstantBlock
	{
		glm::vec4 baseColorFactor{ 1.0f, 0, 1.0f, 1.0f };
		glm::vec4 emissiveColorFactor{ 1.0f, 0, 1.0f, 1.0f };
		float metallicFactor{ 1.0f };
		float roughnessFactor{ 1.0f };
		int baseColorTextureSet{ -1 };
		int physicalDescriptorTextureSet{ -1 };
		int normalTextureSet{ -1 };
		int occlusionTextureSet{ -1 };
		int emissiveTextureSet{ -1 };
		// float workflow;
		// float alphaMask;
		// alphaMaskCutoff;
	};

	struct ModelPushConstantBlock
	{
		glm::mat4 model;
	};

	struct BufferObject
	{
		VkBuffer buffer{ VK_NULL_HANDLE };
		VmaAllocation allocation{ VK_NULL_HANDLE };
	};

	struct ImageObject
	{
		VkImage image{ VK_NULL_HANDLE };
		VmaAllocation allocation{ VK_NULL_HANDLE };
		VkFormat format;
	};
}
