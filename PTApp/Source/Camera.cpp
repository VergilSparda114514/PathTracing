#include "Camera.h"

#include "shared_with_shaders.h"

#include <imgui.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Input.h"

Camera::Camera(float verticalFOV, float nearClip, float farClip)
	: m_VerticalFOV(verticalFOV), m_NearClip(nearClip), m_FarClip(farClip)
{
	position = glm::vec3(-14.0f, 2.0f, 2.0f);
	direction = glm::normalize(glm::vec3(-13.0f, 2.0f, 1.75f) - position);
}

bool Camera::OnUpdate(float ts)
{
	vec2 mousePos = Input::GetMousePosition();
	vec2 delta = (mousePos - m_LastMousePosition) * 0.002f;
	m_LastMousePosition = Input::GetMousePosition();

	if (!Input::IsMouseButtonDown(MouseButton::Right) || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
	{
		m_MouseHeldFrames = 0;

		Input::SetCursorMode(CursorMode::Normal);
		UpdateBuffer();

		return false;
	}

	Input::SetCursorMode(CursorMode::Locked);

	glm::vec3 rightDirection = GetSide();

	bool moved = false;

	float speed = 5.0f * (Input::IsKeyDown(KeyCode::LeftShift) ? 2.0f : 1.0f);

	// Movement
	if (Input::IsKeyDown(KeyCode::W))
	{
		position += direction * speed * ts;
		moved = true;
	}
	else if (Input::IsKeyDown(KeyCode::S))
	{
		position -= direction * speed * ts;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::A))
	{
		position -= rightDirection * speed * ts;
		moved = true;
	}
	else if (Input::IsKeyDown(KeyCode::D))
	{
		position += rightDirection * speed * ts;
		moved = true;
	}
	if (Input::IsKeyDown(KeyCode::Q))
	{
		position -= g_UpDirection * speed * ts;
		moved = true;
	}
	else if (Input::IsKeyDown(KeyCode::E))
	{
		position += g_UpDirection * speed * ts;
		moved = true;
	}

	// Rotation
	if ((delta.x != 0.0f || delta.y != 0.0f) && ++m_MouseHeldFrames >= 3)
	{
		float pitchDelta = delta.y * GetRotationSpeed();
		float yawDelta = delta.x * GetRotationSpeed();

		glm::quat q = glm::normalize(glm::cross(glm::angleAxis(-pitchDelta, rightDirection),
			glm::angleAxis(-yawDelta, g_UpDirection)));
		direction = glm::rotate(q, direction);

		moved = true;
	}

	UpdateBuffer();

	return moved;
}

void Camera::OnResize(uint32_t width, uint32_t height)
{
	if (width == m_ViewportWidth && height == m_ViewportHeight)
		return;

	m_ViewportWidth = width;
	m_ViewportHeight = height;
}

void Camera::CreateBuffer()
{
	m_Buffer.Create(sizeof(CameraParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	m_Buffer.UploadData(new CameraParams(), sizeof(CameraParams));
}

void Camera::UpdateBuffer()
{
	CameraParams* params = reinterpret_cast<CameraParams*>(m_Buffer.Map());

	params->camPos = position;
	params->camDir = direction;
	params->camUp = GetUp();
	params->camSide = GetSide();
	params->camNearFarFov = vec3(GetNearPlane(), GetFarPlane(), Deg2Rad(GetFovY()));

	m_Buffer.Unmap();
}