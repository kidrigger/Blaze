
#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <thirdparty/vma/vk_mem_alloc.h>
#include <vector>

const int32_t MAX_DIR_LIGHTS = 1;
const int32_t MAX_POINT_LIGHTS = 16;
const int32_t MAX_CSM_SPLITS = 4;

namespace blaze
{

/**
 * @struct VertexInputFormat
 *
 * @brief Descriptes the input arrangement for the VertexInput stage.
 *
 * The format is used to properly assign the inputs to match in inputs in the Vertex Shader.
 */
struct VertexInputFormat
{
    /// The location of the position attribute of a vertex.
	uint32_t A_POSITION;
    /// The location of the normal attribute of a vertex.
	uint32_t A_NORMAL;
    /// The location of the UV coordinate set 0 attribute of a vertex.
	uint32_t A_UV0;
    /// The location of the UV coordinate set 1 attribute of a vertex.
	uint32_t A_UV1;

    /**
     * @brief Default Constructor.
     * 
     * The default constructor initializes the attributes to their default ascending order.
     */
	VertexInputFormat() : A_POSITION(0), A_NORMAL(1), A_UV0(2), A_UV1(3)
	{
	}
};

/**
 * @struct Vertex
 *
 * @brief Vertex type commonly used with interleaved data.
 */
struct Vertex
{
	/// Position of the vertex.
	alignas(16) glm::vec3 position;

	/// Normal vector outwards from the vertex.
	alignas(16) glm::vec3 normal;

	/// Texture Coordinate (0)
	alignas(16) glm::vec2 uv0;

	/// Texture Coordinate (1)
	alignas(16) glm::vec2 uv1;

	/**
	 * @fn getBindingDescription(uint32_t binding = 0)
	 *
	 * @brief Creates and returns the binding description for the Vertex.
	 *
	 * @param [in] binding Binding location of the vertex attribute.
	 *
	 * @return Vertex Input Binding Description
	 */
	static VkVertexInputBindingDescription getBindingDescription(uint32_t binding = 0)
	{
		VkVertexInputBindingDescription bindingDesc = {};
		bindingDesc.binding = binding;
		bindingDesc.stride = sizeof(Vertex);
		bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDesc;
	}

	/**
	 * @fn getAttributeDescriptions(uint32_t binding = 0)
	 *
	 * @brief Creates attribute descriptions for the vertex.
	 *
     * @param [in] format The binding locations for each of the vertex inputs.
	 * @param [in] binding Binding location of the vertex attribute.
	 *
	 * @returns vector of attribute descriptions
	 */
	static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(VertexInputFormat format = {},
																				   uint32_t binding = 0)
	{
		std::vector<VkVertexInputAttributeDescription> attributeDescs = {
			{
				format.A_POSITION,
				binding,
				VK_FORMAT_R32G32B32_SFLOAT,
				offsetof(Vertex, position),
			},
			{
				format.A_NORMAL,
				binding,
				VK_FORMAT_R32G32B32_SFLOAT,
				offsetof(Vertex, normal),
			},
			{
				format.A_UV0,
				binding,
				VK_FORMAT_R32G32_SFLOAT,
				offsetof(Vertex, uv0),
			},
			{
				format.A_UV1,
				binding,
				VK_FORMAT_R32G32_SFLOAT,
				offsetof(Vertex, uv1),
			},
		};

		return attributeDescs;
	}
};

/**
 * @struct CubemapUBlock
 *
 * @brief The data sent to the shaders that use cubemap framebuffers.
 *
 * @addtogroup UniformBufferObjects
 * @{
 */
struct CubemapUBlock
{
	/// Projection matrix common to each face of the cubemap.
	alignas(16) glm::mat4 projection;

	/// View matrix to look at the direction of each cubemap face.
	alignas(16) glm::mat4 view[6];
};
/// @}

/**
 * @struct CascadeShadowUniformBufferObject
 *
 * @brief The data sent to the cascaded shadow shaders.
 *
 * @addtogroup UniformBufferObjects
 * @{
 */
struct CascadeUBlock
{
	/// Projection matrices to look at each cascade.
	alignas(16) glm::mat4 view[4];

	/// Number of cascades.
	alignas(4) int numCascades;
};
/// @}

/**
 * @struct ModelPushConstantBlock
 *
 * @brief PCB for sending Model matrix to the GPU.
 *
 * @addtogroup PushConstantBlocks
 * @{
 */
struct ModelPushConstantBlock
{
	glm::mat4 model;
};
/// @}

/**
 * @struct ShadowPushConstantBlock
 *
 * @brief PCB for sending projection matrix and position to the GPU.
 *
 * During omnidirectional shadow mapping, ShadowUBlock provides
 * The view matrix for each face of the cubemap.
 * The Projection and position change per shadow, and hence are a part of the
 * PCB
 *
 * @addtogroup PushConstantBlocks
 * @{
 */
struct ShadowPushConstantBlock
{
	glm::mat4 projection;
	glm::vec3 position;
};
/// @}
} // namespace blaze
