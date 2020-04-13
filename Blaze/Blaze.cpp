﻿// Blaze.cpp : Defines the entry point for the application.
//

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Blaze.hpp"

#include <core/Camera.hpp>
#include <Datatypes.hpp>
#include <gui/GUI.hpp>
#include <Model.hpp>
#include <Primitives.hpp>
#include <Texture2D.hpp>
#include <TextureCube.hpp>
#include <VertexBuffer.hpp>
#include <core/Context.hpp>
#include <rendering/ForwardRenderer.hpp>
#include <rendering/Renderer.hpp>
#include <util/DeviceSelection.hpp>
#include <util/Environment.hpp>
#include <util/Managed.hpp>
#include <util/createFunctions.hpp>
#include <util/files.hpp>

#include <algorithm>
#include <deque>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <vector>

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

/**
 * @namespace blaze
 * @brief Main namespace for the blaze project.
 *
 * Everything in the renderer project should be under the blaze namespace.
 */
namespace blaze
{
// Constants
const int WIDTH = 1920;
const int HEIGHT = 1080;

#ifdef VALIDATION_LAYERS_ENABLED
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

struct CameraControlInfo
{
	bool firstMouse = true;
	bool mouseEnabled = false;
	double lastX = 0.0;
	double lastY = 0.0;

	double yaw = -90.0;
	double pitch = 0.0;
	glm::vec3 cameraFront{0.0f, 0.0f, 1.0f};

	static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
	void update(GLFWwindow* window, Camera& cam, double deltaTime);
} camControlInfo;

void CameraControlInfo::mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (camControlInfo.firstMouse)
	{
		camControlInfo.lastX = xpos;
		camControlInfo.lastY = ypos;
		camControlInfo.firstMouse = false;
	}

	double xoffset = xpos - camControlInfo.lastX;
	double yoffset = camControlInfo.lastY - ypos;
	camControlInfo.lastX = xpos;
	camControlInfo.lastY = ypos;

	double sensitivity = camControlInfo.mouseEnabled ? 0.05f : 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	camControlInfo.yaw += xoffset;
	camControlInfo.pitch += yoffset;

	if (camControlInfo.pitch > 89.0f)
		camControlInfo.pitch = 89.0f;
	if (camControlInfo.pitch < -89.0f)
		camControlInfo.pitch = -89.0f;

	glm::vec3 front;
	front.x = static_cast<float>(cos(glm::radians(camControlInfo.yaw)) * cos(glm::radians(camControlInfo.pitch)));
	front.y = static_cast<float>(sin(glm::radians(camControlInfo.pitch)));
	front.z = static_cast<float>(sin(glm::radians(camControlInfo.yaw)) * cos(glm::radians(camControlInfo.pitch)));
	camControlInfo.cameraFront = glm::normalize(front);
}

void CameraControlInfo::update(GLFWwindow* window, Camera& cam, double deltaTime)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetCursorPosCallback(window, nullptr);
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		camControlInfo.mouseEnabled = false;
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

	double x = lastX;
	double y = lastY;
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		y = lastY + 1.0f;
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		y = lastY - 1.0f;
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		x = lastX + 1.0f;
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		x = lastX - 1.0f;
	mouse_callback(window, x, y);

	cam.lookTo(cameraFront);
}

/**
 * @struct DebugRenderSettings
 *
 * @brief Settings exposed during run using the GUI.
 */
struct DebugRenderSettings
{
	std::array<float, 300> deltaTime{0};
	bool rotate{false};
	float rotationSpeed{1.0f};
	glm::vec3 scale{1.0f};
	char filename[256]{0};
	char skybox[256]{0};
	bool lockLight{false};
	int currentLight{0};

	struct TextureViewMenuControl
	{
		std::array<std::string, 10> labels{"Full Render",	  "Diffuse Map",  "Normal Map",	  "Metallic Map",
										   "Roughness Map",	  "AO Map",		  "Emission Map", "Position",
										   "Cascade Overlay", "Miscellaneous"};
		int currentValue = 0;
		/**
		 * @fn value()
		 *
		 * @brief Gets the selected option as Enum instead of internal int.
		 */
		SettingsUBlock::ViewTextureMap value() const
		{
			return static_cast<SettingsUBlock::ViewTextureMap>(currentValue);
		}
	} textureMapSettings;

	SettingsUBlock settingsUBO = {SettingsUBlock::VTM_FULL, 0, 0};

	void submitDelta(float delta)
	{
		memcpy(deltaTime.data(), deltaTime.data() + 1, deltaTime.size() * sizeof(float));
		deltaTime[deltaTime.size() - 1] = delta;
	}
} settings{};

/**
 * @brief Entrypoint for the renderer application.
 *
 * The main driver code for the entire blaze rendering application
 * including the setup, loading, main loop and the teardown.
 */
void run()
{
	// Usings
	using namespace std;
	using namespace util;

	// Variables
	GLFWwindow* window = nullptr;
	std::unique_ptr<Renderer> renderer;
	IndexedVertexBuffer<Vertex> vbo;

	Camera cam({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, glm::radians(45.0f),
			   (float)WIDTH / (float)HEIGHT, 1.0f, 30.0f);

	// GLFW Setup
	assert(glfwInit());

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// glfwWindowHint(GLFW_CURSOR_HIDDEN, GLFW_TRUE);
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);
	window = glfwCreateWindow(WIDTH, HEIGHT, "Hello, Vulkan", glfwGetPrimaryMonitor(), nullptr);
	assert(window != nullptr);

	{
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		CameraControlInfo::mouse_callback(window, x, y);
	}

	// glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	renderer = std::make_unique<ForwardRenderer>(window, enableValidationLayers);
	if (!renderer->complete())
	{
		throw std::runtime_error("Renderer could not be created");
	}
	renderer->set_camera(&cam);

	auto swingingLight = renderer->get_lightSystem().addPointLight(glm::vec3(0.0f), 3.0f, true);
	std::vector<LightSystem::LightHandle> lights = {
		renderer->get_lightSystem().addPointLight(glm::vec3{-7.0f, 1.0f, -0.5f}, 2.0f, true),
		renderer->get_lightSystem().addPointLight(glm::vec3{7.0f, 1.0f, -0.5f}, 2.0f, true),
		renderer->get_lightSystem().addPointLight(glm::vec3{0.0f, 1.0f, -0.5f}, 2.0f, true)};
	renderer->get_lightSystem().addDirLight(glm::vec3(-0.7, -1.0, -0.5), 1.0f, true);

#ifdef _WIN32
	strcpy_s(settings.skybox, "assets/PaperMill_Ruins_E/PaperMill_E_3k.hdr");
	strcpy_s(settings.filename, "assets/sponza/Sponza.gltf");
#else
	strcpy(settings.skybox, "assets/PaperMill_Ruins_E/PaperMill_E_3k.hdr");
	strcpy(settings.filename, "assets/sponza/Sponza.gltf");
#endif

	auto skybox = loadImageCube(renderer->get_context(), settings.skybox, true);
	vbo = getUVCube(renderer->get_context());

	auto createDescriptorSet = [device = renderer->get_device()](VkDescriptorSetLayout layout, VkDescriptorPool pool) {
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

		return descriptorSet;
	};

	auto writeToDescriptor = [device =
								  renderer->get_device()](VkDescriptorSet descriptorSet,
														  const vector<pair<uint32_t, VkDescriptorImageInfo>>& images) {
		vector<VkWriteDescriptorSet> writes;
		for (auto& image : images)
		{
			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.descriptorCount = 1;
			write.dstSet = descriptorSet;
			write.dstBinding = image.first;
			write.dstArrayElement = 0;
			write.pImageInfo = &image.second;
			writes.push_back(write);
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	};

	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1;
	vector<VkDescriptorPoolSize> poolSizes = {poolSize};
	Managed<VkDescriptorPool> dsPool = Managed(
		createDescriptorPool(renderer->get_device(), poolSizes, 1),
		[dev = renderer->get_device()](VkDescriptorPool& pool) { vkDestroyDescriptorPool(dev, pool, nullptr); });
	Managed<VkDescriptorSet> ds = Managed(createDescriptorSet(renderer->get_environmentLayout(), dsPool.get()),
										  [dev = renderer->get_device(), pool = dsPool.get()](VkDescriptorSet& dset) {
											  vkFreeDescriptorSets(dev, pool, 1, &dset);
										  });
	writeToDescriptor(ds.get(), {{0, skybox.get_imageInfo()}});

	auto irradMap = createIrradianceCube(*renderer, ds.get());
	writeToDescriptor(ds.get(), {{1, irradMap.get_imageInfo()}});

	auto prefilt = createPrefilteredCube(*renderer, ds.get());
	writeToDescriptor(ds.get(), {{2, prefilt.get_imageInfo()}});

	auto brdfLut = createBrdfLut(renderer->get_context());
	writeToDescriptor(ds.get(), {{3, brdfLut.get_imageInfo()}});

	auto model = loadModel(*renderer, settings.filename);

	renderer->set_skyboxCommand([&vbo](VkCommandBuffer buf, VkPipelineLayout lay, uint32_t frameCount) {
		VkBuffer vbufs[] = {vbo.get_vertexBuffer()};
		VkDeviceSize offsets[] = {0};
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

	renderer->set_environmentDescriptor(ds.get());
	renderer->submit(&model);

	while (!glfwWindowShouldClose(window))
	{
		prevTime = glfwGetTime();
		glfwPollEvents();
		elapsed += deltaTime;

		camControlInfo.update(window, cam, deltaTime);

		renderer->get_lightSystem().setLightPosition(swingingLight, glm::vec3(4.0f * glm::cos(elapsed), 5.0f, -0.3f));
		if (settings.lockLight)
		{
			renderer->get_lightSystem().setLightPosition(lights[settings.currentLight], cam.get_position());
		}

		if (settings.rotate)
		{
			model.get_root()->rotation *= glm::quat(glm::vec3(0, settings.rotationSpeed * deltaTime, 0));
		}

		GUI::startFrame();
		{
			if (ImGui::Begin("Profiling"))
			{
				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
							ImGui::GetIO().Framerate);
				settings.submitDelta(1000.0f / ImGui::GetIO().Framerate);
				ImGui::PlotLines("FPS", settings.deltaTime.data(), static_cast<int>(settings.deltaTime.size()), 0,
								 nullptr, 0);
				if (ImGui::Button("Exit"))
				{
					glfwSetWindowShouldClose(window, GLFW_TRUE);
				}
			}
			ImGui::End();
			if (ImGui::Begin("Scene##Settings"))
			{
				ImGui::InputText("Model", settings.filename, IM_ARRAYSIZE(settings.filename));
				ImGui::SameLine();
				if (ImGui::Button("Load##Model"))
				{
					if (fileExists(settings.filename))
					{
						model = loadModel(*renderer, settings.filename);
					}
				}
				if (ImGui::InputFloat3("Scale##Model", &settings.scale[0]))
				{
					model.get_root()->scale = settings.scale;
				}
				ImGui::PushItemWidth(88.0f);
				ImGui::InputFloat("Rotation Speed##Model", &settings.rotationSpeed);
				ImGui::PopItemWidth();
				ImGui::Checkbox("Rotate##Model", &settings.rotate);
				ImGui::Text("Vertex Count: %d", model.get_vertexCount());
				ImGui::Text("Triangle Count: %d", model.get_indexCount() / 3);
				if (ImGui::BeginCombo(
						"Texture Views##Combo",
						settings.textureMapSettings.labels[settings.textureMapSettings.currentValue].c_str()))
				{
					int i = 0;
					for (const auto& label : settings.textureMapSettings.labels)
					{
						bool selected = (settings.textureMapSettings.currentValue == i);
						if (ImGui::Selectable(label.c_str(), selected))
						{
							settings.textureMapSettings.currentValue = i;
						}
						if (selected)
						{
							ImGui::SetItemDefaultFocus();
						}
						i++;
					}
					ImGui::EndCombo();
					settings.settingsUBO.textureMap = settings.textureMapSettings.value();
				}
				ImGui::Checkbox("Enable Skybox", &settings.settingsUBO.enableSkybox.B);
				ImGui::Checkbox("Enable IBL", &settings.settingsUBO.enableIBL.B);
				ImGui::InputFloat("Exposure", &settings.settingsUBO.exposure);
				ImGui::InputFloat("Gamma", &settings.settingsUBO.gamma);
				ImGui::Checkbox("Lock Light", &settings.lockLight);
				ImGui::SameLine();
				ImGui::InputInt("Light to Lock", &settings.currentLight);
				{
					settings.currentLight =
						std::max(std::min(settings.currentLight, static_cast<int>(lights.size() - 1)), 0);
				}
				if (ImGui::Button("Lock Mouse"))
				{
					camControlInfo.firstMouse = true;

					{
						double x, y;
						glfwGetCursorPos(window, &x, &y);
						CameraControlInfo::mouse_callback(window, x, y);
					}

					glfwSetCursorPosCallback(window, CameraControlInfo::mouse_callback);
					glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
					camControlInfo.mouseEnabled = true;
				}
			}
			ImGui::End();
		}
		GUI::endFrame();

		try
		{
			model.update();
			renderer->set_settingsUBO(settings.settingsUBO);
			renderer->renderFrame();
		}
		catch (std::exception& e)
		{
			cerr << e.what() << endl;
		}

		deltaTime = glfwGetTime() - prevTime;
	}

	cout << endl;

	// Wait for all commands to finish
	vkDeviceWaitIdle(renderer->get_device());

	glfwDestroyWindow(window);
	glfwTerminate();
}
} // namespace blaze
