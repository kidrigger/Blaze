
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "DescriptorSet.hpp"
#include "Pipeline.hpp"
#include <map>
#include <thirdparty/spirv_reflect/spirv_reflect.h>
#include <vkwrap/VkWrap.hpp>

namespace blaze::spirv
{
struct LoadStoreConfig
{
	enum class LoadAction : uint8_t
	{
		CLEAR,
		READ,
		DONT_CARE,
		CONTINUE,
	};
	enum class StoreAction : uint8_t
	{
		READ,
		DONT_CARE,
		CONTINUE,
	};

	LoadAction colorLoad;
	StoreAction colorStore;
	LoadAction depthLoad;
	StoreAction depthStore;
};

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

struct GraphicsPipelineCreateInfo
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo;
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo;
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo;
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo;
	VkPipelineColorBlendStateCreateInfo colorblendCreateInfo;
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
};

struct AttachmentFormat
{
	VkImageUsageFlags usage;
	VkFormat format;
	VkSampleCountFlagBits sampleCount;

	bool operator!=(const AttachmentFormat& other) const
	{
		return (usage != other.usage) || (format != other.format) || (sampleCount != other.sampleCount);
	}

	bool operator<(const AttachmentFormat& other) const
	{
		if (usage != other.usage)
		{
			return usage < other.usage;
		}
		else if (format != other.format)
		{
			return format < other.format;
		}
		else if (sampleCount != other.sampleCount)
		{
			return sampleCount < other.sampleCount;
		}
		return false;
	}
};

struct Framebuffer
{
	using FormatID = uint32_t;
	struct Format
	{
		std::vector<AttachmentFormat> attachmentFormats;
		bool operator<(const Format& other) const
		{
			auto self = attachmentFormats.size();
			auto soth = other.attachmentFormats.size();
			if (self != soth)
			{
				return self < soth;
			}
			auto iself = attachmentFormats.begin();
			auto ioth = other.attachmentFormats.begin();
			if (*iself != *ioth)
			{
				return *iself < *ioth;
			}
			return false;
		}
	};
};

struct DescriptorFrame
{
	std::map<Shader::Set::FormatID, uint32_t> formatIDmap;
	vkw::DescriptorPool pool;
	std::vector<vkw::DescriptorSetVector> sets;
};

struct RenderPass
{
	Framebuffer::FormatID fbFormat;
	vkw::RenderPass renderPass;

	VkRenderPass get() const
	{
		return renderPass.get();
	}
};

class PipelineFactory
{
	using SetFormat = Shader::Set::Format;
	using SetFormatID = Shader::Set::FormatID;

	using FBFormat = Framebuffer::Format;
	using FBFormatID = Framebuffer::FormatID;

	VkDevice device{VK_NULL_HANDLE};
	std::map<SetFormat, SetFormatID> setFormatRegistry;
	std::map<FBFormat, FBFormatID> fbFormatRegistry;

public:
	PipelineFactory() noexcept
	{
	}

	PipelineFactory(VkDevice device) noexcept : device(device)
	{
	}

	Shader createShader(const std::vector<ShaderStageData>& stages);
	Pipeline createGraphicsPipeline(const Shader& shader, const RenderPass& renderPass,
									const GraphicsPipelineCreateInfo& createInfo);

	RenderPass createRenderPass(const std::vector<AttachmentFormat>& formats,
								const std::vector<VkSubpassDescription>& subpasses, LoadStoreConfig config,
								const VkRenderPassMultiviewCreateInfo* multiview = nullptr);

	vkw::DescriptorPool createDescriptorPool(const std::vector<Shader::Set*>& sets, uint32_t maxSets);
	DescriptorFrame createDescriptorSets(const std::vector<Shader::Set*>& sets, uint32_t count);

	inline bool valid() const
	{
		return device != VK_NULL_HANDLE;
	}

private:
	vkw::PipelineLayout createPipelineLayout(const std::vector<Shader::Set>& info,
											 const Shader::PushConstant& pcr) const;

	SetFormatID getFormatKey(const std::vector<UniformInfo>& format);
	FBFormatID getFormatKey(const std::vector<AttachmentFormat>& format);
};
}; // namespace blaze::spirv
