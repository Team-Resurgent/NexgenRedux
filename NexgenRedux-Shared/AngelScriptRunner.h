#pragma once

#include <Gensys/Int.h>

#include <AngelScript/angelscript.h>

#include <string>
#include <vector>

using namespace AngelScript;

namespace NexgenRedux
{
	class AngelScriptRunner
	{
	public:

		typedef struct Vec2
		{
			float x;
			float y;
		} Vec2;

		static bool Init(void);
		static bool ExecuteInit(void);
		static bool ExecuteRender(uint32_t windowHandle, float dt);
		static bool ExecuteWindowIconifyCallback(uint32_t windowHandle, uint32_t iconified);
		static bool ExecuteWindowMaximizeCallback(uint32_t windowHandle, uint32_t maximized);
		static bool ExecuteWindowSizeCallback(uint32_t windowHandle, uint32_t width, uint32_t height);
		static bool ExecuteWindowFocusCallback(uint32_t windowHandle, uint32_t focused);
		static bool ExecuteWindowKeyboardKeyCallback(uint32_t windowHandle, uint32_t key, uint32_t scancode, uint32_t action, uint32_t modifier);
		static bool ExecuteWindowKeyboardCharacterCallback(uint32_t windowHandle, uint32_t codepoint);
		static bool ExecuteWindowMouseCursorPositionCallback(uint32_t windowHandle, double xPos, double yPos);
		static bool ExecuteWindowMouseCursorEnterCallback(uint32_t windowHandle, uint32_t entered);
		static bool ExecuteWindowMouseButtonCallback(uint32_t windowHandle, uint32_t button, uint32_t action, uint32_t modifier);
		static bool ExecuteWindowMouseScrollCallback(uint32_t windowHandle, double xOffset, double yOffset);
		static bool ExecuteWindowDropCallback(uint32_t windowHandle, std::vector<std::string> paths);
		static bool ExecuteWindowJoystickCallback(uint32_t windowHandle, uint32_t joystickID, uint32_t event);
		static void Close(void);
	private:
		static bool ProcessExecuteResult(asIScriptContext *context, uint32_t result);	
		static void SetWindowIconifyCallback(asIScriptGeneric* generic);
		static void SetWindowMaximizeCallback(asIScriptGeneric* generic);
		static void SetWindowSizeCallback(asIScriptGeneric* generic);
		static void SetWindowFocusCallback(asIScriptGeneric* generic);
		static void SetWindowKeyboardKeyCallback(asIScriptGeneric* generic);
		static void SetWindowKeyboardCharacterCallback(asIScriptGeneric* generic);
		static void SetWindowMouseCursorPositionCallback(asIScriptGeneric* generic);
		static void SetWindowMouseCursorEnterCallback(asIScriptGeneric* generic);
		static void SetWindowMouseButtonCallback(asIScriptGeneric* generic);
		static void SetWindowMouseScrollCallback(asIScriptGeneric* generic);
		static void SetWindowDropCallback(asIScriptGeneric* generic);
		static void SetWindowJoystickCallback(asIScriptGeneric* generic);

	};
}