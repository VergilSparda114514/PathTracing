#pragma once

#include "VulkanHelpers.h"

constexpr glm::vec3 gUpDirection(0.0f, 1.0f, 0.0f);

class Camera
{
public:
	Camera(float verticalFOV, float nearClip, float farClip);

	bool OnUpdate(float ts);
	void OnResize(uint32_t width, uint32_t height);

	const glm::vec3& GetPosition() const { return m_Position; }
	const glm::vec3& GetDirection() const { return m_ForwardDirection; }
	const glm::vec3& GetUp() const { return glm::normalize(glm::cross(GetSide(), GetDirection())); }
	const glm::vec3& GetSide() const { return glm::normalize(glm::cross(GetDirection(), gUpDirection)); }

	void CreateBuffer();
	VulkanHelpers::Buffer& GetBuffer() { return m_Buffer; }

	float GetNearPlane() const { return m_NearClip; }
	float GetFarPlane() const { return m_FarClip; }
	float GetFovY() const { return m_VerticalFOV; }

	float GetRotationSpeed();
private:
	void UpdateBuffer();
private:
	float m_VerticalFOV = 45.0f;
	float m_NearClip = 0.1f;
	float m_FarClip = 100.0f;

	glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
	glm::vec3 m_ForwardDirection{ 0.0f, 0.0f, 0.0f };

	VulkanHelpers::Buffer m_Buffer;

	uint32_t m_MouseHeldFrames = 0;
	glm::vec2 m_LastMousePosition{ 0.0f, 0.0f };

	uint32_t m_ViewportWidth = 0;
	uint32_t m_ViewportHeight = 0;
};
