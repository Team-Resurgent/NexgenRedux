#pragma once

#include "WindowManager.h"

#include <Gensys/Int.h>

#include <AngelScript/angelscript.h>

#include <string>
#include <vector>

using namespace AngelScript;

namespace NexgenRedux
{
	class WindowManager;
	class AngelScriptRunner
	{
	public:

		typedef struct Vec2
		{
			float x;
			float y;
		} Vec2;

		AngelScriptRunner(WindowManager *windowManager);
		~AngelScriptRunner();

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


		static void DebugPrint(asIScriptGeneric* gen);
		static void DebugPrintIf(asIScriptGeneric* gen);

		static void GetAvailableMonitorCount(asIScriptGeneric* gen);
		static void GetMonitorVideoMode(asIScriptGeneric* gen);
		static void GetMonitorVideoModes(asIScriptGeneric* gen);
		static void WindowCreateWithVideoMode(asIScriptGeneric* gen);
		static void WindowCreateWithSize(asIScriptGeneric* gen);
		static void GetWindowSize(asIScriptGeneric* gen);
		static void SetCursorMode(asIScriptGeneric* gen);
		static void WindowClose(asIScriptGeneric* gen);
		static void GetKeyPressed(asIScriptGeneric* gen);
		static void GetMouseButtonPressed(asIScriptGeneric* gen);
		static void GetMouseCursorPosition(asIScriptGeneric* gen);
		static void SetMouseCursorPosition(asIScriptGeneric* gen);
		static void GetClipboardString(asIScriptGeneric* gen);
		static void SetClipboardString(asIScriptGeneric* gen);

		static void GetTimeNow(asIScriptGeneric* gen);
		static void GetMillisecondsNow(asIScriptGeneric* gen);
		static void GetDurationSeconds(asIScriptGeneric* gen);
		static void CalculateFramesPerSecond(asIScriptGeneric* gen);
		static void SleepMilliseconds(asIScriptGeneric* gen);

		static void SeedRandom(asIScriptGeneric* gen);
		static void SeedRandomWithValue(asIScriptGeneric* gen);
		static void GetRandomDouble(asIScriptGeneric* gen);
		static void GetRandomIntInRange(asIScriptGeneric* gen);

		static void JoystickIsPresent(asIScriptGeneric* gen);
		static void JoystickIsGamepad(asIScriptGeneric* gen);
		static void GetJoystickButtonStates(asIScriptGeneric* gen);
		static void GetJoystickAxisStates(asIScriptGeneric* gen);
		static void GetJoystickHatCount(asIScriptGeneric* gen);
		static void GetJoystickHatDirection(asIScriptGeneric* gen);
	private:
		WindowManager *m_windowManager;
	};
}