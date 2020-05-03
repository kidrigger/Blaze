
#include "PipelineFactory.hpp"

#include <map>
#include <set>
#include <stdexcept>
#include <util/files.hpp>

namespace blaze::spirv
{
enum ErrCode : uint8_t
{
	SUCCESS = 0,
	UNIFORM_MISMATCH = 1,
	UNUNIFIED_PUSH_CONST = 2,
	SPIRV_REFLECT_FAIL = 255,
};

inline uint8_t SPV_ASSERT(SpvReflectResult result)
{
	if (result != SPV_REFLECT_RESULT_SUCCESS)
	{
		std::cerr << "Spirv Reflection Error" << std::endl;
		return ErrCode::SPIRV_REFLECT_FAIL;
	}
	return ErrCode::SUCCESS;
}

Shader PipelineFactory::createShader(const std::vector<ShaderStageData>& stages)
{
	using namespace std;
#ifndef NDEBUG
	{
		VkShaderStageFlags stageCheck = {};
		for (const auto& stage : stages)
		{
			auto check = stageCheck & stage.stage;
			stageCheck |= stage.stage;
			if (check)
			{
				throw runtime_error("Shader stage " + to_string(stage.stage) + " duplicated");
			}
			if (stage.stage == VK_SHADER_STAGE_COMPUTE_BIT && stages.size() != 1)
			{
				throw runtime_error("Compute shaders can only receive 1 stage.");
			}
		}
	}
#endif // NDEBUG

	SpvReflectShaderModule reflector;
	SpvReflectResult result;
	bool isCompute = false;

	uint32_t fragmentOutputs = 0;

	map<uint32_t, map<uint32_t, UniformInfo>> uniformInfos;
	Shader::PushConstant pushConst = {};
	VertexInputFormat vertexInput = {};

	uint8_t error = ErrCode::SUCCESS;

	// Process each stage
	for (auto& stage : stages)
	{
		if (stage.stage == VK_SHADER_STAGE_COMPUTE_BIT)
		{
			isCompute = true;
		}

		result = spvReflectCreateShaderModule(stage.size(), stage.code(), &reflector);
		error |= SPV_ASSERT(result);

		uint32_t setCount;
		result = spvReflectEnumerateDescriptorSets(&reflector, &setCount, nullptr);
		error |= SPV_ASSERT(result);

		if (setCount)
		{
			vector<SpvReflectDescriptorSet*> sets(setCount, nullptr);
			result = spvReflectEnumerateDescriptorSets(&reflector, &setCount, sets.data());
			error |= SPV_ASSERT(result);

			// For each set in the system
			for (SpvReflectDescriptorSet* set : sets)
			{
				// For each binding in the set
				for (uint32_t i_bind = 0; i_bind < set->binding_count; ++i_bind)
				{
					SpvReflectDescriptorBinding* binding = set->bindings[i_bind];

					UniformInfo info = {};

					bool needBlockSize = false;

					uint32_t length = 1;
					for (uint32_t i_dim = 0; i_dim < binding->array.dims_count; ++i_dim)
					{
						length *= binding->array.dims[i_dim];
					}

					// Get the details about the binding descriptor.
					info.type = static_cast<VkDescriptorType>(binding->descriptor_type);
					info.binding = binding->binding;
					info.arrayLength = length;
					info.stages = static_cast<VkShaderStageFlagBits>(reflector.shader_stage);
					info.size = binding->block.size;
					info.name = binding->name;

					if (uniformInfos[set->set].find(binding->binding) != uniformInfos[set->set].end())
					{
						if (uniformInfos[set->set][binding->binding].type != info.type)
						{
							error |= ErrCode::UNIFORM_MISMATCH;
						}
						else
						{
							uniformInfos[set->set][binding->binding].stages |= info.stages;
						}
					}
					else
					{
						uniformInfos[set->set][binding->binding] = info;
					}
				}
			}

			// Process Vertex input
			if (stage.stage == VK_SHADER_STAGE_VERTEX_BIT)
			{

				uint32_t iv_count = 0;
				result = spvReflectEnumerateInputVariables(&reflector, &iv_count, nullptr);
				error |= SPV_ASSERT(result);

				if (iv_count)
				{
					vector<SpvReflectInterfaceVariable*> input_vars;
					input_vars.resize(iv_count);

					result = spvReflectEnumerateInputVariables(&reflector, &iv_count, input_vars.data());
					error |= SPV_ASSERT(result);

					for (uint32_t j = 0; j < iv_count; j++)
					{
						if (input_vars[j] && input_vars[j]->decoration_flags == 0)
						{ // regular input
							vertexInput.A_POSITION =
								strcmp(input_vars[j]->name, "A_POSITION") ? vertexInput.A_POSITION : input_vars[j]->location;
							vertexInput.A_NORMAL = strcmp(input_vars[j]->name, "A_NORMAL") ? vertexInput.A_NORMAL
																						   : input_vars[j]->location;
							vertexInput.A_UV0 =
								strcmp(input_vars[j]->name, "A_UV0") ? vertexInput.A_UV0 : input_vars[j]->location;
							vertexInput.A_UV1 =
								strcmp(input_vars[j]->name, "A_UV1") ? vertexInput.A_UV1 : input_vars[j]->location;
						}
					}
				}
			}

			// Process Fragment Input
			if (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
			{

				uint32_t ov_count = 0;
				result = spvReflectEnumerateOutputVariables(&reflector, &ov_count, nullptr);
				error |= SPV_ASSERT(result);

				if (ov_count)
				{
					vector<SpvReflectInterfaceVariable*> output_vars;
					output_vars.resize(ov_count);

					result = spvReflectEnumerateOutputVariables(&reflector, &ov_count, output_vars.data());
					error |= SPV_ASSERT(result);

					for (uint32_t j = 0; j < ov_count; j++)
					{
						if (output_vars[j])
						{
							fragmentOutputs = max(fragmentOutputs, output_vars[j]->location + 1);
						}
					}
				}
			}
		}

		// Process Push Constants
		uint32_t pc_count = 0;
		result = spvReflectEnumeratePushConstantBlocks(&reflector, &pc_count, nullptr);
		error |= SPV_ASSERT(result);

		if (pc_count > 1)
		{
			error |= ErrCode::UNUNIFIED_PUSH_CONST;
		}
		else if (pc_count)
		{
			SpvReflectBlockVariable* pconstants;
			result = spvReflectEnumeratePushConstantBlocks(&reflector, &pc_count, &pconstants);
			error |= SPV_ASSERT(result);

			if (pconstants->offset != 0 || (pushConst.size != 0 && pushConst.size != pconstants->size))
			{
				error |= ErrCode::UNUNIFIED_PUSH_CONST;
			}
			else
			{
				pushConst.size = pconstants->size;
				pushConst.stage |= stage.stage;
			}
		}

		spvReflectDestroyShaderModule(&reflector);
	}

	if (error)
	{
		throw runtime_error("ERR: Shader creation failed with " + to_string(error));
	}

	// Create the shader modules
	vector<vkw::ShaderModule> shaderModules;
	vector<VkPipelineShaderStageCreateInfo> pipelineStagesCI;
	shaderModules.reserve(stages.size());
	pipelineStagesCI.reserve(stages.size());

	for (auto& stage : stages)
	{
		auto& shaderModule = shaderModules.emplace_back(util::createShaderModule(device, stage.spirv), device);

		VkPipelineShaderStageCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.stage = stage.stage;
		createInfo.module = shaderModule.get();
		createInfo.pName = "main";
		createInfo.pSpecializationInfo = nullptr;
		pipelineStagesCI.push_back(createInfo);
	}

	// Collapse nested maps to a vector
	vector<Shader::Set> descriptorSetLayouts;
	vector<SetFormatID> setFormatKeys;
	descriptorSetLayouts.reserve(uniformInfos.size());
	setFormatKeys.reserve(uniformInfos.size());

	for (auto& [set, map] : uniformInfos)
	{
		auto& lay = descriptorSetLayouts.emplace_back();
		lay.uniforms.reserve(map.size());

		lay.set = set;
		vector<VkDescriptorSetLayoutBinding> binds;
		for (auto& [key, val] : map)
		{
			lay.uniforms.push_back(val);
			binds.push_back(static_cast<VkDescriptorSetLayoutBinding>(val));
		}
		lay.layout = vkw::DescriptorSetLayout(util::createDescriptorSetLayout(device, binds), device);

		setFormatKeys.push_back(getFormatKey(lay.uniforms));
	}

	// Shader structure creation
	Shader shader = {};
	shader.pushConstant = pushConst;
	shader.isCompute = isCompute;
	shader.fragmentOutputs = fragmentOutputs;
	shader.vertexInputFormat = vertexInput;
	shader.pipelineLayout = createPipelineLayout(descriptorSetLayouts, pushConst);
	shader.sets = std::move(descriptorSetLayouts);
	shader.setFormats = std::move(setFormatKeys);
	shader.shaderModules = std::move(shaderModules);
	shader.pipelineStages = std::move(pipelineStagesCI);

	for (auto& dset : shader.sets)
	{
		for (auto& uniform : dset.uniforms)
		{
			shader.uniformLocations[uniform.name] = {dset.set, uniform.binding};
		}
	}

	return std::move(shader);
}

vkw::PipelineLayout PipelineFactory::createPipelineLayout(const std::vector<Shader::Set>& dsl,
														  const Shader::PushConstant& pushConst) const
{
	using namespace std;

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	descriptorSetLayouts.reserve(dsl.size());
	for (auto& s : dsl)
	{
		descriptorSetLayouts.push_back(s.layout.get());
	}

	VkPushConstantRange pcr = {
		pushConst.stage,
		0,
		pushConst.size,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(dsl.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = pcr.size ? 1u : 0u;
	pipelineLayoutCreateInfo.pPushConstantRanges = pcr.size ? &pcr : nullptr;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	auto result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Pipeline Layout Creation Failed with " + result);
	}
	return vkw::PipelineLayout(pipelineLayout, device);
}

Pipeline PipelineFactory::createGraphicsPipeline(const Shader& shader, const RenderPass& renderPass,
												 const GraphicsPipelineCreateInfo& createInfo)
{
	if (shader.isCompute)
	{
		throw std::invalid_argument("ERR: Trying to create a Rendering Pipeline from a Compute Shader");
	}

	VkVertexInputBindingDescription vertexBindDescription = Vertex::getBindingDescription();
	std::vector<VkVertexInputAttributeDescription> vertexAttrDescription(
		std::move(Vertex::getAttributeDescriptions(shader.vertexInputFormat)));

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &vertexBindDescription;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttrDescription.size());
	vertexInputCreateInfo.pVertexAttributeDescriptions = vertexAttrDescription.data();

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = nullptr;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = nullptr;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shader.pipelineStages.size());
	pipelineCreateInfo.pStages = shader.pipelineStages.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &createInfo.inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &createInfo.rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &createInfo.multisampleCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &createInfo.depthStencilCreateInfo;
	pipelineCreateInfo.pColorBlendState = &createInfo.colorblendCreateInfo;
	pipelineCreateInfo.pDynamicState = &createInfo.dynamicStateCreateInfo;
	pipelineCreateInfo.layout = shader.pipelineLayout.get();
	pipelineCreateInfo.renderPass = renderPass.renderPass.get();
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Graphics Pipeline creation failed with " + std::to_string(result));
	}
	Pipeline pipe = {};
	pipe.pipeline = vkw::Pipeline(graphicsPipeline, device);
	pipe.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	return pipe;
}

PipelineFactory::SetFormatID PipelineFactory::getFormatKey(const std::vector<UniformInfo>& uniforms)
{
	if (uniforms.size())
	{
		SetFormat format = {uniforms};
		auto it = setFormatRegistry.find(format);
		if (it != setFormatRegistry.end())
		{
			return it->second;
		}
		else
		{
			return setFormatRegistry[format] = static_cast<SetFormatID>(setFormatRegistry.size() + 1);
		}
	}
	else
	{
		return 0;
	}
}

PipelineFactory::FBFormatID PipelineFactory::getFormatKey(const std::vector<AttachmentFormat>& attachments)
{
	if (attachments.size())
	{
		FBFormat format = {attachments};
		auto it = fbFormatRegistry.find(format);
		if (it != fbFormatRegistry.end())
		{
			return it->second;
		}
		else
		{
			return fbFormatRegistry[format] = static_cast<SetFormatID>(fbFormatRegistry.size() + 1);
		}
	}
	else
	{
		return 0;
	}
}

RenderPass PipelineFactory::createRenderPass(const std::vector<AttachmentFormat>& formats,
											 const std::vector<VkSubpassDescription>& subpasses, LoadStoreConfig config,
											 const VkRenderPassMultiviewCreateInfo* multiview)
{
	std::vector<VkAttachmentDescription> attachmentDescriptions;
	for (auto& format : formats)
	{
		if ((format.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) == 0)
		{
			throw std::invalid_argument("Format Usage " + std::to_string(format.usage) + " is not supported");
		}
		bool isSampled = format.usage & VK_IMAGE_USAGE_SAMPLED_BIT;
		bool isDepthStencil = format.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		bool isStorage = format.usage & VK_IMAGE_USAGE_STORAGE_BIT;

		auto& description = attachmentDescriptions.emplace_back();
		description.flags = 0;
		description.format = format.format;
		description.samples = format.sampleCount;

		switch (isDepthStencil ? config.depthLoad : config.colorLoad)
		{
		case LoadStoreConfig::LoadAction::CLEAR: {
			description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		};
		break;
		case LoadStoreConfig::LoadAction::CONTINUE: {
			if (isDepthStencil)
			{
				description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				description.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		};
		break;
		case LoadStoreConfig::LoadAction::DONT_CARE: {
			if (isDepthStencil)
			{
				description.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				description.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.initialLayout =
					(isSampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
							   : (isStorage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			}
		};
		break;
		case LoadStoreConfig::LoadAction::READ: {
			if (isDepthStencil)
			{
				description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				description.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
				description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				description.initialLayout =
					(isSampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
							   : (isStorage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			}
		};
		break;
		}

		switch (isDepthStencil ? config.depthStore : config.colorStore)
		{
		case LoadStoreConfig::StoreAction::CONTINUE: {
			if (isDepthStencil)
			{
				description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
				description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
				description.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			}
		};
		break;
		case LoadStoreConfig::StoreAction::DONT_CARE: {
			if (isDepthStencil)
			{
				description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				description.initialLayout =
					(isSampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
							   : (isStorage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			}
		};
		break;
		case LoadStoreConfig::StoreAction::READ: {
			if (isDepthStencil)
			{
				description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
				description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				description.finalLayout =
					(isSampled ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
							   : (isStorage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
			}
		};
		break;
		}
	}

	if (multiview != nullptr && multiview->subpassCount != static_cast<uint32_t>(subpasses.size()))
	{
		throw std::invalid_argument(
			"Number of subpasses in the RenderPass must match the subpassCount of the multiview");
	}

	VkRenderPassCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.pNext = multiview;
	createInfo.flags = 0;
	createInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	createInfo.pAttachments = attachmentDescriptions.data();
	createInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	createInfo.pSubpasses = subpasses.data();
	createInfo.dependencyCount;
	createInfo.pDependencies;

	VkRenderPass renderPass;
	auto result = vkCreateRenderPass(device, &createInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("RenderPass creation failed with " + std::to_string(result));
	}
	RenderPass rp = {};
	rp.fbFormat = getFormatKey(formats);
	rp.renderPass = vkw::RenderPass(renderPass, device);

	return rp;
}

vkw::DescriptorPool PipelineFactory::createDescriptorPool(const Shader::Set& set, uint32_t maxSets)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (auto& uniform : set.uniforms)
	{
		bool found = false;
		for (auto& poolSize : poolSizes)
		{
			if (poolSize.type == uniform.type)
			{
				poolSize.descriptorCount++;
				found = true;
				break;
			}
		}
		if (!found)
		{
			auto& ps = poolSizes.emplace_back();
			ps.type = uniform.type;
			ps.descriptorCount = 1;
		}
	}
	for (auto& ps : poolSizes)
	{
		ps.descriptorCount *= maxSets;
	}

	return vkw::DescriptorPool(
		util::createDescriptorPool(device, poolSizes, static_cast<uint32_t>(maxSets)), device);
}

SetVector PipelineFactory::createSets(const Shader::Set& set, uint32_t count)
{
	auto pool = createDescriptorPool(set, count);
	std::vector<VkDescriptorSetLayout> layouts(count, set.layout.get());
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool.get();
	allocInfo.descriptorSetCount = count;
	allocInfo.pSetLayouts = layouts.data();

	std::vector<VkDescriptorSet> descriptorSets(count);
	auto result = vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
	}

	SetVector retVal;
	retVal.formatID = getFormatKey(set.uniforms);
	retVal.pool = std::move(pool);
	retVal.sets = vkw::DescriptorSetVector(std::move(descriptorSets));
	retVal.setIdx = set.set;

	return retVal;
}

SetSingleton PipelineFactory::createSet(const Shader::Set& set)
{
	auto pool = createDescriptorPool(set, 1);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool.get();
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &set.layout.get();

	VkDescriptorSet descriptorSet;
	auto result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
	}

	SetSingleton retVal;
	retVal.formatID = getFormatKey(set.uniforms);
	retVal.pool = std::move(pool);
	retVal.set = vkw::DescriptorSet(descriptorSet);
	retVal.setIdx = set.set;

	return retVal;
}
} // namespace blaze::spirv