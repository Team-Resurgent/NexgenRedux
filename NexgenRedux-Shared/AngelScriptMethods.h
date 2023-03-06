#pragma once

#include <AngelScript/angelscript.h>

using namespace AngelScript;

namespace NexgenRedux
{
	class AngelScriptMethods
	{
	public:
		static void DebugPrint(asIScriptGeneric* generic);
		static void DebugPrintIf(asIScriptGeneric* generic);

		static void GetAvailableMonitorCount(asIScriptGeneric* generic);
		static void GetMonitorVideoMode(asIScriptGeneric* generic);
		static void GetMonitorVideoModes(asIScriptGeneric* generic);
		static void WindowCreateWithVideoMode(asIScriptGeneric* generic);
		static void WindowCreateWithSize(asIScriptGeneric* generic);
		static void GetWindowSize(asIScriptGeneric* generic);
		static void SetCursorMode(asIScriptGeneric* generic);
		static void WindowClose(asIScriptGeneric* generic);
		static void GetKeyPressed(asIScriptGeneric* generic);
		static void GetMouseButtonPressed(asIScriptGeneric* generic);
		static void GetMouseCursorPosition(asIScriptGeneric* generic);
		static void SetMouseCursorPosition(asIScriptGeneric* generic);
		static void GetClipboardString(asIScriptGeneric* generic);
		static void SetClipboardString(asIScriptGeneric* generic);

		static void GetTimeNow(asIScriptGeneric* generic);
		static void GetMillisecondsNow(asIScriptGeneric* generic);
		static void GetDurationSeconds(asIScriptGeneric* generic);
		static void CalculateFramesPerSecond(asIScriptGeneric* generic);
		static void SleepMilliseconds(asIScriptGeneric* generic);

		static void JoystickIsPresent(asIScriptGeneric* generic);
		static void JoystickIsGamepad(asIScriptGeneric* generic);
		static void GetJoystickButtonStates(asIScriptGeneric* generic);
		static void GetJoystickAxisStates(asIScriptGeneric* generic);
		static void GetJoystickHatCount(asIScriptGeneric* generic);
		static void GetJoystickHatDirection(asIScriptGeneric* generic);
	};
}