#include "Input.h"

#include "Singleton.h"

#include <GLFW/glfw3.h>

bool Input::IsKeyDown(KeyCode keycode)
{
	GLFWwindow* windowHandle = Singleton<GLFWwindow*>::Get();
	int state = glfwGetKey(windowHandle, (int)keycode);
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool Input::IsMouseButtonDown(MouseButton button)
{
	GLFWwindow* windowHandle = Singleton<GLFWwindow*>::Get();
	int state = glfwGetMouseButton(windowHandle, (int)button);
	return state == GLFW_PRESS;
}

glm::vec2 Input::GetMousePosition()
{
	GLFWwindow* windowHandle = Singleton<GLFWwindow*>::Get();

	double x, y;
	glfwGetCursorPos(windowHandle, &x, &y);
	return { (float)x, (float)y };
}

void Input::SetCursorMode(CursorMode mode)
{
	GLFWwindow* windowHandle = Singleton<GLFWwindow*>::Get();
	glfwSetInputMode(windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL + (int)mode);
}