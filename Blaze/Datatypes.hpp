
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
        alignas(16) glm::vec2 texCoord0;
        
        /// Texture Coordinate (1)
        alignas(16) glm::vec2 texCoord1;

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
		static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions(uint32_t binding = 0)
		{
			std::vector<VkVertexInputAttributeDescription> attributeDescs;
			uint32_t idx = 0;
			{
				VkVertexInputAttributeDescription attributeDesc = {};
				attributeDesc.binding = binding;
				attributeDesc.location = idx++;
				attributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
				attributeDesc.offset = offsetof(Vertex, position);
				attributeDescs.push_back(attributeDesc);

				attributeDesc.binding = binding;
				attributeDesc.location = idx++;
				attributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
				attributeDesc.offset = offsetof(Vertex, normal);
				attributeDescs.push_back(attributeDesc);

				attributeDesc.binding = binding;
				attributeDesc.location = idx++;
				attributeDesc.format = VK_FORMAT_R32G32_SFLOAT;
				attributeDesc.offset = offsetof(Vertex, texCoord0);
				attributeDescs.push_back(attributeDesc);

				attributeDesc.binding = binding;
				attributeDesc.location = idx++;
				attributeDesc.format = VK_FORMAT_R32G32_SFLOAT;
				attributeDesc.offset = offsetof(Vertex, texCoord1);
				attributeDescs.push_back(attributeDesc);
			}

			return attributeDescs;
		}
	};

    /**
     * @struct CameraUniformBufferObject
     *
     * @brief Holds camera data to be sent to GPU.
     *
     * @note Needs Restructuring.
     *
     * @addtogroup UniformBufferObjects
     * @{
     */
	struct CameraUniformBufferObject
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
     * @struct LightsUniformBufferObject
     *
     * @brief Holds Light data to be sent to GPU.
     *
     * @note Needs Restructuring.
     *
     * @addtogroup UniformBufferObjects
     * @{
     */
	struct LightsUniformBufferObject
	{
        /// The transformation matrix to directional light space coordinates.
		alignas(16) glm::mat4 dirLightTransform[4];
        
        /**
         * @brief The direction of the directional light.
         *
         * @arg xyz coordinates refer to the direction.
         * @arg w coordinate is the brightness.
         * @arg xyz should be normalized.
         *
         */
		alignas(16) glm::vec4 lightDir[4];
        
        /**
         * @brief The position of the point light.
         *
         * @arg xyz coordinates refer to the location.
         * @arg w coordinate is the brightness.
         *
         */
		alignas(16) glm::vec4 lightPos[16];
	
        /** 
         * @brief Indices of shadowMaps associated with the point lights.
         *
         * Valid shadow indices must be in range [0,MAX_SHADOWS)
         * Shadow index -1 denotes that there is no shadow.
         */
        alignas(16) int shadowIdx[16];

        /// Number of point lights.
		int numPointLights;

        /// Number of direcional lights.
		int numDirLights;
	};
    /// @}

    /**
     * @struct RendererUniformBufferObject
     *
     * @brief The actual data sent to the GPU.
     *
     * This struct is a aggregation of CameraUniformBufferObject and LightsUniformBufferObject.
     * They can be directly copied before sending to GPU view UBO update.
     *
     * @sa CameraUniformBufferObject
     * @sa LightsUniformBufferObject
     *
     * @addtogroup UniformBufferObjects
     * @{
     */
	struct RendererUniformBufferObject
	{
		alignas(16) glm::mat4 view;
		alignas(16) glm::mat4 projection;
		alignas(16) glm::vec3 viewPos;
		alignas(4)  float	  farPlane;
		alignas(16) glm::mat4 dirLightTransform[4];
		alignas(16) glm::vec4 lightDir[4];
		alignas(16) glm::vec4 lightPos[16];
		alignas(16) int shadowIdx[16];
		int numLights;
		int numDirLights;
	};
    /// @}

    /**
     * @struct CubemapUniformBufferObject
     *
     * @brief The data sent to the shaders that use cubemap framebuffers.
     *
     * @addtogroup UniformBufferObjects
     * @{
     */
	struct CubemapUniformBufferObject
	{
        /// Projection matrix common to each face of the cubemap.
		alignas(16) glm::mat4 projection;

        /// View matrix to look at the direction of each cubemap face.
		alignas(16) glm::mat4 view[6];
	};
    /// @}

    /**
     * @struct ShadowUniformBufferObject
     *
     * @brief The data sent to the omnidirectional shadow shaders.
     *
     * @note Needs Projection matrix from ShadowPushConstantBlock
     *
     * @addtogroup UniformBufferObjects
     * @{
     */
	struct ShadowUniformBufferObject
	{
        /// View matrices to look at each direction.
		alignas(16) glm::mat4 view[6];
	};
    /// @}

    /**
     * @struct SettingsUniformBufferObject
     *
     * @brief Main display settings exposed to both Player and GPU Shader.
     *
     * The component of the GUI settings that are updated to the GPU.
     * Used for shading options or debug displays.
     *
     * @addtogroup UniformBufferObjects
     * @{
     */
	struct SettingsUniformBufferObject
	{
        /// Difference image maps or data visualizable on the screen.
		alignas(4) enum ViewTextureMap : int
		{
			VTM_FULL,       /// < Full Render.
			VTM_DIFFUSE,    /// < Show only albedo/diffuse maps.
			VTM_NORMAL,     /// < Show only the world space normals.
			VTM_METALLIC,   /// < Show only the metallicity.
			VTM_ROUGHNESS,  /// < Show only the roughness.
			VTM_AO,         /// < Show only the ambient occlusion maps.
			VTM_EMISSION,   /// < Show only the emissions.
			VTM_POSITION,   /// < Show position coordinates normalized by far plane.
			VTM_MISC,       /// < Show a miscellaneous/in test visualization.
			VTM_MAX_COUNT
		} textureMap;
        
        /// Enable skybox.
		alignas(4) union
		{
			bool B;
			int I;
		} enableSkybox{ 1 };
        
        /// Enable IBL
		alignas(4) union
		{
			bool B;
			int I;
		} enableIBL{ 1 };

        /// Exposure
		alignas(4) float exposure{ 4.5f };
		
        /// Gamma
        alignas(4) float gamma{ 2.2f };
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
		glm::vec4 baseColorFactor{ 1.0f, 0, 1.0f, 1.0f };
		glm::vec4 emissiveColorFactor{ 1.0f, 0, 1.0f, 1.0f };
		float metallicFactor{ 1.0f };
		float roughnessFactor{ 1.0f };
		int baseColorTextureSet{ -1 };
		int physicalDescriptorTextureSet{ -1 };
		int normalTextureSet{ -1 };
		int occlusionTextureSet{ -1 };
		int emissiveTextureSet{ -1 };
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
     * During omnidirectional shadow mapping, ShadowUniformBufferObject provides
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
     * @struct BufferObject
     *
     * @brief Simple holder for info on a buffer (handle and allocation)
     */
	struct BufferObject
	{
		VkBuffer buffer{ VK_NULL_HANDLE };
		VmaAllocation allocation{ VK_NULL_HANDLE };
	};

    /**
     * @struct ImageObject
     *
     * @brief Simple holder for info on an image (handle, allocation and format)
     */
	struct ImageObject
	{
		VkImage image{ VK_NULL_HANDLE };
		VmaAllocation allocation{ VK_NULL_HANDLE };
		VkFormat format;
	};
}
