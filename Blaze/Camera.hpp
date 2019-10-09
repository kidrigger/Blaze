
#pragma once

#include "Datatypes.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace blaze
{
	class Camera
	{
		CameraUniformBufferObject ubo;
		bool uboDirty;

		glm::vec3 position;
		glm::vec3 target;
		glm::vec3 left{ -1, 0, 0 };
		glm::vec3 up{ 0, 1, 0 };
		float fov;
		float aspect;
		float nearPlane;
		float farPlane;

	public:
		Camera(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up, float fov, float aspect, float nearPlane = 0.1f, float farPlane = 10.0f)
			: position(pos), target(target), up(up), fov(fov), aspect(aspect), nearPlane(nearPlane), farPlane(farPlane)
		{
			ubo.view = glm::lookAt(position, target + position, up);
			ubo.projection = glm::perspective(fov, aspect, 0.1f, 10.0f);
			ubo.viewPos = position;
			ubo.numLights = 0;
			uboDirty = true;
		}

		void moveBy(const glm::vec3& offset)
		{
			position += offset;
			uboDirty = true;
		}

		void moveTo(const glm::vec3& pos)
		{
			position = pos;
			uboDirty = true;
		}
		
		void rotateTo(const float up, const float right)
		{
			target = glm::vec3(glm::sin(right) * glm::cos(up), glm::sin(up), glm::cos(right) * glm::cos(up));
			uboDirty = true;
		}

		void lookTo(const glm::vec3& direction)
		{
			target = direction;
			uboDirty = true;
		}

		void addLight(const glm::vec3& position, float brightness)
		{
			if (ubo.numLights < 16)
			{
				ubo.lightPos[ubo.numLights++] = glm::vec4(position, brightness);
			}
			else
			{
				throw std::runtime_error("Max Light Count Reached.");
			}
		}

		void setLight(int index, const glm::vec3& position, float brightness)
		{
			if (ubo.numLights > index)
			{
				ubo.lightPos[index] = glm::vec4(position, brightness);
			}
			else
			{
				throw std::runtime_error("Max Light index Wrong.");
			}
		}

		const glm::vec3& get_position() const { return position; }
		const glm::vec3& get_up() const { return up; }

		const CameraUniformBufferObject& getUbo()
		{
			if (uboDirty)
			{
				ubo.view = glm::lookAt(position, target + position, up);
				ubo.projection = glm::perspective(fov, aspect, nearPlane, farPlane);
				ubo.viewPos = position;
				uboDirty = false;
			}
			return ubo;
		}
	};
}