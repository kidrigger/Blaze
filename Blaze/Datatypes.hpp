
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
		alignas(16) glm::vec3 tangent;
		alignas(16) glm::vec2 texCoord;

		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDesc = {};
			bindingDesc.binding = 0;
			bindingDesc.stride = sizeof(Vertex);
			bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDesc;
		}

		static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 4> attributeDesc = {};

			attributeDesc[0].binding = 0;
			attributeDesc[0].location = 0;
			attributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDesc[0].offset = offsetof(Vertex, position);

			attributeDesc[1].binding = 0;
			attributeDesc[1].location = 1;
			attributeDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDesc[1].offset = offsetof(Vertex, normal);

			attributeDesc[2].binding = 0;
			attributeDesc[2].location = 2;
			attributeDesc[2].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDesc[2].offset = offsetof(Vertex, tangent);

			attributeDesc[3].binding = 0;
			attributeDesc[3].location = 3;
			attributeDesc[3].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDesc[3].offset = offsetof(Vertex, texCoord);

			return attributeDesc;
		}
	};

	struct CameraUniformBufferObject
	{
		alignas(16) glm::mat4 model;
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
		alignas(16) glm::vec3 lightPos;
		alignas(16) glm::vec3 viewPos;
	};

	struct MaterialPushConstantBlock
	{
		glm::vec4 baseColorFactor;
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
