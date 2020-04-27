
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <map>
#include <vector>
#include <vkwrap/VkWrap.hpp>

namespace blaze::spirv
{
struct UniformInfo
{
	VkDescriptorType type;
	VkShaderStageFlags stages;
	uint32_t binding;
	uint32_t arrayLength;
	uint32_t size;
	std::string name;

	bool operator!=(const UniformInfo& other) const
	{
		return (binding != other.binding) || (type != other.type) || (arrayLength != other.arrayLength) ||
			   (size != other.size);
	}
	bool operator<(const UniformInfo& other) const
	{
		if (binding != other.binding)
		{
			return binding < other.binding;
		}
		else if (type != other.type)
		{
			return type < other.type;
		}
		else if (stages != other.stages)
		{
			return stages < other.stages;
		}
		else if (arrayLength != other.arrayLength)
		{
			return arrayLength < other.arrayLength;
		}
		else if (size != other.size)
		{
			return size < other.size;
		}
		return false;
	}

	explicit operator VkDescriptorSetLayoutBinding()
	{
		return VkDescriptorSetLayoutBinding{
			binding, type, arrayLength, stages, nullptr,
		};
	}
};

struct Shader
{
	struct Set
	{
		uint32_t set{0};
		std::vector<UniformInfo> uniforms;
		vkw::DescriptorSetLayout layout;

		using FormatID = uint32_t;
		struct Format
		{
			std::vector<UniformInfo> uniforms;
			bool operator<(const Format& other) const
			{
				auto size = uniforms.size();
				auto osize = other.uniforms.size();
				if (size != osize)
				{
					return size < osize;
				}

				auto iself = uniforms.begin();
				auto ioth = other.uniforms.begin();
				for (size_t i = 0; i < size; i++)
				{
					if (*iself != *ioth)
					{
						return *iself < *ioth;
					}
				}

				return false;
			}
		};
	};

	struct PushConstant
	{
		uint32_t size = 0;
		uint32_t stage = 0;
	};

	VertexInputFormat vertexInputFormat;
	int fragmentOutputs;

	PushConstant pushConstant;

	bool isCompute = false;
	std::vector<Set> sets;
	std::vector<Set::FormatID> setFormats;
	std::vector<VkPipelineShaderStageCreateInfo> pipelineStages;
	std::vector<vkw::ShaderModule> shaderModules; // to take ownership
	vkw::PipelineLayout pipelineLayout;
	std::map<std::string, std::pair<uint32_t, uint32_t>> uniformLocations;

	bool valid() const
	{
		return pipelineLayout.valid();
	}

#ifndef NDEBUG
	friend std::ostream& operator<<(std::ostream& out, const Shader& shader)
	{
		constexpr char nl = '\n';
		constexpr char tab = '\t';
		out << "Vertex Input: {" << nl;
		out << tab << "A_POSITION: " << shader.vertexInputFormat.A_POSITION << nl;
		out << tab << "A_NORMAL: " << shader.vertexInputFormat.A_NORMAL << nl;
		out << tab << "A_UV0: " << shader.vertexInputFormat.A_UV0 << nl;
		out << tab << "A_UV1: " << shader.vertexInputFormat.A_UV1 << nl;
		out << "Fragment Outputs:" << shader.fragmentOutputs << nl;
		out << "Push Constant: { size = " << shader.pushConstant.size << ", stages = 0x" << std::hex
			<< shader.pushConstant.stage << "}" << std::dec << nl;
		out << "IsCompute: " << (shader.isCompute ? "yes" : "no") << nl;
		out << "DescriptorSets: " << nl;
		for (auto& set : shader.sets)
		{
			out << "Set " << set.set << nl;
			for (auto& uniform : set.uniforms)
			{
				out << tab << "Uniform " << uniform.binding << " " << uniform.name << nl;
				out << tab << tab << "type: ";
#define ENUM_STR_CASE(e)                                                                                               \
	case VK_DESCRIPTOR_TYPE_##e:                                                                                       \
		out << #e << nl;                                                                                               \
		break
				switch (uniform.type)
				{
					ENUM_STR_CASE(SAMPLER);
					ENUM_STR_CASE(COMBINED_IMAGE_SAMPLER);
					ENUM_STR_CASE(SAMPLED_IMAGE);
					ENUM_STR_CASE(STORAGE_IMAGE);
					ENUM_STR_CASE(UNIFORM_TEXEL_BUFFER);
					ENUM_STR_CASE(STORAGE_TEXEL_BUFFER);
					ENUM_STR_CASE(UNIFORM_BUFFER);
					ENUM_STR_CASE(STORAGE_BUFFER);
					ENUM_STR_CASE(UNIFORM_BUFFER_DYNAMIC);
					ENUM_STR_CASE(STORAGE_BUFFER_DYNAMIC);
					ENUM_STR_CASE(INPUT_ATTACHMENT);
				};
				out << tab << tab << "size: " << uniform.size << nl;
			}
		}
		return out;
	}
#endif // NDEBUG
};

struct Pipeline
{
	vkw::Pipeline pipeline;
};
} // namespace blaze::spirv
