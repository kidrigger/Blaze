
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <map>
#include <thirdparty/spirv_reflect/spirv_reflect.h>

#include <deque>
#include <spirv/Pipeline.hpp>
#include <vkwrap/VkWrap.hpp>

namespace blaze::spirv
{

struct ShaderStageData
{
	VkShaderStageFlagBits stage;
	std::vector<uint32_t> spirv;
	inline uint32_t size() const
	{
		return static_cast<uint32_t>(sizeof(decltype(spirv)::value_type) * spirv.size());
	}
	inline const void* code() const
	{
		return spirv.data();
	}
};

class PipelineFactory
{
	using SetFormat = Shader::Set::Format;
	using SetFormatKey = Shader::Set::FormatKey;

	VkDevice device;
	std::map<SetFormat, SetFormatKey> setFormatRegistry;

public:
	PipelineFactory(VkDevice device) noexcept : device(device)
	{
	}

	Shader createShader(const std::vector<ShaderStageData>& stages);

	~PipelineFactory()
	{
	
	}

private:
	vkw::PipelineLayout createPipelineLayout(const std::vector<Shader::Set>& info, const Shader::PushConstant& pcr) const;

	SetFormatKey getFormatKey(const std::vector<UniformInfo>& format);
};
}; // namespace blaze::spirv
