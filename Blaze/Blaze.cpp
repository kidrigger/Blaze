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
#include "Texture.hpp"
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
		IndexedVertexBuffer<Vertex> vertexBuffer;
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

		auto model = loadModel(renderer, "assets/helmet/DamagedHelmet.gltf");

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
