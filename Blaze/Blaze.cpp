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
#include "Texture2D.hpp"
#include "TextureCube.hpp"
#include "Model.hpp"
#include "Camera.hpp"

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

	bool firstMouse = true;
	double lastX = 0.0;
	double lastY = 0.0;

	double yaw = -90.0;
	double pitch = 0.0;

	glm::vec3 cameraFront{ 0.0f, 0.0f, 1.0f };

	void mouse_callback(GLFWwindow* window, double xpos, double ypos)
	{
		if (firstMouse)
		{
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		double xoffset = xpos - lastX;
		double yoffset = lastY - ypos;
		lastX = xpos;
		lastY = ypos;

		double sensitivity = 0.05f;
		xoffset *= sensitivity;
		yoffset *= sensitivity;

		yaw += xoffset;
		pitch += yoffset;

		if (pitch > 89.0f)
			pitch = 89.0f;
		if (pitch < -89.0f)
			pitch = -89.0f;

		glm::vec3 front;
		front.x = static_cast<float>(cos(glm::radians(yaw)) * cos(glm::radians(pitch)));
		front.y = static_cast<float>(sin(glm::radians(pitch)));
		front.z = static_cast<float>(sin(glm::radians(yaw)) * cos(glm::radians(pitch)));
		cameraFront = glm::normalize(front);
	}

	void run()
	{
		// Usings
		using namespace std;
		using namespace util;

		// Variables
		GLFWwindow* window = nullptr;
		Renderer renderer;
		IndexedVertexBuffer<Vertex> vbo;
		Texture2D image;

		Camera cam({ 0.0f, 0.0f, 4.0f }, { 0.0f, 0.0f, -4.0f }, { 0.0f, 1.0f, 0.0f }, glm::radians(45.0f), (float)WIDTH / (float)HEIGHT, 1.0f, 1000.0f);
		// cam.addLight(glm::vec3{ -8.0f, 2.0f, 0.0f }, 1.0f);
		// cam.addLight(glm::vec3{ -4.0f, 2.0f, 0.0f }, 1.0f);
		// cam.addLight(glm::vec3{ 0.0f, 2.0f, 0.0f }, 1.0f);
		// cam.addLight(glm::vec3{ 4.0f, 2.0f, 0.0f }, 1.0f);
		// cam.addLight(glm::vec3{ 8.0f, 2.0f, 0.0f }, 1.0f);
		cam.addLight(glm::vec3(0.0f), 1.0f);

		// GLFW Setup
		assert(glfwInit());

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_CURSOR_HIDDEN, GLFW_TRUE);
		glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Hello, Vulkan", nullptr, nullptr);
		assert(window != nullptr);

		{
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			mouse_callback(window, x, y);
		}

		glfwSetCursorPosCallback(window, mouse_callback);
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

		renderer = Renderer(window);
		if (!renderer.complete())
		{
			throw std::runtime_error("Renderer could not be created");
		}

		// fbudrl
		string skybox_dir = "assets/Environment1.cubemap/";
		vector<string> skybox_faces{ "negx.bmp", "posx.bmp", "posy.bmp", "negy.bmp", "posz.bmp", "negz.bmp" };
		for (auto& face : skybox_faces)
		{
			face = skybox_dir + face;
		}

		auto skybox = loadImageCube(renderer.get_context(), skybox_faces, true);

		// Hello my old code
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
		vbo = IndexedVertexBuffer(renderer.get_context(), vertices, indices);

		auto createDescriptorSet = [device = renderer.get_device()](VkDescriptorSetLayout layout, VkDescriptorPool pool, const TextureCube& texture, uint32_t binding)
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

		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 1;
		vector<VkDescriptorPoolSize> poolSizes = { poolSize };
		Managed<VkDescriptorPool> dsPool = Managed(createDescriptorPool(renderer.get_device(), poolSizes, 1), [dev = renderer.get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });
		Managed<VkDescriptorSet> ds = Managed(createDescriptorSet(renderer.get_skyboxLayout(), dsPool.get(), skybox, 1), [dev = renderer.get_device(), pool = dsPool.get()](VkDescriptorSet& dset) { vkFreeDescriptorSets(dev, pool, 1, &dset); });


		auto model = loadModel(renderer, "assets/boombox2/BoomBoxWithAxes.gltf");
		model.get_root()->scale = { 100.0f };

		renderer.set_skyboxCommand([&vbo](VkCommandBuffer buf, VkPipelineLayout lay) 
		{
			VkBuffer vbufs[] = { vbo.get_vertexBuffer() };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(buf, 0, 1, vbufs, offsets);
			vkCmdBindIndexBuffer(buf, vbo.get_indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(buf, vbo.get_indexCount(), 1, 0, 0, 0); 
		});

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

			{
				if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
				{
					glfwSetWindowShouldClose(window, GLFW_TRUE);
				}
				glm::vec3 cameraPos(0.f);
				float cameraSpeed = 1.f * static_cast<float>(deltaTime); // adjust accordingly
				if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
					cameraSpeed *= 5.0f;

				if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
					cameraPos += cameraSpeed * cameraFront;
				if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
					cameraPos -= cameraSpeed * cameraFront;
				if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
					cameraPos -= glm::normalize(glm::cross(cameraFront, cam.get_up())) * cameraSpeed;
				if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
					cameraPos += glm::normalize(glm::cross(cameraFront, cam.get_up())) * cameraSpeed;
				cam.moveBy(cameraPos);
			}
			cam.lookTo(cameraFront);
			cam.setLight(0, cam.get_position(), 1.0f);
			// cam.setLight(0, glm::vec3{ -8.0f, 2.0f + 0.5f * sin(3*elapsed), 0.5f * cos(3 * elapsed) }, 1.0f);
			// cam.setLight(1, glm::vec3{ -4.0f, 2.0f + 0.5f * cos(3 * elapsed + 4), 0.5f * sin(3 * elapsed + 4) }, 1.0f);
			// cam.setLight(2, glm::vec3{ 0.0f, 2.0f + 0.5f * sin(3 * elapsed + 3.14), 0.5f * cos(3 * elapsed + 3.14) }, 1.0f);
			// cam.setLight(3, glm::vec3{ 4.0f, 2.0f + 0.5f * sin(3 * elapsed + 1.4), 0.5f * cos(3 * elapsed + 1.4) }, 1.0f);
			// cam.setLight(4, glm::vec3{ 8.0f, 2.0f + 0.5f * cos(3 * elapsed), 0.5f * sin(3 * elapsed) }, 1.0f);

			try
			{
				model.update();
				renderer.set_cameraUBO(cam.getUbo());
				renderer.renderFrame();
				if (onetime) 
				{
					// renderer.submit(renderCommand);
					renderer.submit([&ds](VkCommandBuffer cmdBuffer, VkPipelineLayout layout) { vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 2, 1, &ds.get(), 0, nullptr); });
					renderer.submit([&model](VkCommandBuffer cmdBuffer, VkPipelineLayout layout) { model.draw(cmdBuffer, layout); });
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
