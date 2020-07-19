
#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Pipeline.hpp"
#include <map>
#include <thirdparty/spirv_reflect/spirv_reflect.h>
#include <vkwrap/VkWrap.hpp>

/**
 * @brief The namespace for all Spirv reflection related structures.
 *
 * TODO: Remove the namespace.
 */
namespace blaze::spirv
{
/**
 * @brief Construction struct for a RenderPass' attachment behavior.
 */
struct LoadStoreConfig
{
	/// Description of behavior at RenderPass begin.
	enum class LoadAction : uint8_t
	{
		CLEAR,	   ///< Clear to the predefined 'empty' value.
		READ,	   ///< Was being used to sample.
		DONT_CARE, ///< Whatever, doesn't matter.
		CONTINUE,  ///< Was used as at attachment before.
	};
	enum class StoreAction : uint8_t
	{
		READ,	   ///< Will be used as a sample image next.
		DONT_CARE, ///< Doesn't matter if you store the data.
		CONTINUE,  ///< Will be continued to use as attachment.
	};

	LoadAction loadAction;
	StoreAction storeAction;

	LoadStoreConfig() : LoadStoreConfig(LoadAction::CLEAR, StoreAction::CONTINUE)
	{
	}

	LoadStoreConfig(LoadAction loadAction, StoreAction storeAction) : loadAction(loadAction), storeAction(storeAction)
	{
	}
};

/**
 * @brief Data about one of the shader stages.
 *
 * Contains both the shader stage and the spirv code to use.
 */
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

/**
 * @brief Info about literally all the stages in pipeline creation.
 *
 * Just a struct to compress all the required config information into it.
 * Should have a default constructor, and only tuning afterwards.
 */
struct GraphicsPipelineCreateInfo
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo;
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo;
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo;
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo;
	VkPipelineColorBlendStateCreateInfo colorblendCreateInfo;
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
	uint32_t subpass{0};
};

/**
 * @brief Description of an attachment used for the renderpass and framebuffer.
 */
struct AttachmentFormat
{
	VkImageUsageFlags usage;
	VkFormat format;
	VkSampleCountFlagBits sampleCount;
	LoadStoreConfig loadStoreConfig;

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

/// Literally just the format info - TODO
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

/**
 * @brief Collection of descriptor sets.
 *
 * Specially made for uniforms that vary per-frame, and thus have a
 * \ref UBOVector for the buffers.
 *
 * Each SetVector should ideally map to a UBOVector.
 */
struct SetVector
{
	Shader::Set::FormatID formatID;
	vkw::DescriptorPool pool;
	vkw::DescriptorSetVector sets;
	uint32_t setIdx;
	std::vector<UniformInfo> info;

	const VkDescriptorSet& operator[](uint32_t idx) const
	{
		return sets[idx];
	}

	const UniformInfo* getUniform(const std::string_view& name) const;

	uint32_t size() const
	{
		return static_cast<uint32_t>(sets.size());
	}
};

/**
 * @brief Wrapper over a single set.
 *
 * Just to create a uniform situation, a SetSingleton is a SetVector
 * of size 1. Mainly to use for non frame-varying buffers.
 */
struct SetSingleton
{
	Shader::Set::FormatID formatID;
	vkw::DescriptorPool pool;
	vkw::DescriptorSet set;
	uint32_t setIdx;
	std::vector<UniformInfo> info;

	const VkDescriptorSet& get() const
	{
		return set.get();
	}

	// Consistency
	const VkDescriptorSet& operator[](uint32_t idx) const
	{
		return set.get();
	}

	const UniformInfo* getUniform(const std::string_view& name) const;

	uint32_t size() const
	{
		return 1u;
	}
};

/**
 * @brief Holder for a renderpass and the framebuffer format.
 *
 * Ideally a framebuffer should be verified with the renderpass before use.
 */
struct RenderPass
{
	Framebuffer::FormatID fbFormat;
	vkw::RenderPass renderPass;

	VkRenderPass get() const
	{
		return renderPass.get();
	}

	inline bool valid() const
	{
		return renderPass.valid();
	}
};

/**
 * @brief Factory class for pipelines based on shader reflection.
 *
 * PipelineFactory is created such that it will be easy to provide configuration of a pipeline and the shaders and let
 * the factory handle matching of descriptors etc.
 *
 * This makes it much easier to create pipeline layouts and allows shaders to change their descriptor set indices
 * mostly without needing any change in the source code.
 */
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

	/**
	 * @brief Creates the Shader with all the reflection information.
	 *
	 * @param stages The ShaderStageData of each of the stages to be used in the pipeline.
	 */
	Shader createShader(const std::vector<ShaderStageData>& stages);

	/**
	 * @brief Creates the graphic pipeline from the shader and renderpass.
	 *
	 * @param shader The Shader to use to create the pipeline.
	 * @param renderPass The RenderPass that would use the pipeline.
	 * @param createInfo The complete set of structs that describe the pipeline variables.
	 */
	Pipeline createGraphicsPipeline(const Shader& shader, const RenderPass& renderPass,
									const GraphicsPipelineCreateInfo& createInfo);

	/**
	 * @brief Creates a renderpass given the attachments and subpasses.
	 *
	 * @param formats The collection of all the attachments that the renderpass will use.
	 * @param config Describes the load/store behaviour on all the attachments.
	 * @param subpasses The VkSubpassDescriptor of all the subpasses that the renderpass will contain.
	 * @param multiview If the renderpass uses multiview, this struct must be provided.
	 */
	RenderPass createRenderPass(const std::vector<AttachmentFormat>& formats,
								const std::vector<VkSubpassDescription>& subpasses,
								const std::vector<VkSubpassDependency>& dependencies,
								const VkRenderPassMultiviewCreateInfo* multiview = nullptr);

	/**
	 * @brief Creates a renderpass given the attachments and subpasses.
	 *
	 * @param formats The collection of all the attachments that the renderpass will use.
	 * @param config Describes the load/store behaviour on all the attachments.
	 * @param subpasses The VkSubpassDescriptor of all the subpasses that the renderpass will contain.
	 * @param multiview If the renderpass uses multiview, this struct must be provided.
	 */
	RenderPass createRenderPass(const std::vector<AttachmentFormat>& formats,
								const std::vector<VkSubpassDescription>& subpasses,
								const VkRenderPassMultiviewCreateInfo* multiview = nullptr);

	/**
	 * @brief Creates the DescriptorSets for the given set.
	 *
	 * @param set The Set that must be bound to.
	 * @param count The number of sets that the vector must contain.
	 */
	SetVector createSets(const Shader::Set& set, uint32_t count);

	/**
	 * @brief Creates the DescriptorSets for the given set.
	 *
	 * @param set The Set that must be bound to.
	 */
	SetSingleton createSet(const Shader::Set& set);

	/// @brief Asserts validity of the factory.
	inline bool valid() const
	{
		return device != VK_NULL_HANDLE;
	}

private:
	vkw::DescriptorPool createDescriptorPool(const Shader::Set& sets, uint32_t maxSets);
	vkw::PipelineLayout createPipelineLayout(const std::vector<Shader::Set>& info,
											 const Shader::PushConstant& pcr) const;

	SetFormatID getFormatKey(const std::vector<UniformInfo>& format);
	FBFormatID getFormatKey(const std::vector<AttachmentFormat>& format);
};
}; // namespace blaze::spirv
