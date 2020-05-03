
#include "Material.hpp"
#include <array>

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

	descriptorSet = vkw::DescriptorSet(newDescriptorSet);
}
}