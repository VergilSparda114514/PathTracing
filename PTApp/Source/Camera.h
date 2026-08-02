// https://github.com/TheCherno/RayTracing

#pragma once

#include "VulkanHelpers.h"

constexpr glm::vec3 g_UpDirection{ 0.0f, 1.0f, 0.0f };

class Camera
{
public:
	Camera(float verticalFOV, float nearClip, float farClip);

	bool OnUpdate(float ts);
	void OnResize(uint32_t width, uint32_t height);

	const glm::vec3& GetUp() const { return glm::normalize(glm::cross(GetSide(), direction)); }
	const glm::vec3& GetSide() const { return glm::normalize(glm::cross(direction, g_UpDirection)); }

	void CreateBuffer();
	VulkanHelpers::Buffer& GetBuffer() { return m_Buffer; }

	constexpr float GetNearPlane() const { return m_NearClip; }
	constexpr float GetFarPlane() const { return m_FarClip; }
	constexpr float GetFovY() const { return m_VerticalFOV; }

	constexpr float GetRotationSpeed() const { return 0.3; }
public:
	glm::vec3 position{ 0.0f };
	glm::vec3 direction{ 0.0f, 0.0f, -1.0f };
private:
	void UpdateBuffer();
private:
	float m_VerticalFOV = 45.0f;
	float m_NearClip = 0.1f;
	float m_FarClip = 100.0f;

	VulkanHelpers::Buffer m_Buffer;

	uint32_t m_MouseHeldFrames = 0;
	glm::vec2 m_LastMousePosition{ 0.0f, 0.0f };

	uint32_t m_ViewportWidth = 0;
	uint32_t m_ViewportHeight = 0;
};
