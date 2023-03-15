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
	};
}