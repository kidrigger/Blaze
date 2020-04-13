
#include "Blaze.hpp"

#include <Datatypes.hpp>
#include <glm/glm.hpp>

#include <rendering/FwdRenderer.hpp>
#include <core/Camera.hpp>

namespace blaze
{
// Constants
const int WIDTH = 640;
const int HEIGHT = 480;

#ifdef VALIDATION_LAYERS_ENABLED
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

bool firstMouse = true;
bool mouseEnabled = false;
double lastX = 0.0;
double lastY = 0.0;

double yaw = -90.0;
double pitch = 0.0;

glm::vec3 cameraFront{0.0f, 0.0f, 1.0f};

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

	double sensitivity = mouseEnabled ? 0.05f : 0.1f;
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

void runRefactored()
{
	// Usings
	using namespace std;

	// Variables
	GLFWwindow* window = nullptr;

	std::unique_ptr<ARenderer> renderer;

	Camera cam({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, glm::radians(45.0f),
			   (float)WIDTH / (float)HEIGHT, 1.0f, 30.0f);

	// GLFW Setup
	assert(glfwInit());

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// glfwWindowHint(GLFW_CURSOR_HIDDEN, GLFW_TRUE);
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);
	window = glfwCreateWindow(WIDTH, HEIGHT, "Hello, Vulkan", nullptr, nullptr);
	assert(window != nullptr);

	{
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		mouse_callback(window, x, y);
	}

	// glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// Renderer creation
	
	//*
	renderer = make_unique<FwdRenderer>(window, enableValidationLayers);
	/*/
	auto f = new FwdRenderer(window, enableValidationLayers);
	//*/
	assert(renderer->complete());
	
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
				glfwSetCursorPosCallback(window, nullptr);
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				mouseEnabled = false;
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}

			renderer->render();

			deltaTime = glfwGetTime() - prevTime;
		}
	}
}
} // namespace blaze