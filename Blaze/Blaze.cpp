// Blaze.cpp : Defines the entry point for the application.
//

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Blaze.hpp"
#include "util/Managed.hpp"
#include "util/DeviceSelection.hpp"
#include "util/createFunctions.hpp"
#include "Context.hpp"
#include "Renderer.hpp"
#include "Datatypes.hpp"
#include "VertexBuffer.hpp"
#include "util/loaders.hpp"
#include "Texture.hpp"

#include <optional>
#include <vector>
#include <iostream>
#include <set>
#include <algorithm>
#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define VALIDATION_LAYERS_ENABLED

#ifdef NDEBUG
#ifdef VALIDATION_LAYERS_ENABLED
#undef VALIDATION_LAYERS_ENABLED
#endif
#endif

namespace blaze
{
	// Constants
	const int WIDTH = 800;
	const int HEIGHT = 600;

#ifdef VALIDATION_LAYERS_ENABLED
	const bool enableValidationLayers = true;
#else
	const bool enableValidationLayers = false;
#endif

	void run()
	{
		// Usings
		using namespace std;
		using namespace util;

		// Variables
		GLFWwindow* window = nullptr;
		Context ctx;
		Renderer renderer;
		IndexedVertexBuffer<Vertex> vertexBuffer;
		TextureImage image;

		UniformBufferObject cameraUBO
		{
			glm::mat4(1.0f),
			glm::lookAt(
				glm::vec3{0.0f, 0.0f, -4.0f},
				glm::vec3{0.0f, 0.0f, 0.0f},
				glm::vec3{0.0f, -1.0f, 0.0f}),
			glm::perspective(
				glm::radians(45.0f),
				4.0f / 3.0f,
				0.1f,
				10.0f)
		};

		// GLFW Setup
		assert(glfwInit());

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Hello, Vulkan", nullptr, nullptr);
		assert(window != nullptr);

		renderer = Renderer(window);
		if (!renderer.complete())
		{
			throw std::runtime_error("Renderer could not be created");
		}

		vector<Vertex> vertices = {
			{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.2f}, {1.0f, 1.0f}},
			{{0.5f, -0.5f, -0.5f}, {1.0f, 0.2f, 0.2f}, {1.0f, 0.0f}},
			{{-0.5f, -0.5f, -0.5f}, {0.2f, 0.2f, 0.2f}, {0.0f, 0.0f}},
			{{-0.5f, 0.5f, -0.5f}, {0.2f, 1.0f, 0.2f}, {0.0f, 1.0f}},
			{{-0.5f, -0.5f, 0.5f}, {0.2f, 0.2f, 1.0f}, {0.0f, 0.0f}},
			{{0.5f, -0.5f, 0.5f}, {1.0f, 0.2f, 1.0f}, {1.0f, 0.0f}},
			{{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.5f}, {0.2f, 1.0f, 1.0f}, {0.0f, 1.0f}}
		};

		vector<uint32_t> indices = {
			0, 1, 2,
			0, 2, 3,
			4, 5, 6,
			4, 6, 7,
			1, 0, 6,
			1, 6, 5,
			7, 6, 0,
			7, 0, 3,
			7, 3, 2,
			7, 2, 4,
			4, 2, 1,
			4, 1, 5
		};

		vertexBuffer = IndexedVertexBuffer(renderer, vertices, indices);

		image = loadImage(renderer, "assets/container2.png");

		auto createDescriptorPool = [&renderer]()
		{
			VkDescriptorPoolSize poolSize = {};
			poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			poolSize.descriptorCount = 1;

			VkDescriptorPoolCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			createInfo.poolSizeCount = 1;
			createInfo.pPoolSizes = &poolSize;
			createInfo.maxSets = 1;
			createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

			VkDescriptorPool pool;
			auto result = vkCreateDescriptorPool(renderer.get_device(), &createInfo, nullptr, &pool);
			if (result != VK_SUCCESS)
			{
				throw std::runtime_error("Descriptor pool creation failed with " + std::to_string(result));
			}
			return pool;
		};

		auto createDescriptorSet = [&renderer](VkDescriptorSetLayout layout, VkDescriptorPool pool, const TextureImage& texture, uint32_t binding)
		{
			VkDescriptorSetAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = pool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &layout;

			VkDescriptorSet descriptorSet;
			auto result = vkAllocateDescriptorSets(renderer.get_device(), &allocInfo, &descriptorSet);
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

			vkUpdateDescriptorSets(renderer.get_device(), 1, &write, 0, nullptr);

			return descriptorSet;
		};

		Managed<VkDescriptorPool> dsPool = Managed(createDescriptorPool(), [dev = renderer.get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });
		Managed<VkDescriptorSet> ds = Managed(createDescriptorSet(renderer.get_materialLayout(), dsPool.get(), image, 1), [dev = renderer.get_device(), pool = dsPool.get()](VkDescriptorSet& dset) { vkFreeDescriptorSets(dev, pool, 1, &dset); });

		loadModel("assets/helmet/DamagedHelmet.gltf");

		auto renderCommand = [
			vert = vertexBuffer.get_vertexBuffer(),
			idx = vertexBuffer.get_indexBuffer(),
			size = vertexBuffer.get_verticeSize(),
			idxc = vertexBuffer.get_indexCount(),
			dset = ds.get()
		]
		(VkCommandBuffer cmdBuffer, VkPipelineLayout layout)
		{
			VkBuffer buffers[] = { vert };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmdBuffer, 0, 1, buffers, offsets);
			vkCmdBindIndexBuffer(cmdBuffer, idx, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &dset, 0, nullptr);
			vkCmdDrawIndexed(cmdBuffer, idxc, 1, 0, 0, 0);
		};

		// Run
		bool onetime = true;

		double prevTime = glfwGetTime();
		double deltaTime = 0.0;
		int intermittence = 0;
		double elapsed = 0.0;

		while (!glfwWindowShouldClose(window))
		{
			prevTime = glfwGetTime();
			glfwPollEvents();
			elapsed += deltaTime;

			try
			{
				cameraUBO.view = glm::lookAt(4.0f*glm::vec3(glm::cos(glm::radians(20*elapsed)), 0.0f, glm::sin(glm::radians(20 * elapsed))), glm::vec3{ 0.0f }, glm::vec3{ 0.0f, -1.0f, 0.0f });
				// cameraUBO.model = glm::rotate(cameraUBO.model, glm::radians(static_cast<float>(10*deltaTime)), glm::vec3{ 1.0f, 0.0f, 0.0f });
				renderer.set_cameraUBO(cameraUBO);
				renderer.renderFrame();
				if (onetime) 
				{
					renderer.submit(renderCommand);
					onetime = false;
				}
			}
			catch (std::exception& e)
			{
				cerr << e.what() << endl;
			}

			deltaTime = glfwGetTime() - prevTime;
			printf("\r%.4lf", deltaTime);
		}

		cout << endl;

		// Wait for all commands to finish
		vkDeviceWaitIdle(renderer.get_device());

		glfwDestroyWindow(window);
		glfwTerminate();
	}
}

int main(int argc, char* argv[])
{
	blaze::run();

	return 0;
}
