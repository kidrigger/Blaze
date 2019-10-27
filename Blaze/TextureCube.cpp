
#include "TextureCube.hpp"
#include "Texture2D.hpp"
#include "util/files.hpp"
#include "Primitives.hpp"
#include "util/processing.hpp"

namespace blaze {

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

		util::Texture2CubemapInfo<decltype(std::ignore)> convertInfo {
			"shaders/vIrradiance.vert.spv",
			"shaders/fEqvrect2Cube.frag.spv",
			ds.get(),
			dsLayout.get(),
			512u,
			std::ignore
		};

		return util::Process<decltype(convertInfo.pcb)>::convertDescriptorToCubemap(context, convertInfo);
	}
}