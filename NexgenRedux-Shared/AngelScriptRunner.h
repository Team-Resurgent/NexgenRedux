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

		static bool Init(std::wstring launchFolder);
		static bool ExecuteInit(void);
		static bool ExecuteRender(uint32_t windowHandle, double dt);
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
		static bool ExecuteJoystickConnectCallback(uint32_t joystickID, uint32_t connected);
		static void Close(void);
	private:
		static bool Execute(asIScriptContext *context);	
		static void SetWindowIconifyCallback(asIScriptGeneric* gen);
		static void SetWindowMaximizeCallback(asIScriptGeneric* gen);
		static void SetWindowSizeCallback(asIScriptGeneric* gen);
		static void SetWindowFocusCallback(asIScriptGeneric* gen);
		static void SetWindowKeyboardKeyCallback(asIScriptGeneric* gen);
		static void SetWindowKeyboardCharacterCallback(asIScriptGeneric* gen);
		static void SetWindowMouseCursorPositionCallback(asIScriptGeneric* gen);
		static void SetWindowMouseCursorEnterCallback(asIScriptGeneric* gen);
		static void SetWindowMouseButtonCallback(asIScriptGeneric* gen);
		static void SetWindowMouseScrollCallback(asIScriptGeneric* gen);
		static void SetWindowDropCallback(asIScriptGeneric* gen);
		static void SetJoystickConnectCallback(asIScriptGeneric* gen);

	};
}