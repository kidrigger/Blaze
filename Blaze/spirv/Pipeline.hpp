
#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

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
		
		using FormatKey = uint32_t;
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
						return iself < ioth;
					}
				}

				return false;
			}
		};
	};

	struct PushConstant
	{
		uint32_t size;
		uint32_t stage;
	};

	uint32_t vertexInputMask; // inputs used, this is mostly for validation
	int fragmentOutputs;

	PushConstant pushConstant;

	bool isCompute = false;
	std::vector<Set> sets;
	std::vector<Set::FormatKey> setFormats;
	std::vector<VkPipelineShaderStageCreateInfo> pipelineStages;
	std::vector<vkw::ShaderModule> shaderModules;	// to take ownership
	vkw::PipelineLayout pipelineLayout;

	bool valid() const
	{
		return pipelineLayout.valid();
	}
};
} // namespace blaze::spirv