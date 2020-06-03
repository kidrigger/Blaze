
#include "Blaze.hpp"

#include <Datatypes.hpp>
#include <glm/glm.hpp>

#include <core/Camera.hpp>
#include <drawables/ModelLoader.hpp>
#include <rendering/forward/FwdRenderer.hpp>
#include <util/Environment.hpp>
#include <util/files.hpp>

namespace blaze
{
// Constants
const int WIDTH = 1280;
const int HEIGHT = 720;
const bool FULLSCREEN = false;

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

void glfwErrorCallback(int error, const char* desc)
{
	std::cerr << "GLFW_ERROR: " << desc << std::endl;
}

struct SettingsOverlay
{
	bool rotate;
	float rotSpeed;
};

void runRefactored()
{
	// Usings
	using namespace std;

	// Variables
	GLFWwindow* window = nullptr;
	unique_ptr<ARenderer> renderer;
	unique_ptr<ModelLoader> modelLoader;

	glfwSetErrorCallback(glfwErrorCallback);

	Camera cam({0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, glm::radians(45.0f),
			   (float)WIDTH / (float)HEIGHT, 1.0f, 30.0f);

	// GLFW Setup
	assert(glfwInit());
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// glfwWindowHint(GLFW_CURSOR_HIDDEN, GLFW_TRUE);
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);
	window =
		glfwCreateWindow(WIDTH, HEIGHT, VERSION.FULL_NAME, FULLSCREEN ? glfwGetPrimaryMonitor() : nullptr, nullptr);
	assert(window != nullptr);

	{
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		mouse_callback(window, x, y);
	}

	// glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// Renderer creation
	renderer = make_unique<FwdRenderer>(window, enableValidationLayers);
	renderer->set_camera(&cam);
	assert(renderer->complete());

	auto environment = make_unique<util::Environment>(
		renderer.get(), loadImageCube(renderer->get_context(), "assets/PaperMill_Ruins_E/PaperMill_E_3k.hdr", false));
	renderer->setEnvironment(environment.get());

	modelLoader = make_unique<ModelLoader>();
	struct SceneInfo
	{
		string modelName;
		int modelIndex;
	};

	struct CameraInfo
	{
		glm::vec3 position;
		float pitch;
		float yaw;
	} camInfo = {
		cam.get_position(),
		0,
		0,
	};

	struct LightsInfo
	{
		struct PointLights
		{
			glm::vec3 pos{0};
			float brightness{1};
			float radius{1};
			bool hasShadow{false};

			bool draw()
			{
				bool edited = false;
				edited |= ImGui::InputFloat3("Position##POINT", &pos[0]);
				edited |= ImGui::InputFloat("Brightness##POINT", &brightness);
				edited |= ImGui::InputFloat("Radius##POINT", &radius);
				edited |= ImGui::Checkbox("Enable Shadow##POINT", &hasShadow);
				return edited;
			}
		};

		struct DirLight
		{
			glm::vec3 dir{-1.0f};
			float brightness{1};
			int numCascades{1};
			bool hasShadow{false};

			bool draw(bool editCascade = false)
			{
				bool edited = false;
				edited |= ImGui::InputFloat3("Direction##DIR", &dir[0]);
				edited |= ImGui::InputFloat("Brightness##DIR", &brightness);
				edited |= ImGui::Checkbox("Enable Shadow##DIR", &hasShadow);
				if (hasShadow && editCascade)
				{
					edited |= ImGui::SliderInt("Num Cascades##DIR", &numCascades, 1, 4);
				}
				else if (!editCascade)
				{
					ImGui::Text("Num Cascades: %d", numCascades);
				}
				return edited;
			}
		};

		std::vector<ALightCaster::Handle> pointHandles;
		PointLights editable;
		std::vector<PointLights> lights;
		uint32_t maxLights;

		std::vector<ALightCaster::Handle> dirHandles;
		DirLight dirEditable;
		std::vector<DirLight> dirLights;
		uint32_t maxDirLights;

		ALightCaster::Type type;
		int toDelete{-1};
		int toAdd{-1};
		int toUpdate{-1};

		void update(ARenderer* renderer)
		{
			if (type == ALightCaster::Type::POINT)
			{
				if (toDelete >= 0)
				{
					lights.erase(lights.begin() + toDelete);
					renderer->get_lightCaster()->removeLight(pointHandles[toDelete]);
					pointHandles.erase(pointHandles.begin() + toDelete);
				}
				if (toAdd >= 0)
				{
					lights.push_back(editable);
					auto handle = renderer->get_lightCaster()->createPointLight(editable.pos, editable.brightness,
																				editable.radius, editable.hasShadow);
					pointHandles.push_back(handle);

					editable.brightness = 1.0f;
					editable.pos = glm::vec3{0.0f};
					editable.radius = 1.0f;
					editable.hasShadow = false;
				}
				if (toUpdate >= 0)
				{
					auto h = pointHandles[toUpdate];
					auto& l = lights[toUpdate];
					renderer->get_lightCaster()->setPosition(h, l.pos);
					renderer->get_lightCaster()->setBrightness(h, l.brightness);
					renderer->get_lightCaster()->setRadius(h, l.radius);
					l.hasShadow = renderer->get_lightCaster()->setShadow(h, l.hasShadow);
				}
			}
			else if (type == ALightCaster::Type::DIRECTIONAL)
			{
				if (toDelete >= 0)
				{
					dirLights.erase(dirLights.begin() + toDelete);
					renderer->get_lightCaster()->removeLight(dirHandles[toDelete]);
					dirHandles.erase(dirHandles.begin() + toDelete);
				}
				if (toAdd >= 0)
				{
					dirLights.push_back(dirEditable);
					auto handle = renderer->get_lightCaster()->createDirectionLight(dirEditable.dir, dirEditable.brightness, dirEditable.hasShadow ? dirEditable.numCascades : 0);
					dirHandles.push_back(handle);

					dirEditable.brightness = 1.0f;
					dirEditable.dir = glm::vec3{-1.0f};
					dirEditable.numCascades = 1;
					dirEditable.hasShadow = false;
				}
				if (toUpdate >= 0)
				{
					auto h = dirHandles[toUpdate];
					auto& l = dirLights[toUpdate];
					renderer->get_lightCaster()->setDirection(h, l.dir);
					renderer->get_lightCaster()->setBrightness(h, l.brightness);
					l.numCascades = renderer->get_lightCaster()->setShadow(h, l.numCascades) ? l.numCascades : 0;
				}
			}
			toDelete = -1;
			toAdd = -1;
			toUpdate = -1;
		}
	} lightInfo;
	lightInfo.maxLights = renderer->get_lightCaster()->getMaxPointLights();
	lightInfo.maxDirLights = renderer->get_lightCaster()->getMaxDirectionLights();

	SceneInfo sceneInfo;
	sceneInfo.modelName = "Sponza";
	sceneInfo.modelIndex = 0;
	{
		auto& ms = modelLoader->getFileNames();
		for (auto& fn : ms)
		{
			if (fn == sceneInfo.modelName)
			{
				break;
			}
			sceneInfo.modelIndex++;
		}
	}

	int holderKey = 0;
	std::map<int, std::shared_ptr<Model2>> modelHolder;

	auto mod = modelHolder[holderKey++] =
		modelLoader->loadModel(renderer->get_context(), &renderer->get_shader(), sceneInfo.modelIndex);
	auto handle = renderer->submit(mod.get());

	// Run
	bool onetime = true;

	double prevTime = glfwGetTime();
	double deltaTime = 0.0;
	double elapsed = 0.0;
	double delay = 0.0f;

	SettingsOverlay settings;
	settings.rotate = false;
	settings.rotSpeed = 1.0f;

	while (!glfwWindowShouldClose(window))
	{
		prevTime = glfwGetTime();
		glfwPollEvents();
		elapsed += deltaTime;
		if (deltaTime < 0.006)
		{
			delay += 0.5;
		}
		else if (deltaTime > 0.007)
		{
			delay -= 0.5;
		}
		Sleep(glm::max(delay, 5.0));

		for (auto& [k, model] : modelHolder)
		{
			if (settings.rotate)
			{
				model->get_root()->rotation = glm::rotate(
					model->get_root()->rotation, settings.rotSpeed * static_cast<float>(deltaTime), glm::vec3(0, 1, 0));
			}
			model->update();
		}

		{
			if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			{
				glfwSetCursorPosCallback(window, nullptr);
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				mouseEnabled = false;
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}

			GUI::startFrame();
			{
				if (ImGui::Begin("Settings"))
				{
					ImGui::Text("Delta: %.3lf", deltaTime);

					ImGui::Checkbox("Rotate##Model", &settings.rotate);
					ImGui::SameLine();
					ImGui::PushItemWidth(100);
					ImGui::InputFloat("Speed##Rotation", &settings.rotSpeed, 0.1f, 0.3f, 2);
					ImGui::PopItemWidth();

					bool quit = ImGui::Button("Exit");
					if (quit)
					{
						glfwSetWindowShouldClose(window, GLFW_TRUE);
					}
				}
				ImGui::End();

				if (ImGui::Begin("Camera"))
				{
					if (ImGui::InputFloat3("Position", &camInfo.position[0]))
					{
						cam.moveTo(camInfo.position);
					}
					bool lookChange = false;
					lookChange |= ImGui::SliderAngle("Yaw", &camInfo.yaw, -179.99f, 180.0f, "%.2f deg");
					lookChange |= ImGui::SliderAngle("Pitch", &camInfo.pitch, -89.99f, 89.99f, "%.2f deg");
					if (lookChange)
					{
						cam.lookTo(glm::vec3(glm::sin(camInfo.yaw) * glm::cos(camInfo.pitch), glm::sin(camInfo.pitch),
											 -glm::cos(camInfo.yaw) * glm::cos(camInfo.pitch)));
					}
				}
				ImGui::End();

				if (ImGui::Begin("Scene"))
				{
					if (ImGui::CollapsingHeader("Models##InTheScene"))
					{
						auto& modelNames = modelLoader->getFileNames();
						if (ImGui::BeginCombo("Model##Combo", modelNames[sceneInfo.modelIndex].c_str()))
						{
							int i = 0;
							for (const auto& label : modelNames)
							{
								bool selected = (sceneInfo.modelIndex == i);
								if (ImGui::Selectable(label.c_str(), selected))
								{
									sceneInfo.modelIndex = i;
									sceneInfo.modelName = label;
									handle.destroy();
									auto mod = modelHolder[holderKey++] =
										modelLoader->loadModel(renderer->get_context(), &renderer->get_shader(),
															   sceneInfo.modelIndex); // TODO
									handle = renderer->submit(mod.get());
									renderer->waitIdle();
									modelHolder.erase(holderKey - 2);
									// TODO(Improvement) Make this smoother
								}
								if (selected)
								{
									ImGui::SetItemDefaultFocus();
								}
								++i;
							}
							ImGui::EndCombo();
						}
					}

					if (ImGui::CollapsingHeader("Lights"))
					{
						int idx = 0;
						for (auto& light : lightInfo.lights)
						{
							ImGui::PushID(idx);
							if (ImGui::TreeNode("Light##POINT", "light %d", idx))
							{
								bool edited = light.draw();
								if (edited)
								{
									lightInfo.toUpdate = idx;
									lightInfo.type = ALightCaster::Type::POINT;
								}
								if (ImGui::Button("Remove##POINT"))
								{
									lightInfo.toDelete = idx;
									lightInfo.type = ALightCaster::Type::POINT;
								}
								ImGui::TreePop();
							}
							ImGui::PopID();
							ImGui::Separator();
							idx++;
						}
						if (lightInfo.lights.size() < lightInfo.maxLights)
						{
							ImGui::Text("New Light");
							ImGui::TreePush("new light");
							{
								ImGui::PushID("LightEditable");
								lightInfo.editable.draw();
								ImGui::PopID();
								if (ImGui::Button("Add##POINT"))
								{
									lightInfo.toAdd = 1;
									lightInfo.type = ALightCaster::Type::POINT;
								}
							}
							ImGui::TreePop();
						}
					}
					if (ImGui::CollapsingHeader("DirLights"))
					{
						int idx = 0;
						for (auto& light : lightInfo.dirLights)
						{
							ImGui::PushID(idx);
							if (ImGui::TreeNode("Light##DIR", "light %d", idx))
							{
								bool edited = light.draw();
								if (edited)
								{
									lightInfo.toUpdate = idx;
									lightInfo.type = ALightCaster::Type::DIRECTIONAL;
								}
								if (ImGui::Button("Remove##DIR"))
								{
									lightInfo.toDelete = idx;
									lightInfo.type = ALightCaster::Type::DIRECTIONAL;
								}
								ImGui::TreePop();
							}
							ImGui::PopID();
							ImGui::Separator();
							idx++;
						}
						if (lightInfo.lights.size() < lightInfo.maxDirLights)
						{
							ImGui::Text("New Directional Light");
							ImGui::TreePush("new dir light");
							{
								ImGui::PushID("LightEditable");
								lightInfo.dirEditable.draw(true);
								ImGui::PopID();
								if (ImGui::Button("Add##DIR"))
								{
									lightInfo.toAdd = 1;
									lightInfo.type = ALightCaster::Type::DIRECTIONAL;
								}
							}
							ImGui::TreePop();
						}
					}
					lightInfo.update(renderer.get());
				}
				ImGui::End();
			}
			GUI::endFrame();

			renderer->render();

			deltaTime = glfwGetTime() - prevTime;
		}
	}
	renderer->waitIdle();
}
} // namespace blaze
