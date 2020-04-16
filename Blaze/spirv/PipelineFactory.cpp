
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

	uint32_t vertexInputs = 0;
	uint32_t fragmentOutputs = 0;

	map<uint32_t, map<uint32_t, UniformInfo>> uniformInfos;
	Shader::PushConstant pushConst = {0, 0};

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
							vertexInputs |= (1 << uint32_t(input_vars[j]->location));
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

			if (pconstants->offset != 0 || pushConst.size != pconstants->size)
			{
				error |= ErrCode::UNUNIFIED_PUSH_CONST;
			}
			else
			{
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
	vector<SetFormatKey> setFormatKeys;
	descriptorSetLayouts.reserve(uniformInfos.size());
	setFormatKeys.reserve(uniformInfos.size());

	for (auto& mui : uniformInfos)
	{
		auto& uniformMap = mui.second;
		auto& lay = descriptorSetLayouts.emplace_back();
		lay.uniforms.reserve(uniformMap.size());

		lay.set = mui.first;
		vector<VkDescriptorSetLayoutBinding> binds;
		for (auto& kv : uniformMap)
		{
			lay.uniforms.push_back(kv.second);
			binds.push_back(static_cast<VkDescriptorSetLayoutBinding>(kv.second));
		}
		lay.layout = vkw::DescriptorSetLayout(util::createDescriptorSetLayout(device, binds), device);

		setFormatKeys.push_back(getFormatKey(lay.uniforms));
	}

	// Shader structure creation
	Shader shader = {};
	shader.pushConstant = pushConst;
	shader.isCompute = isCompute;
	shader.fragmentOutputs = fragmentOutputs;
	shader.vertexInputMask = vertexInputs;
	shader.sets = std::move(descriptorSetLayouts);
	shader.pipelineLayout = createPipelineLayout(descriptorSetLayouts, pushConst);
	shader.setFormats = std::move(setFormatKeys);
	shader.shaderModules = std::move(shaderModules);
	shader.pipelineStages = std::move(pipelineStagesCI);

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
		0,
		pushConst.size,
		pushConst.stage,
	};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(dsl.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1u;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pcr;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	auto result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Pipeline Layout Creation Failed with " + result);
	}
	return vkw::PipelineLayout(pipelineLayout, device);
}

PipelineFactory::SetFormatKey PipelineFactory::getFormatKey(const std::vector<UniformInfo>& uniforms)
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
			return setFormatRegistry[format] = static_cast<SetFormatKey>(setFormatRegistry.size() + 1);
		}
	}
	else
	{
		return 0;
	}
}
} // namespace blaze::spirv