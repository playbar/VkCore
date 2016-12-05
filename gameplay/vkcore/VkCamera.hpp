#pragma once
#include "define.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#define MATH_DEG_TO_RAD(x)          ((x) * 0.01745329251994329576923690768489f)

#include "Matrix.h"
#include "Vector3.h"
using namespace vkcore;

class VkCamera
{
private:
	float fov;
	float znear, zfar;

	void updateViewMatrix()
	{
		//glm::mat4 rotM = glm::mat4();
		//glm::mat4 transM;

		Matrix rotM;
		Matrix transM;
		rotM.rotateX(MATH_DEG_TO_RAD(rotation.x));
		rotM.rotateY(MATH_DEG_TO_RAD(rotation.y));
		rotM.rotateZ(MATH_DEG_TO_RAD(rotation.z));

		//rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
		//rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
		//rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		//transM = glm::translate(glm::mat4(), position);

		transM.translate(position);

		if (type == CameraType::firstperson)
		{
			mMatrices.view = rotM * transM;
		}
		else
		{
			mMatrices.view = transM * rotM;
		}
	};
public:
	enum CameraType { lookat, firstperson };
	CameraType type = CameraType::lookat;

	Vector3 rotation;
	Vector3 position;

	float rotationSpeed = 1.0f;
	float movementSpeed = 1.0f;

	struct
	{
		Matrix perspective;
		Matrix view;
		//glm::mat4 perspective;
		//glm::mat4 view;
	} mMatrices;

	struct
	{
		bool left = false;
		bool right = false;
		bool up = false;
		bool down = false;
	} keys;

	bool moving()
	{
		return keys.left || keys.right || keys.up || keys.down;
	}

	void setPerspective(float fov, float aspect, float znear, float zfar)
	{
		this->fov = fov;
		this->znear = znear;
		this->zfar = zfar;
		Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(fov), aspect, znear, zfar, &mMatrices.perspective);
	};

	void updateAspectRatio(float aspect)
	{
		Matrix::createPerspectiveVK(MATH_DEG_TO_RAD(fov), aspect, znear, zfar, &mMatrices.perspective);	
	}

	void setRotation(Vector3 rotation)
	{
		this->rotation = rotation;
		updateViewMatrix();
	};

	void rotate(Vector3 delta)
	{
		this->rotation += delta;
		updateViewMatrix();
	}

	void setTranslation(Vector3 translation)
	{
		this->position = translation;
		updateViewMatrix();
	};

	void translate(Vector3 delta)
	{
		this->position += delta;
		updateViewMatrix();
	}

	void update(float deltaTime)
	{
		if (type == CameraType::firstperson)
		{
			if (moving())
			{
				Vector3 camFront;
				camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
				camFront.y = sin(glm::radians(rotation.x));
				camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
				//camFront = glm::normalize(camFront);
				camFront.normalize();

				float moveSpeed = deltaTime * movementSpeed;

				if (keys.up)
					position += camFront * moveSpeed;
				if (keys.down)
					position -= camFront * moveSpeed;
				if (keys.left)
				{
					//position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
					camFront.cross(Vector3(0.0f, 1.0f, 0.0f));
					position -= camFront.normalize() * moveSpeed;
				}
				if (keys.right)
				{
					//position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
					camFront.cross(Vector3(0.0f, 1.0f, 0.0f));
					position -= camFront.normalize() * moveSpeed;
				}

				updateViewMatrix();
			}
		}
	};

	// Update camera passing separate axis data (gamepad)
	// Returns true if view or position has been changed
	bool updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime)
	{
		bool retVal = false;

		if (type == CameraType::firstperson)
		{
			// Use the common console thumbstick layout		
			// Left = view, right = move

			const float deadZone = 0.0015f;
			const float range = 1.0f - deadZone;

			Vector3 camFront;
			camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
			camFront.y = sin(glm::radians(rotation.x));
			camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
			camFront.normalize();

			//camFront = glm::normalize(camFront);

			float moveSpeed = deltaTime * movementSpeed * 2.0f;
			float rotSpeed = deltaTime * rotationSpeed * 50.0f;
			 
			// Move
			if (fabsf(axisLeft.y) > deadZone)
			{
				float pos = (fabsf(axisLeft.y) - deadZone) / range;
				position -= camFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
				retVal = true;
			}
			if (fabsf(axisLeft.x) > deadZone)
			{
				float pos = (fabsf(axisLeft.x) - deadZone) / range;
				//position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
				camFront.cross(Vector3(0.0f, 1.0f, 0.0f));
				position += camFront.normalize() * pos * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
				retVal = true;
			}

			// Rotate
			if (fabsf(axisRight.x) > deadZone)
			{
				float pos = (fabsf(axisRight.x) - deadZone) / range;
				rotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
				retVal = true;
			}
			if (fabsf(axisRight.y) > deadZone)
			{
				float pos = (fabsf(axisRight.y) - deadZone) / range;
				rotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
				retVal = true;
			}
		}
		else
		{
			// todo: move code from example base class for look-at
		}

		if (retVal)
		{
			updateViewMatrix();
		}

		return retVal;
	}

};