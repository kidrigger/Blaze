
#include "Blaze.hpp"

#include <Datatypes.hpp>
#include <glm/glm.hpp>

#include <core/Camera.hpp>
#include <drawables/ModelLoader.hpp>
#include <rendering/forward/FwdRenderer.hpp>
#include <rendering/deferred/DfrRenderer.hpp>
#include <util/Environment.hpp>
#include <util/files.hpp>

#define VALIDATION_LAYERS_ENABLED

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

struct CameraInfo
{
	glm::vec3 position = {0.0f, 0.0f, 0.0f};
	const glm::vec3 up = {0.0f, 1.0f, 0.0f};
	float pitch = 0.0f;
	float yaw = 0.0f;
	float lastX = 0.0;
	float lastY = 0.0;
	float ambient = 0.03f;

	glm::vec3 getForward()
	{
		glm::vec3 front;
		front.x = static_cast<float>(-cos(camInfo.yaw) * cos(camInfo.pitch));
		front.y = static_cast<float>(sin(camInfo.pitch));
		front.z = static_cast<float>(-sin(camInfo.yaw) * cos(camInfo.pitch));

		return glm::normalize(front);
	}
} camInfo;

void mouse_callback(GLFWwindow* window, double x, double y)
{
	float xpos = static_cast<float>(x);
	float ypos = static_cast<float>(y);

	const static float maxPitch = glm::radians(89.0f);
	const static float PI = glm::pi<float>();
	const static float TAU = glm::two_pi<float>();

	if (firstMouse)
	{
		camInfo.lastX = xpos;
		camInfo.lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - camInfo.lastX;
	float yoffset = camInfo.lastY - ypos;
	camInfo.lastX = xpos;
	camInfo.lastY = ypos;

	float sensitivity = 0.01f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	camInfo.yaw += xoffset;
	camInfo.pitch += yoffset;

	glm::clamp(camInfo.pitch, -maxPitch, maxPitch);

	if (camInfo.yaw > PI)
	{
		camInfo.yaw -= TAU;
	}
	if (camInfo.yaw <= -PI)
	{
		camInfo.yaw += TAU;
	}
}

void glfwErrorCallback(int error, const char* desc)
{
	std::cerr << "GLFW_ERROR: " << desc << std::endl;
}

void run()
{
	// Usings
	using namespace std;

	// Variables
	GLFWwindow* window = nullptr;
	unique_ptr<ARenderer> renderer;
	unique_ptr<ModelLoader> modelLoader;

	glfwSetErrorCallback(glfwErrorCallback);

	Camera cam({9.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, glm::radians(45.0f),
			   glm::vec2((float)WIDTH, (float)HEIGHT), 1.0f, 30.0f);
	camInfo.position = cam.get_position();
	camInfo.ambient = cam.get_ambient();

	// GLFW Setup
	assert(glfwInit());
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);
	window =
		glfwCreateWindow(WIDTH, HEIGHT, VERSION.FULL_NAME, FULLSCREEN ? glfwGetPrimaryMonitor() : nullptr, nullptr);
	assert(window != nullptr);

	{
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		mouse_callback(window, x, y);
	}

	glfwSetCursorPosCallback(window, nullptr);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// Renderer creation
	renderer = make_unique<DfrRenderer>(window, enableValidationLayers);
	renderer->set_camera(&cam);
	assert(renderer->complete());

	renderer->setSkybox(loadImageCube(renderer->get_context(), "assets/PaperMill_Ruins_E/PaperMill_E_3k.hdr", false));

	modelLoader = make_unique<ModelLoader>();
	struct SceneInfo
	{
		string modelName;
		int modelIndex;
	};

	struct LightsInfo
	{
		struct PointLights
		{
			glm::vec3 pos{0};
			glm::vec3 color{1};
			float brightness;
			float radius{1};
			bool hasShadow{false};

			bool draw()
			{
				bool edited = false;
				edited |= ImGui::DragFloat3("Position##POINT", &pos[0], 0.2f, -100.0f, 100.0f);
				edited |=
					ImGui::ColorEdit3("Color##POINT", &color[0], ImGuiColorEditFlags_Float); // 0.05f, 0.0f, 16.0f);
				edited |= ImGui::DragFloat("Brightness##POINT", &brightness, 0.01f, 0.0f, 16.0f);
				edited |= ImGui::DragFloat("Radius##POINT", &radius, 0.1f, 0.1f, 100.0f);
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
				edited |= ImGui::DragFloat3("Direction##DIR", &dir[0], 0.01f, -1.0f, 1.0f);
				edited |= ImGui::DragFloat("Brightness##DIR", &brightness, 0.05f, 0.1f, 2.0f);
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
					auto handle = renderer->get_lightCaster()->createPointLight(editable.pos, editable.color * editable.brightness,
																				editable.radius, editable.hasShadow);
					pointHandles.push_back(handle);

					editable.color = glm::vec3(1.0f);
					editable.pos = glm::vec3{0.0f};
					editable.brightness = 1.0f;
					editable.radius = 1.0f;
					editable.hasShadow = false;
				}
				if (toUpdate >= 0)
				{
					auto h = pointHandles[toUpdate];
					auto& l = lights[toUpdate];
					renderer->get_lightCaster()->setPosition(h, l.pos);
					renderer->get_lightCaster()->setColor(h, l.color * l.brightness);
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
					l.hasShadow = renderer->get_lightCaster()->setShadow(h, l.hasShadow);
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
	std::map<int, std::shared_ptr<Model>> modelHolder;

	auto mod = modelHolder[holderKey++] =
		modelLoader->loadModel(renderer->get_context(), renderer->get_shader(), sceneInfo.modelIndex);
	auto handle = renderer->submit(mod.get());

	// Run
	bool onetime = true;

	double prevTime = glfwGetTime();
	double deltaTime = 0.0;
	double elapsed = 0.0;
	double delay = 0.0f;

	while (!glfwWindowShouldClose(window))
	{
		prevTime = glfwGetTime();
		glfwPollEvents();
		elapsed += deltaTime;

		for (auto& [k, model] : modelHolder)
		{
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

			if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS)
			{
				if (!mouseEnabled)
				{
					mouseEnabled = true;
					firstMouse = true;
					glfwSetCursorPosCallback(window, mouse_callback);
				}
			}
			else
			{
				if (mouseEnabled)
				{
					mouseEnabled = false;
					glfwSetCursorPosCallback(window, nullptr);
				}
			}
			
			if (mouseEnabled)
			{
				float dx = static_cast<float>((glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) -
											  (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS));
				float dz = static_cast<float>((glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) -
											  (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS));
				if (dx != 0.0f || dz != 0.0f)
				{
					auto fwd = camInfo.getForward();
					auto right = glm::cross(fwd, camInfo.up);
					glm::vec3 delta = glm::normalize(fwd * dz + right * dx);

					float speed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 5.0f : 1.0f;

					camInfo.position += delta * static_cast<float>(deltaTime) * speed;
				}
			}

			GUI::startFrame();
			{
				if (ImGui::Begin("Main Panel"))
				{
					ImGui::Text("Mouse %s", mouseEnabled ? "Enabled" : "Disabled");

					ImGui::Text("Delta: %.3lf", deltaTime);

					bool quit = ImGui::Button("Exit");
					if (quit)
					{
						glfwSetWindowShouldClose(window, GLFW_TRUE);
					}
				}
				ImGui::End();

				if (ImGui::Begin("Camera"))
				{
					if (ImGui::DragFloat("Ambient Brightness", &camInfo.ambient, 0.02f, 0.02f, 1.0f))
					{
						cam.set_ambient(camInfo.ambient);
					}
					if (ImGui::InputFloat3("Position", &camInfo.position[0]) || mouseEnabled)
					{
						cam.moveTo(camInfo.position);
					}
					bool lookChange = false;
					lookChange |= ImGui::SliderAngle("Yaw", &camInfo.yaw, -179.99f, 180.0f, "%.2f deg");
					lookChange |= ImGui::SliderAngle("Pitch", &camInfo.pitch, -89.99f, 89.99f, "%.2f deg");
					if (lookChange || mouseEnabled)
					{
						cam.lookTo(camInfo.getForward());
					}
				}
				ImGui::End();

				if (ImGui::Begin("Scene"))
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
									modelLoader->loadModel(renderer->get_context(), renderer->get_shader(),
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

				renderer->drawSettings();
			}
			GUI::endFrame();

			renderer->render();

			deltaTime = glfwGetTime() - prevTime;
		}
	}
	renderer->waitIdle();
}
} // namespace blaze
