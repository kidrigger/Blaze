
#include "TextureCube.hpp"
#include "Texture2D.hpp"
#include "util/files.hpp"
#include "Primitives.hpp"

namespace blaze {

	TextureCube convertEqvrectToCubemap(const Context& context, VkDescriptorSet environment, VkDescriptorSetLayout dsLayout)
	{
		const uint32_t dim = 512;

		util::Managed<VkPipelineLayout> irPipelineLayout;
		util::Managed<VkPipeline> irPipeline;
		util::Managed<VkRenderPass> irRenderPass;
		util::Managed<VkFramebuffer> irFramebuffer;

		VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

		// Setup the TextureCube
		ImageDataCube idc{};
		idc.height = dim;
		idc.width = dim;
		idc.numChannels = 4;
		idc.size = 4 * 6 * dim * dim;
		idc.layerSize = 4 * dim * dim;
		idc.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		idc.format = format;
		idc.access = VK_ACCESS_TRANSFER_WRITE_BIT;
		TextureCube irradianceMap(context, idc, true);

		ImageData2D id2d{};
		id2d.height = dim;
		id2d.width = dim;
		id2d.numChannels = 4;
		id2d.size = 4 * dim * dim;
		id2d.format = format;
		id2d.usage = id2d.usage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		id2d.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		id2d.access = VK_ACCESS_SHADER_WRITE_BIT;
		Texture2D fbColorAttachment(context, id2d, true);

		struct IrradiancePushConstantBlock
		{
			glm::mat4 mvp;
			float deltaPhi;
			float deltaTheta;
		};

		{
			std::vector<VkDescriptorSetLayout> descriptorSetLayouts = {
				dsLayout
			};

			std::vector<VkPushConstantRange> pushConstantRanges;
			{
				VkPushConstantRange pushConstantRange = {};
				pushConstantRange.offset = 0;
				pushConstantRange.size = sizeof(glm::mat4);
				pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
				pushConstantRanges.push_back(pushConstantRange);
				pushConstantRange.offset = sizeof(glm::mat4);
				pushConstantRange.size = 2 * sizeof(float);
				pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
				pushConstantRanges.push_back(pushConstantRange);
			}

			VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
			pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
			pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
			pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
			pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

			VkPipelineLayout tPipelineLayout = VK_NULL_HANDLE;
			auto result = vkCreatePipelineLayout(context.get_device(), &pipelineLayoutCreateInfo, nullptr, &tPipelineLayout);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Pipeline Layout Creation Failed with " + result);
			}
			irPipelineLayout = util::Managed(tPipelineLayout, [dev = context.get_device()](VkPipelineLayout& lay){vkDestroyPipelineLayout(dev, lay, nullptr); });
		}

		{
			VkRenderPass rpass = VK_NULL_HANDLE;

			VkAttachmentDescription colorAttachment = {};
			colorAttachment.format = fbColorAttachment.get_format();
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkAttachmentReference colorAttachmentRef = {};
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpassDesc = {};
			subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDesc.colorAttachmentCount = 1;
			subpassDesc.pColorAttachments = &colorAttachmentRef;
			subpassDesc.pDepthStencilAttachment = nullptr;

			VkSubpassDependency dependency = {};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			std::array<VkAttachmentDescription, 1> attachments = {
				colorAttachment
			};
			VkRenderPassCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			createInfo.pAttachments = attachments.data();
			createInfo.subpassCount = 1;
			createInfo.pSubpasses = &subpassDesc;
			createInfo.dependencyCount = 1;
			createInfo.pDependencies = &dependency;

			auto result = vkCreateRenderPass(context.get_device(), &createInfo, nullptr, &rpass);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("RenderPass creation failed with " + std::to_string(result));
			}
			irRenderPass = util::Managed(rpass, [dev = context.get_device()](VkRenderPass& pass) { vkDestroyRenderPass(dev, pass, nullptr); });
		}

		{
			auto vertexShaderCode = util::loadBinaryFile("shaders/vIrradiance.vert.spv");
			auto fragmentShaderCode = util::loadBinaryFile("shaders/fEqvrect2Cube.frag.spv");

			auto vertexShaderModule = util::Managed(util::createShaderModule(context.get_device(), vertexShaderCode), [dev = context.get_device()](VkShaderModule& sm) { vkDestroyShaderModule(dev, sm, nullptr); });
			auto fragmentShaderModule = util::Managed(util::createShaderModule(context.get_device(), fragmentShaderCode), [dev = context.get_device()](VkShaderModule& sm) { vkDestroyShaderModule(dev, sm, nullptr); });

			VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
			vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertexShaderStageCreateInfo.module = vertexShaderModule.get();
			vertexShaderStageCreateInfo.pName = "main";

			VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
			fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragmentShaderStageCreateInfo.module = fragmentShaderModule.get();
			fragmentShaderStageCreateInfo.pName = "main";

			std::vector<VkPipelineShaderStageCreateInfo> shaderStagesCreateInfo = {
				vertexShaderStageCreateInfo,
				fragmentShaderStageCreateInfo
			};

			auto bindingDescriptions = Vertex::getBindingDescription();
			auto attributeDescriptions = Vertex::getAttributeDescriptions();

			VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
			vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
			vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescriptions;
			vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
			vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
			inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

			VkViewport viewport = {};
			viewport.x = 0.0f;
			viewport.y = static_cast<float>(dim);
			viewport.width = static_cast<float>(dim);
			viewport.height = -static_cast<float>(dim);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor = {};
			scissor.offset = { 0, 0 };
			scissor.extent = { dim, dim };

			VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
			viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateCreateInfo.viewportCount = 1;
			viewportStateCreateInfo.pViewports = &viewport;
			viewportStateCreateInfo.scissorCount = 1;
			viewportStateCreateInfo.pScissors = &scissor;

			VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
			rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizerCreateInfo.depthClampEnable = VK_FALSE;
			rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
			rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
			rasterizerCreateInfo.lineWidth = 1.0f;
			rasterizerCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
			rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
			rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

			VkPipelineMultisampleStateCreateInfo multisampleCreateInfo = {};
			multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
			multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

			VkPipelineColorBlendAttachmentState colorblendAttachment = {};
			colorblendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			colorblendAttachment.blendEnable = VK_FALSE;

			VkPipelineColorBlendStateCreateInfo colorblendCreateInfo = {};
			colorblendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorblendCreateInfo.logicOpEnable = VK_FALSE;
			colorblendCreateInfo.attachmentCount = 1;
			colorblendCreateInfo.pAttachments = &colorblendAttachment;

			VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
			depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
			depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
			depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
			depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
			depthStencilStateCreateInfo.maxDepthBounds = 0.0f;	// Don't care
			depthStencilStateCreateInfo.minDepthBounds = 1.0f;	// Don't care
			depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
			depthStencilStateCreateInfo.front = {}; // Don't Care
			depthStencilStateCreateInfo.back = {}; // Don't Care

			VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
			dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicStateCreateInfo.dynamicStateCount = 0;
			dynamicStateCreateInfo.pDynamicStates = nullptr;

			VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStagesCreateInfo.size());
			pipelineCreateInfo.pStages = shaderStagesCreateInfo.data();
			pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
			pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
			pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
			pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
			pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
			pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
			pipelineCreateInfo.pColorBlendState = &colorblendCreateInfo;
			pipelineCreateInfo.pDynamicState = nullptr;
			pipelineCreateInfo.layout = irPipelineLayout.get();
			pipelineCreateInfo.renderPass = irRenderPass.get();
			pipelineCreateInfo.subpass = 0;
			pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			pipelineCreateInfo.basePipelineIndex = -1;

			VkPipeline tPipeline = VK_NULL_HANDLE;
			auto result = vkCreateGraphicsPipelines(context.get_device(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &tPipeline);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Graphics Pipeline creation failed with " + std::to_string(result));
			}
			irPipeline = util::Managed(tPipeline, [dev = context.get_device()](VkPipeline& pipe) { vkDestroyPipeline(dev, pipe, nullptr); });
		}

		{
			VkFramebuffer fbo = VK_NULL_HANDLE;
			VkFramebufferCreateInfo fbCreateInfo = {};
			fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			fbCreateInfo.width = dim;
			fbCreateInfo.height = dim;
			fbCreateInfo.layers = 1;
			fbCreateInfo.renderPass = irRenderPass.get();
			fbCreateInfo.attachmentCount = 1;
			fbCreateInfo.pAttachments = &fbColorAttachment.get_imageView();
			vkCreateFramebuffer(context.get_device(), &fbCreateInfo, nullptr, &fbo);
			irFramebuffer = util::Managed(fbo, [dev = context.get_device()](VkFramebuffer& fbo) { vkDestroyFramebuffer(dev, fbo, nullptr); });
		}

		auto cube = getUVCube(context);

		auto cmdBuffer = context.startCommandBufferRecord();

		// RENDERPASSES

		VkRenderPassBeginInfo renderpassBeginInfo = {};
		renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderpassBeginInfo.renderPass = irRenderPass.get();
		renderpassBeginInfo.framebuffer = irFramebuffer.get();
		renderpassBeginInfo.renderArea.offset = { 0, 0 };
		renderpassBeginInfo.renderArea.extent = { dim, dim };

		std::array<VkClearValue, 1> clearColor;
		clearColor[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderpassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColor.size());
		renderpassBeginInfo.pClearValues = clearColor.data();

		glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 512.0f);

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X (Outside in - so NEG_X face)
			glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// NEGATIVE_X (Outside in - so POS_X face)
			glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// POSITIVE_Y
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
			// NEGATIVE_Y
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			// POSITIVE_Z
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
			// NEGATIVE_Z
			glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
		};

		IrradiancePushConstantBlock pcb{};
		pcb.deltaPhi = 0.1f;
		pcb.deltaTheta = 0.025f;

		for (int face = 0; face < 6; face++)
		{
			vkCmdBeginRenderPass(cmdBuffer, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipeline.get());

			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, irPipelineLayout.get(), 0, 1, &environment, 0, nullptr);

			pcb.mvp = proj * matrices[face];
			vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &pcb.mvp);
			vkCmdPushConstants(cmdBuffer, irPipelineLayout.get(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), 2 * sizeof(float), &pcb.deltaPhi);

			VkDeviceSize offsets = { 0 };

			vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &cube.get_vertexBuffer(), &offsets);
			vkCmdBindIndexBuffer(cmdBuffer, cube.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(cmdBuffer, cube.get_indexCount(), 1, 0, 0, 0);

			vkCmdEndRenderPass(cmdBuffer);

			fbColorAttachment.transferLayout(cmdBuffer,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = face;
			copyRegion.dstSubresource.mipLevel = 0;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = static_cast<uint32_t>(dim);
			copyRegion.extent.height = static_cast<uint32_t>(dim);
			copyRegion.extent.depth = 1;

			vkCmdCopyImage(cmdBuffer,
				fbColorAttachment.get_image(), fbColorAttachment.get_imageInfo().imageLayout,
				irradianceMap.get_image(), irradianceMap.get_imageInfo().imageLayout,
				1, &copyRegion);

			fbColorAttachment.transferLayout(cmdBuffer,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
		}

		irradianceMap.transferLayout(cmdBuffer,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			0, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		context.flushCommandBuffer(cmdBuffer);

		return irradianceMap;
	}

	[[nodiscard]] TextureCube loadImageCube(const Context& context, const std::vector<std::string>& names_lrudfb, bool mipmapped)
	{
		ImageDataCube image;
		int width, height, numChannels;

		int layer = 0;
		for (const auto& name : names_lrudfb)
		{
			image.data[layer] = stbi_load(name.c_str(), &width, &height, &numChannels, STBI_rgb_alpha);

			if (!image.data[layer])
			{
				throw std::runtime_error("Image " + name + " could not be loaded.");
			}

			layer++;
		}
		image.width = width;
		image.height = height;
		image.numChannels = numChannels;
		image.layerSize = 4 * width * height;
		image.size = 6 * image.layerSize;

		auto&& ti = TextureCube(context, image, mipmapped);

		for (int i = 0; i < 6; i++)
		{
			stbi_image_free(image.data[i]);
		}

		return std::move(ti);
	}

	[[nodiscard]] TextureCube loadImageCube(const Context& context, const std::string& name, bool mipmapped)
	{
		auto ext = name.substr(name.find_last_of('.'));
		if (ext != ".hdr")
		{
			throw std::runtime_error("Can't load " + ext + " files.");
		}

		int width, height, numChannels;
		float* data = stbi_loadf(name.c_str(), &width, &height, &numChannels, 0);

		float* data_rgba = new float[size_t(width) * size_t(height) * 4];
		if (numChannels == 3)
		{
			for (size_t i = 0; i < size_t(width) * size_t(height); i++)
			{
				for (size_t c = 0; c < 3; c++)
				{
					data_rgba[4 * i + c] = data[3 * i + c];
				}
				data_rgba[4 * i + 3] = 1.0f;
			}
		}
		else
		{
			memcpy(data_rgba, data, size_t(width) * size_t(height) * 4 * 4);
		}

		stbi_image_free(data);

		ImageData2D eqvData = {};
		eqvData.data = reinterpret_cast<uint8_t*>(data_rgba);
		eqvData.width = width;
		eqvData.height = height;
		eqvData.numChannels = 4;
		eqvData.size = width * height * 4 * sizeof(float);
		eqvData.format = VK_FORMAT_R32G32B32A32_SFLOAT;

		Texture2D equirect(context, eqvData, false);
		delete[] data_rgba;

		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 1;
		std::vector<VkDescriptorPoolSize> poolSizes = { poolSize };
		util::Managed<VkDescriptorPool> dsPool = util::Managed(util::createDescriptorPool(context.get_device(), poolSizes, 2), [dev = context.get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });
		util::Managed<VkDescriptorSetLayout> dsLayout;
		{
			std::vector<VkDescriptorSetLayoutBinding> cubemapLayoutBindings = {
				{
					0,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1,
					VK_SHADER_STAGE_FRAGMENT_BIT,
					nullptr
				}
			};
			dsLayout = util::Managed(util::createDescriptorSetLayout(context.get_device(), cubemapLayoutBindings), [dev = context.get_device()](VkDescriptorSetLayout& dsl){vkDestroyDescriptorSetLayout(dev, dsl, nullptr); });
		}


		auto createDescriptorSet = [device = context.get_device()](VkDescriptorSetLayout layout, VkDescriptorPool pool, const Texture2D& texture, uint32_t binding)
		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = pool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &layout;

			VkDescriptorSet descriptorSet;
			auto result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Descriptor Set allocation failed with " + std::to_string(result));
			}

			VkDescriptorImageInfo info = {};
			info.imageView = texture.get_imageView();
			info.sampler = texture.get_imageSampler();
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.descriptorCount = 1;
			write.dstSet = descriptorSet;
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.pImageInfo = &info;

			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

			return descriptorSet;
		};

		util::Managed<VkDescriptorSet> ds = util::Managed(createDescriptorSet(dsLayout.get(), dsPool.get(), equirect, 1), [dev = context.get_device(), pool = dsPool.get()](VkDescriptorSet& dset) { vkFreeDescriptorSets(dev, pool, 1, &dset); });

		return convertEqvrectToCubemap(context, ds.get(), dsLayout.get());
	}
}