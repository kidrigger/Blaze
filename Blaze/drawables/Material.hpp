
#pragma once

#include <vkwrap/VkWrap.hpp>

#include <core/Texture2D.hpp>
#include <Datatypes.hpp>

namespace blaze
{

/**
 * @class Material
 *
 * @brief Collection of the material textures and constants and descriptor.
 *
 * Holds the data for material representing the material from glTF 2.0 and
 * holds a descriptor used to bind the entire material at once.
 *
 * A push constant block in the material is used to push indices and constant values
 * such as multipliers and factors.
 */
class Material
{
	Texture2D diffuse;
	Texture2D metallicRoughness;
	Texture2D normal;
	Texture2D occlusion;
	Texture2D emissive;
	MaterialPushConstantBlock pushConstantBlock;
	vkw::DescriptorSet descriptorSet;

public:
	/**
	 * @brief Constructor for Material class.
	 *
	 * All the input textures are \b Moved into the class.
	 *
	 * @param pushBlock The MaterialPushConstantBlock required.
	 * @param diff The Albedo Map.
	 * @param norm The Normal Map.
	 * @param metal The PhysicalDescription Map (Metallic/Roughness).
	 * @param ao The Ambient Occlusion Map.
	 * @param em The Emissive Map.
	 */
	Material(MaterialPushConstantBlock pushBlock, Texture2D&& diff, Texture2D&& norm, Texture2D&& metal, Texture2D&& ao,
			 Texture2D&& em);

	/**
	 * @name Move Constructors.
	 *
	 * @brief Set of move constructors for the class.
	 *
	 * Material is a move only class and hence the copy constructors are deleted.
	 *
	 * @{
	 */
	Material(Material&& other) noexcept;
	Material& operator=(Material&& other) noexcept;
	Material(const Material& other) = delete;
	Material& operator=(const Material& other) = delete;
	/**
	 * @}
	 */

	/**
	 * @fn generateDescriptorSet
	 *
	 * @brief Generates the Descriptor Set for the material textures.
	 *
	 * As the pool is not ready during material loading time,
	 * the Model loader lazily generates the sets after the construction of the pool.
	 *
	 * @param device The logical device in use.
	 * @param layout The descriptor set layout of the material descriptor set.
	 * @param pool The descriptor pool to create the set from.
	 *
	 * @note This step is necessary before use.
	 */
	void generateDescriptorSet(VkDevice device, VkDescriptorSetLayout layout, VkDescriptorPool pool);

	/**
	 * @name Getters
	 *
	 * @brief Getters for the private members.
	 *
	 * @{
	 */
	inline const VkDescriptorSet& get_descriptorSet() const
	{
		return descriptorSet.get();
	}
	inline const MaterialPushConstantBlock& get_pushConstantBlock() const
	{
		return pushConstantBlock;
	}
	/**
	 * @}
	 */
};

}