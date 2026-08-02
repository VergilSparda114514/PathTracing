// https://github.com/StudioCherno/Walnut

#pragma once

#include "VulkanHelpers.h"

#include "KeyCodes.h"

class Input
{
public:
	static bool GetKey(KeyCode keycode);
	static bool IsMouseButtonDown(MouseButton button);

	static glm::vec2 GetMousePosition();

	static void SetCursorMode(CursorMode mode);
};