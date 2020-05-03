
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

struct VertexInputFormat
{
	uint32_t A_POSITION;
	uint32_t A_NORMAL;
	uint32_t A_UV0;
	uint32_t A_UV1;

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
 * @struct CameraUBlock
 *
 * @brief Holds camera data to be sent to GPU.
 *
 * @note Needs Restructuring.
 *
 * @addtogroup UniformBufferObjects
 * @{
 */
struct CameraUBlock
{
	/// The View matrix of the camera.
	alignas(16) glm::mat4 view;

	/// The Projection matrix of the camera.
	alignas(16) glm::mat4 projection;

	/// The position of the camera.
	alignas(16) glm::vec3 viewPos;

	/// The distance of the Far Plane of the frustum from the camera.
	alignas(4) float farPlane;
};
/// @}

/**
 * @struct LightsUBlock
 *
 * @brief Holds Light data to be sent to GPU.
 *
 * @note Needs Restructuring.
 *
 * @addtogroup UniformBufferObjects
 * @{
 */
struct LightsUBlock
{
	/// The transformation matrix to directional light space coordinates.
	alignas(16) glm::mat4 dirLightTransform[MAX_DIR_LIGHTS][MAX_CSM_SPLITS];

	/**
	 * @brief The direction of the directional light.
	 *
	 * @arg xyz coordinates refer to the direction.
	 * @arg w coordinate is the brightness.
	 * @arg xyz should be normalized.
	 *
	 */
	alignas(16) glm::vec4 lightDir[MAX_DIR_LIGHTS];

	/**
	 * @brief The Cascade split distances of the directional light.
	 *
	 * @arg idx = 0 refers to first split
	 * @arg idx = 1 refers to second split
	 * @arg idx = 2 refers to third split
	 * @arg idx = 3 is the number of splits
	 */
	alignas(16) glm::vec4 csmSplits[MAX_DIR_LIGHTS];

	/**
	 * @brief The position of the point light.
	 *
	 * @arg xyz coordinates refer to the location.
	 * @arg w coordinate is the brightness.
	 *
	 */
	alignas(16) glm::vec4 lightPos[MAX_POINT_LIGHTS];

	/**
	 * @brief Indices of shadowMaps associated with the point lights.
	 *
	 * Valid shadow indices must be in range [0,MAX_SHADOWS)
	 * Shadow index -1 denotes that there is no shadow.
	 */
	alignas(16) int shadowIdx[MAX_POINT_LIGHTS];

	/// Number of point lights.
	int numPointLights;

	/// Number of direcional lights.
	int numDirLights;
};
/// @}

/**
 * @struct RendererUBlock
 *
 * @brief The actual data sent to the GPU.
 *
 * This struct is a aggregation of CameraUBlock and LightsUBlock.
 * They can be directly copied before sending to GPU view UBO update.
 *
 * @sa CameraUBlock
 * @sa LightsUBlock
 *
 * @addtogroup UniformBufferObjects
 * @{
 */
struct RendererUBlock
{
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
	alignas(16) glm::vec3 viewPos;
	alignas(4) float farPlane;
	alignas(16) glm::mat4 dirLightTransform[MAX_DIR_LIGHTS][MAX_CSM_SPLITS];
	alignas(16) glm::vec4 lightDir[MAX_DIR_LIGHTS];
	alignas(16) glm::vec4 csmSplits[MAX_CSM_SPLITS];
	alignas(16) glm::vec4 lightPos[MAX_POINT_LIGHTS];
	alignas(16) int shadowIdx[MAX_POINT_LIGHTS];
	int numLights;
	int numDirLights;
};
/// @}

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
 * @struct ShadowUBlock
 *
 * @brief The data sent to the omnidirectional shadow shaders.
 *
 * @note Needs Projection matrix from ShadowPushConstantBlock
 *
 * @addtogroup UniformBufferObjects
 * @{
 */
struct ShadowUBlock
{
	/// View matrices to look at each direction.
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
 * @struct SettingsUBlock
 *
 * @brief Main display settings exposed to both Player and GPU Shader.
 *
 * The component of the GUI settings that are updated to the GPU.
 * Used for shading options or debug displays.
 *
 * @addtogroup UniformBufferObjects
 * @{
 */
struct SettingsUBlock
{
	/// Difference image maps or data visualizable on the screen.
	alignas(4) enum ViewTextureMap : int {
		VTM_FULL,	   /// < Full Render.
		VTM_DIFFUSE,   /// < Show only albedo/diffuse maps.
		VTM_NORMAL,	   /// < Show only the world space normals.
		VTM_METALLIC,  /// < Show only the metallicity.
		VTM_ROUGHNESS, /// < Show only the roughness.
		VTM_AO,		   /// < Show only the ambient occlusion maps.
		VTM_EMISSION,  /// < Show only the emissions.
		VTM_POSITION,  /// < Show position coordinates normalized by far plane.
		VTM_CASCADE,   /// < Show the CSM splits overlay on the render.
		VTM_MISC,	   /// < Misc
		VTM_MAX_COUNT
	} textureMap;

	/// Enable skybox.
	alignas(4) union {
		bool B;
		int I;
	} enableSkybox{1};

	/// Enable IBL
	alignas(4) union {
		bool B;
		int I;
	} enableIBL{1};

	/// Exposure
	alignas(4) float exposure{4.5f};

	/// Gamma
	alignas(4) float gamma{2.2f};
};
/// @}

/**
 * @struct MaterialPushConstantBlock
 *
 * @brief PCB for sending Material information to the GPU
 *
 * @addtogroup PushConstantBlocks
 * @{
 */
struct MaterialPushConstantBlock
{
	glm::vec4 baseColorFactor{1.0f, 0, 1.0f, 1.0f};
	glm::vec4 emissiveColorFactor{1.0f, 0, 1.0f, 1.0f};
	float metallicFactor{1.0f};
	float roughnessFactor{1.0f};
	int baseColorTextureSet{-1};
	int physicalDescriptorTextureSet{-1};
	int normalTextureSet{-1};
	int occlusionTextureSet{-1};
	int emissiveTextureSet{-1};
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

/**
 * @struct CascadeBlock
 *
 * @brief A struct to return cascaded shadow PCB data.
 */
struct CascadeBlock
{
	/// @brief The PV transformation of each cascade.
	glm::mat4 pvs[MAX_CSM_SPLITS];
	/// @brief The split distances of the cascade.
	glm::vec4 splits;
};

/**
 * @struct BufferObject
 *
 * @brief Simple holder for info on a buffer (handle and allocation)
 */
struct BufferObject
{
	VkBuffer buffer{VK_NULL_HANDLE};
	VmaAllocation allocation{VK_NULL_HANDLE};

	template <typename T, typename U>
	operator std::tuple<T, U>()
	{
		return {buffer, allocation};
	}
};

/**
 * @struct ImageObject
 *
 * @brief Simple holder for info on an image (handle, allocation and format)
 */
struct ImageObject
{
	VkImage image{VK_NULL_HANDLE};
	VmaAllocation allocation{VK_NULL_HANDLE};
	VkFormat format;

	template <typename T, typename U, typename V>
	operator std::tuple<T, U, V>()
	{
		return {image, allocation, format};
	}
};
} // namespace blaze
