
#include "Bloom.hpp"
#include <util/files.hpp>
#include <gui/GUI.hpp>

namespace blaze
{

Bloom::Bloom(const Context* context, Texture2D* colorOutput)
{
	using namespace spirv;

	ImageData2D id2d = {};
	id2d.height = colorOutput->get_height() / 2;
	id2d.width = colorOutput->get_width() / 2;
	id2d.numChannels = 4;
	id2d.anisotropy = VK_FALSE;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	id2d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	id2d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	id2d.access = VK_ACCESS_SHADER_READ_BIT;
	id2d.format = colorOutput->get_format();

	for (int i = 0; i < 2; i++)
	{
		pingPongAttachment[i] = Texture2D(context, id2d, false);
	}

	AttachmentFormat attachmentFormat = {};
	attachmentFormat.format = id2d.format;
	attachmentFormat.usage = id2d.usage;
	attachmentFormat.sampleCount = VK_SAMPLE_COUNT_1_BIT;
	attachmentFormat.loadStoreConfig =
		LoadStoreConfig(LoadStoreConfig::LoadAction::DONT_CARE, LoadStoreConfig::StoreAction::READ);
		
	VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorRef;
	subpassDesc.pDepthStencilAttachment = nullptr;

	renderpass = context->get_pipelineFactory()->createRenderPass({attachmentFormat}, {subpassDesc});
	VkClearValue clear = {};
	clear.color = {0.0f, 0.0f, 0.0f, 1.0f};
	renderpass.clearValues = {clear};

	for (int i = 0; i < 2; i++)
	{
		pingPong[i] = context->get_pipelineFactory()->createFramebuffer(renderpass, pingPongAttachment[i].get_extent(),
																		{pingPongAttachment[i].get_imageView()});
	}

	outputFB = context->get_pipelineFactory()->createFramebuffer(renderpass, colorOutput->get_extent(),
																 {colorOutput->get_imageView()});

	std::vector<ShaderStageData> stages(2);
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

	stages[0].spirv = util::loadBinaryFile(vHighpassShaderFile);
	stages[1].spirv = util::loadBinaryFile(fHighpassShaderFile);
	highpassShader = context->get_pipelineFactory()->createShader(stages);

	stages[0].spirv = util::loadBinaryFile(vBloomShaderFile);
	stages[1].spirv = util::loadBinaryFile(fBloomShaderFile);
	bloomShader = context->get_pipelineFactory()->createShader(stages);

	stages[0].spirv = util::loadBinaryFile(vCombineShaderFile);
	stages[1].spirv = util::loadBinaryFile(fCombineShaderFile);
	combineShader = context->get_pipelineFactory()->createShader(stages);

	spirv::GraphicsPipelineCreateInfo info = {};
	info.inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.inputAssemblyCreateInfo.flags = 0;
	info.inputAssemblyCreateInfo.pNext = nullptr;
	info.inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	info.inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	info.rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	info.rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	info.rasterizerCreateInfo.lineWidth = 1.0f;
	info.rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	info.rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	info.rasterizerCreateInfo.depthBiasEnable = VK_TRUE;
	info.rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	info.rasterizerCreateInfo.pNext = nullptr;
	info.rasterizerCreateInfo.flags = 0;

	info.multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	info.multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorblendAttachment = {};
	colorblendAttachment.blendEnable = VK_FALSE;
	colorblendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorblendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorblendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorblendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	info.colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorblendCreateInfo.logicOpEnable = VK_FALSE;
	info.colorblendCreateInfo.attachmentCount = 1;
	info.colorblendCreateInfo.pAttachments = &colorblendAttachment;

	info.depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilCreateInfo.depthTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
	info.depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_ALWAYS;
	info.depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.maxDepthBounds = 0.0f; // Don't care
	info.depthStencilCreateInfo.minDepthBounds = 1.0f; // Don't care
	info.depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
	info.depthStencilCreateInfo.front = {}; // Don't Care
	info.depthStencilCreateInfo.back = {};	// Don't Care

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	info.dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	info.dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	info.dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();


	highpassPipeline = context->get_pipelineFactory()->createGraphicsPipeline(highpassShader, renderpass, info);
	bloomPipeline = context->get_pipelineFactory()->createGraphicsPipeline(bloomShader, renderpass, info);

	colorblendAttachment.blendEnable = VK_TRUE;
	combinePipeline = context->get_pipelineFactory()->createGraphicsPipeline(combineShader, renderpass, info);

	std::array<VkWriteDescriptorSet, 3> writes;
	for (int i = 0; i < 2; i++)
	{
		attachSets[i] = context->get_pipelineFactory()->createSet(*bloomShader.getSetWithUniform("I_COLOR"));

		const spirv::UniformInfo* unif = bloomShader.getUniform("I_COLOR");

		writes[i] = {};
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].descriptorType = unif->type;
		writes[i].descriptorCount = unif->arrayLength;
		writes[i].dstSet = attachSets[i].get();
		writes[i].dstBinding = unif->binding;
		writes[i].dstArrayElement = 0;
		writes[i].pImageInfo = &pingPongAttachment[1 - i].get_imageInfo();
	}

	inputSet = context->get_pipelineFactory()->createSet(*highpassShader.getSetWithUniform("I_COLOR"));

	const spirv::UniformInfo* unif = highpassShader.getUniform("I_COLOR");

	writes[2] = {};
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].descriptorType = unif->type;
	writes[2].descriptorCount = unif->arrayLength;
	writes[2].dstSet = inputSet.get();
	writes[2].dstBinding = unif->binding;
	writes[2].dstArrayElement = 0;
	writes[2].pImageInfo = &colorOutput->get_imageInfo();

	vkUpdateDescriptorSets(context->get_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	VkExtent2D extent = colorOutput->get_extent();

	viewport = createViewport(extent);

	extent.height /= 2;
	extent.width /= 2;

	halfViewport = createViewport(extent);
}

void Bloom::drawSettings()
{
	if (ImGui::CollapsingHeader("Bloom##BloomSettings"))
	{
		ImGui::Checkbox("Enabled##Bloom", &enabled);
		if (!enabled)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		ImGui::DragInt("Blur Iterations##Bloom", &iterations, 0.5f, 1, 10);
		ImGui::DragFloat("Highpass Threshold##Bloom", &highpassSettings.threshold, 0.1f, 0.1f, 5.0f);
		ImGui::DragFloat("Highpass Tolerance##Bloom", &highpassSettings.tolerance, 0.01f, 0.0f, 0.5f);
		
		if (!enabled)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
	}
}

void Bloom::recreate(const Context* context, Texture2D* colorOutput)
{
	using namespace spirv;

	ImageData2D id2d = {};
	id2d.height = colorOutput->get_height() / 2;
	id2d.width = colorOutput->get_width() / 2;
	id2d.numChannels = 4;
	id2d.anisotropy = VK_FALSE;
	id2d.samplerAddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	id2d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	id2d.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	id2d.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	id2d.access = VK_ACCESS_SHADER_READ_BIT;
	id2d.format = colorOutput->get_format();

	for (int i = 0; i < 2; i++)
	{
		pingPongAttachment[i] = Texture2D(context, id2d, false);
		pingPong[i] = context->get_pipelineFactory()->createFramebuffer(renderpass, pingPongAttachment[i].get_extent(),
																		{pingPongAttachment[i].get_imageView()});
	}

	outputFB = context->get_pipelineFactory()->createFramebuffer(renderpass, colorOutput->get_extent(),
																 {colorOutput->get_imageView()});

	std::array<VkWriteDescriptorSet, 3> writes;
	for (int i = 0; i < 2; i++)
	{
		attachSets[i] = context->get_pipelineFactory()->createSet(*bloomShader.getSetWithUniform("I_COLOR"));

		const spirv::UniformInfo* unif = bloomShader.getUniform("I_COLOR");

		writes[i] = {};
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].descriptorType = unif->type;
		writes[i].descriptorCount = unif->arrayLength;
		writes[i].dstSet = attachSets[i].get();
		writes[i].dstBinding = unif->binding;
		writes[i].dstArrayElement = 0;
		writes[i].pImageInfo = &pingPongAttachment[1 - i].get_imageInfo();
	}

	inputSet = context->get_pipelineFactory()->createSet(*highpassShader.getSetWithUniform("I_COLOR"));

	const spirv::UniformInfo* unif = highpassShader.getUniform("I_COLOR");
	
	writes[2] = {};
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].descriptorType = unif->type;
	writes[2].descriptorCount = unif->arrayLength;
	writes[2].dstSet = inputSet.get();
	writes[2].dstBinding = unif->binding;
	writes[2].dstArrayElement = 0;
	writes[2].pImageInfo = &colorOutput->get_imageInfo();

	vkUpdateDescriptorSets(context->get_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	VkExtent2D extent = colorOutput->get_extent();

	viewport = createViewport(extent);

	extent.height /= 2;
	extent.width /= 2;

	halfViewport = createViewport(extent);
}

void Bloom::process(VkCommandBuffer cmd, IndexedVertexBuffer<Vertex>& quad)
{
	renderpass.begin(cmd, pingPong[0]);
	vkCmdSetViewport(cmd, 0, 1, &halfViewport);
	vkCmdSetScissor(cmd, 0, 1, &pingPong[0].renderArea);

	highpassPipeline.bind(cmd);
	vkCmdPushConstants(cmd, highpassShader.pipelineLayout.get(), highpassShader.pushConstant.stage, 0,
					   highpassShader.pushConstant.size, &highpassSettings);
	vkCmdBindDescriptorSets(cmd, highpassPipeline.bindPoint, highpassShader.pipelineLayout.get(), inputSet.setIdx, 1,
							&inputSet.get(), 0, nullptr);
	quad.bind(cmd);
	vkCmdDrawIndexed(cmd, quad.get_indexCount(), 1, 0, 0, 0);

	renderpass.end(cmd);

	int vertical = 0;

	for (int i = 0; i < iterations; i++)
	{
		vertical = 0;
		renderpass.begin(cmd, pingPong[1]);
		bloomPipeline.bind(cmd);
		vkCmdBindDescriptorSets(cmd, bloomPipeline.bindPoint, bloomShader.pipelineLayout.get(),
								attachSets[1].setIdx, 1, &attachSets[1].get(), 0, nullptr);
		vkCmdPushConstants(cmd, bloomShader.pipelineLayout.get(), bloomShader.pushConstant.stage, 0,
						   bloomShader.pushConstant.size, &vertical);
		quad.bind(cmd);
		vkCmdDrawIndexed(cmd, quad.get_indexCount(), 1, 0, 0, 0);
		renderpass.end(cmd);

		vertical = 1;
		renderpass.begin(cmd, pingPong[0]);
		bloomPipeline.bind(cmd);
		vkCmdBindDescriptorSets(cmd, bloomPipeline.bindPoint, bloomShader.pipelineLayout.get(), attachSets[0].setIdx, 1,
								&attachSets[0].get(), 0, nullptr);
		vkCmdPushConstants(cmd, bloomShader.pipelineLayout.get(), bloomShader.pushConstant.stage, 0,
						   bloomShader.pushConstant.size, &vertical);
		quad.bind(cmd);
		vkCmdDrawIndexed(cmd, quad.get_indexCount(), 1, 0, 0, 0);
		renderpass.end(cmd);
	}

	renderpass.begin(cmd, outputFB);
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &outputFB.renderArea);

	combinePipeline.bind(cmd);
	vkCmdBindDescriptorSets(cmd, combinePipeline.bindPoint, combineShader.pipelineLayout.get(), attachSets[0].setIdx, 1,
							&attachSets[0].get(), 0, nullptr);
	quad.bind(cmd);
	vkCmdDrawIndexed(cmd, quad.get_indexCount(), 1, 0, 0, 0);

	renderpass.end(cmd);
}

} // namespace blaze
