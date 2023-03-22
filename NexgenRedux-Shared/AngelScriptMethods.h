#pragma once

#include <AngelScript/angelscript.h>

using namespace AngelScript;

namespace NexgenRedux
{
	class WindowManager;
	class AngelScriptMethods
	{
	public:

		AngelScriptMethods(WindowManager* windowManager);

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

		static void JoystickIsPresent(asIScriptGeneric* gen);
		static void JoystickIsGamepad(asIScriptGeneric* gen);
		static void GetJoystickButtonStates(asIScriptGeneric* gen);
		static void GetJoystickAxisStates(asIScriptGeneric* gen);
		static void GetJoystickHatCount(asIScriptGeneric* gen);
		static void GetJoystickHatDirection(asIScriptGeneric* gen);
	};
}