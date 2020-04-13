
#pragma once

#include "Datatypes.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace blaze
{
/**
 * @class Camera
 * @brief Utility class enclosing the UBO and associated calculations.
 */
class Camera
{
	CameraUBlock ubo;
	bool uboDirty;

	glm::vec3 position;
	glm::vec3 target;
	glm::vec3 left{-1, 0, 0};
	glm::vec3 up{0, 1, 0};
	float fov;
	float aspect;
	float nearPlane;
	float farPlane;

public:
	Camera(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up, float fov, float aspect,
		   float nearPlane = 0.1f, float farPlane = 10.0f)
		: position(pos), target(glm::normalize(target)), up(up), fov(fov), aspect(aspect), nearPlane(nearPlane),
		  farPlane(farPlane)
	{
		ubo.view = glm::lookAt(position, target + position, up);
		ubo.projection = glm::perspective(fov, aspect, nearPlane, farPlane);
		ubo.viewPos = position;
		ubo.farPlane = farPlane;
		uboDirty = true;
	}

	/// @brief Moves the camera by the offset.
	void moveBy(const glm::vec3& offset)
	{
		position += offset;
		uboDirty = true;
	}

	/// @brief Moves the camera to the location.
	void moveTo(const glm::vec3& pos)
	{
		position = pos;
		uboDirty = true;
	}

	/**
	 * @brief Rotates the camera to face the given rotation.
	 *
	 * @param up The altitude of the look vector (in radians)
	 * @param right The rotation of the look vector on Y axis.
	 */
	void rotateTo(const float up, const float right)
	{
		target =
			glm::normalize(glm::vec3(glm::sin(right) * glm::cos(up), glm::sin(up), glm::cos(right) * glm::cos(up)));
		uboDirty = true;
	}

	/**
	 * @brief Rotates the camera to face the given direction.
	 *
	 * @param direction The direction to look into.
	 */
	void lookTo(const glm::vec3& direction)
	{
		target = glm::normalize(direction);
		uboDirty = true;
	}

	/**
	 * @brief Get the CameraUBO from the camera.
	 *
	 * If the data has been changed, recalculate the UBO.
	 * Else only return a reference.
	 */
	const CameraUBlock& getUbo()
	{
		if (uboDirty)
		{
			ubo.view = glm::lookAt(position, target + position, up);
			ubo.projection = glm::perspective(fov, aspect, nearPlane, farPlane);
			ubo.viewPos = position;
			ubo.farPlane = farPlane;
			uboDirty = false;
		}
		return ubo;
	}

	/**
	 * @name getters
	 *
	 * @brief Getters for private fields.
	 *
	 * Each getter returns the private field.
	 * The fields are borrowed from Context and must not be deleted.
	 *
	 * @{
	 */
	inline const glm::vec3& get_position() const
	{
		return position;
	}
	inline glm::vec3& get_position()
	{
		return position;
	}
	inline const glm::vec3& get_direction()
	{
		return target;
	}
	inline const glm::vec3& get_up() const
	{
		return up;
	}

	inline const glm::mat4& get_projection() const
	{
		return ubo.projection;
	}
	inline const glm::mat4& get_view() const
	{
		return ubo.view;
	}

	inline float get_nearPlane() const
	{
		return nearPlane;
	}
	inline float get_farPlane() const
	{
		return farPlane;
	}
	inline float get_fov() const
	{
		return fov;
	}
	inline float get_aspect() const
	{
		return aspect;
	}
	/**
	 * @}
	 */
};
} // namespace blaze
