
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
 *
 * @brief Utility class enclosing the UBO and associated calculations.
 */
class Camera
{
public:
	/**
	 * @struct CameraUBlock
	 *
	 * @brief Holds camera data to be sent to GPU.
	 *
	 * @note Needs Restructuring.
	 *
	 * @addtogroup UniformBufferObjects
	 * @{
	 */
	struct UBlock
	{
		/// The View matrix of the camera.
		alignas(16) glm::mat4 view;

		/// The Projection matrix of the camera.
		alignas(16) glm::mat4 projection;

		/// The position of the camera.
		alignas(16) glm::vec3 viewPos;

		alignas(8) glm::vec2 screenSize;

		/// The distance of the Near Plane of the frustum from the camera.
		alignas(4) float nearPlane;

		/// The distance of the Far Plane of the frustum from the camera.
		alignas(4) float farPlane;
	};
	/// @}

private:
	UBlock ubo;
	bool uboDirty;

	glm::vec3 position;
	glm::vec3 direction;
	glm::vec3 left{-1, 0, 0};
	glm::vec3 up{0, 1, 0};
	float fov;
	float aspect;
	float nearPlane;
	float farPlane;

	glm::vec2 screenSize;

public:
	Camera(const glm::vec3& pos, const glm::vec3& direction, const glm::vec3& up, float fov, const glm::vec2& screenSize,
		   float nearPlane = 0.1f, float farPlane = 10.0f)
		: position(pos), direction(glm::normalize(direction)), up(up), fov(fov), aspect(screenSize.x / screenSize.y), screenSize(screenSize), nearPlane(nearPlane),
		  farPlane(farPlane)
	{
		ubo.view = glm::lookAt(position, direction + position, up);
		ubo.projection = glm::perspective(fov, aspect, nearPlane, farPlane);
		ubo.viewPos = position;
		ubo.nearPlane = nearPlane;
		ubo.farPlane = farPlane;
		ubo.screenSize = screenSize;
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
		direction =
			glm::normalize(glm::vec3(glm::sin(right) * glm::cos(up), glm::sin(up), glm::cos(right) * glm::cos(up)));
		uboDirty = true;
	}

	/**
	 * @brief Rotates the camera to face the given direction.
	 *
	 * @param dir The direction to look into.
	 */
	void lookTo(const glm::vec3& dir)
	{
		direction = glm::normalize(dir);
		uboDirty = true;
	}

	/**
	 * @brief Get the CameraUBO from the camera.
	 *
	 * If the data has been changed, recalculate the UBO.
	 * Else only return a reference.
	 */
	const UBlock& getUbo()
	{
		if (uboDirty)
		{
			ubo.view = glm::lookAt(position, direction + position, up);
			ubo.projection = glm::perspective(fov, aspect, nearPlane, farPlane);
			ubo.viewPos = position;
			ubo.nearPlane = nearPlane;
			ubo.farPlane = farPlane;
			ubo.screenSize = screenSize;
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

	inline const glm::vec3& get_direction() const
	{
		return direction;
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
	inline void set_screenSize(const glm::vec2& screen_size)
	{
		aspect = screen_size.x / screen_size.y;
		screenSize = screen_size;
		uboDirty = true;
	}
	/**
	 * @}
	 */
};
} // namespace blaze
