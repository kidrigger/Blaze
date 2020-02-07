
#pragma once

#include <spirv_glsl.hpp>

#include <iostream>
#include <map>
#include <vector>

namespace blaze::util
{
class ShaderUtils
{
private:
	struct PushConstantRangeInfo
	{
		std::string name;
		uint32_t offset;
		uint32_t size;
		std::string accessibility;
	};

	struct DescriptorSetLayoutBindingInfo
	{
		std::string name;
		std::string type;
		uint32_t set;
		uint32_t binding;
		uint32_t array;
		std::vector<std::string> accessibility;
	};

	std::vector<VkPushConstantRange> pushConstantRanges;
	std::vector<PushConstantRangeInfo> pushConstantRangesInfo;
	std::map<uint32_t, std::map<uint32_t, VkDescriptorSetLayoutBinding>> layoutBindings;
	std::map<uint32_t, std::map<uint32_t, DescriptorSetLayoutBindingInfo>> layoutBindingsInfo;
	std::vector<VkDescriptorSetLayout> descriptorLayouts;

public:
	void loadGraphicsShaders(const std::string& vertShader, const std::string& fragShader)
	{
		addVertexShader(vertShader);
		addFragmentShader(fragShader);
	}

	void loadComputeShaders(const std::string& compShader)
	{
		addComputeShader(compShader);
	}

	void printInfo()
	{
		printf("Push Constant Ranges:\n");
		for (auto& pcri : pushConstantRangesInfo)
		{
			printf("\t%s: offset = %u, size = %u => %s\n", pcri.name.c_str(), pcri.offset, pcri.size,
				   pcri.accessibility.c_str());
		}

		printf("Descriptors:\n");
		for (auto& dset : layoutBindingsInfo)
		{
			for (auto& dbind : dset.second)
			{
				if (dbind.second.array <= 1)
				{
					printf("\t[%s] %s: (%u, %u) => ", dbind.second.type.c_str(), dbind.second.name.c_str(),
						   dbind.second.set, dbind.second.binding);
				}
				else
				{
					printf("\t[%s] %s: (%u, %u, %u) => ", dbind.second.type.c_str(), dbind.second.name.c_str(),
						   dbind.second.set, dbind.second.binding, dbind.second.array);
				}
				bool first = true;
				for (auto& acc : dbind.second.accessibility)
				{
					if (first)
					{
						printf("%s", acc.c_str());
					}
					else
					{
						printf(", %s", acc.c_str());
					}
					first = false;
				}
				printf("\n");
			}
		}
	}

private:
	void addVertexShader(const std::string& shaderFile)
	{
		auto spirv_binary = loadBinaryFile(shaderFile);

		spirv_cross::CompilerGLSL comp(std::move(spirv_binary));

		auto resources = comp.get_shader_resources();

		// Push Constant Ranges
		for (auto& res : resources.push_constant_buffers)
		{
			addPushRanges(VK_SHADER_STAGE_VERTEX_BIT, comp, res);
		}

		// Uniform Buffers
		for (auto& res : resources.uniform_buffers)
		{
			addUniformBuffer(VK_SHADER_STAGE_VERTEX_BIT, comp, res);
		}

		// Combined Image Samplers
		for (auto& res : resources.sampled_images)
		{
			addCombinedImageSampler(VK_SHADER_STAGE_VERTEX_BIT, comp, res);
		}
	}

	void addFragmentShader(const std::string& shaderFile)
	{
		auto spirv_binary = loadBinaryFile(shaderFile);

		spirv_cross::CompilerGLSL comp(std::move(spirv_binary));

		auto resources = comp.get_shader_resources();

		// Push Constant Ranges
		for (auto& res : resources.push_constant_buffers)
		{
			addPushRanges(VK_SHADER_STAGE_FRAGMENT_BIT, comp, res);
		}

		// Uniform Buffers
		for (auto& res : resources.uniform_buffers)
		{
			addUniformBuffer(VK_SHADER_STAGE_FRAGMENT_BIT, comp, res);
		}

		// Combined Image Samplers
		for (auto& res : resources.sampled_images)
		{
			addCombinedImageSampler(VK_SHADER_STAGE_FRAGMENT_BIT, comp, res);
		}
	}

	void addComputeShader(const std::string& shaderFile)
	{
		assert(0 && "Not Implemented");
	}

	void addPushRanges(VkShaderStageFlags stage, const spirv_cross::CompilerGLSL& comp,
					   const spirv_cross::Resource& res)
	{
		auto& ranges = comp.get_active_buffer_ranges(res.id);
		if (ranges.empty())
		{
			return;
		}

		std::vector<std::pair<uint32_t, uint32_t>> memberRanges;
		for (auto& range : ranges)
		{
			memberRanges.emplace_back(range.offset, range.range + range.offset);
		}

		std::sort(memberRanges.begin(), memberRanges.end());

		std::vector<std::pair<uint32_t, uint32_t>> pushRanges{memberRanges[0]};
		for (auto& range : memberRanges)
		{
			if (range.first <= pushRanges.back().second)
			{
				pushRanges.back().second = range.second;
			}
			else
			{
				pushRanges.push_back(range);
			}
		}

		for (auto& range : pushRanges)
		{
			pushConstantRangesInfo.push_back({res.name, range.first, range.second - range.first, getStageName(stage)});

			pushConstantRanges.push_back({stage, range.first, range.second - range.first});
		}
	}

	void addUniformBuffer(VkShaderStageFlags stage, const spirv_cross::CompilerGLSL& comp,
						  const spirv_cross::Resource& res)
	{
		auto stageName = getStageName(stage);

		auto set = comp.get_decoration(res.id, spv::Decoration::DecorationDescriptorSet);
		auto binding = comp.get_decoration(res.id, spv::Decoration::DecorationBinding);

		auto name = res.name;
		auto lastIdx = name.find_last_of('_');
		bool dynamic = lastIdx != std::string::npos && name.substr(lastIdx) == "_dyn";
		VkDescriptorType descType =
			(dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

		auto type = comp.get_type(res.type_id);
		uint32_t arraySize = type.array.empty() ? 1 : type.array[0];

		auto& dset = layoutBindings.find(set);
		if (dset != layoutBindings.end())
		{
			auto& lbind = dset->second.find(binding);
			if (lbind != dset->second.end())
			{
				assert(lbind->second.descriptorType == descType && "Descriptor Type must be same.");
				assert(lbind->second.descriptorCount == arraySize && "Descriptor Count must be same.");

				lbind->second.stageFlags |= stage;
				layoutBindingsInfo[set][binding].accessibility.push_back(stageName);
			}
			else
			{
				dset->second.insert(std::make_pair(
					binding, VkDescriptorSetLayoutBinding{binding, descType, arraySize, stage, nullptr}));

				layoutBindingsInfo[set][binding] = {
					name, dynamic ? "Dynamic Uniform" : "Uniform Buffer", set, binding, arraySize, {stageName}};
			}
		}
		else
		{
			auto [dset, success] =
				layoutBindings.insert(std::make_pair(set, std::map<uint32_t, VkDescriptorSetLayoutBinding>{}));
			assert(success);
			dset->second.insert(
				std::make_pair(binding, VkDescriptorSetLayoutBinding{binding, descType, arraySize, stage, nullptr}));

			layoutBindingsInfo[set][binding] = {
				name, dynamic ? "Dynamic Uniform" : "Uniform Buffer", set, binding, arraySize, {stageName}};
		}
	}

	void addCombinedImageSampler(VkShaderStageFlags stage, const spirv_cross::CompilerGLSL& comp,
								 const spirv_cross::Resource& res)
	{
		auto stageName = getStageName(stage);

		auto name = res.name;

		auto set = comp.get_decoration(res.id, spv::Decoration::DecorationDescriptorSet);
		auto binding = comp.get_decoration(res.id, spv::Decoration::DecorationBinding);

		auto& type = comp.get_type(res.type_id);
		uint32_t arraySize = type.array.empty() ? 1 : type.array[0];

		VkDescriptorType descType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		auto& dset = layoutBindings.find(set);
		if (dset != layoutBindings.end())
		{
			auto& lbind = dset->second.find(binding);
			if (lbind != dset->second.end())
			{
				assert(lbind->second.descriptorType == descType && "Descriptor Type must be same.");
				assert(lbind->second.descriptorCount == arraySize);

				lbind->second.stageFlags |= stage;
				layoutBindingsInfo[set][binding].accessibility.push_back(stageName);
			}
			else
			{
				dset->second.insert(std::make_pair(
					binding, VkDescriptorSetLayoutBinding{binding, descType, arraySize, stage, nullptr}));

				layoutBindingsInfo[set][binding] = {name, "Combined Sampler", set, binding, arraySize, {stageName}};
			}
		}
		else
		{
			auto [dset, success] =
				layoutBindings.insert(std::make_pair(set, std::map<uint32_t, VkDescriptorSetLayoutBinding>{}));
			assert(success);
			dset->second.insert(
				std::make_pair(binding, VkDescriptorSetLayoutBinding{binding, descType, arraySize, stage, nullptr}));

			layoutBindingsInfo[set][binding] = {name, "Combined Sampler", set, binding, arraySize, {stageName}};
		}
	}

	std::string getStageName(VkShaderStageFlags stage)
	{
		switch (stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return "Vertex Shader";
		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return "Fragment Shader";
		case VK_SHADER_STAGE_COMPUTE_BIT:
			return "Compute Shader";
		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return "Geometry Shader";
		default:
			return "Unknown";
		}
	}
};
} // namespace blaze::util
