#pragma once

#include <AngelScript/angelscript.h>

using namespace AngelScript;

namespace NexgenRedux
{
	class AngelScriptMethods
	{
	public:
		static void DebugPrint(asIScriptGeneric* generic);
		static void GetAvailableMonitorCount(asIScriptGeneric* generic);
		static void GetMonitorVideoMode(asIScriptGeneric* generic);
		static void GetMonitorVideoModes(asIScriptGeneric* generic);
		static void WindowCreateWithVideoMode(asIScriptGeneric* generic);
		static void WindowCreateWithSize(asIScriptGeneric* generic);
		static void SetCursorMode(asIScriptGeneric* generic);
		static void WindowClose(asIScriptGeneric* generic);
		static void GetClipboardString(asIScriptGeneric* generic);
		static void SetClipboardString(asIScriptGeneric* generic);

		static void JoystickIsPresent(asIScriptGeneric* generic);
		static void JoystickIsGamepad(asIScriptGeneric* generic);
		static void GetJoystickButtonStates(asIScriptGeneric* generic);
		static void GetJoystickAxisStates(asIScriptGeneric* generic);

		static void Test1(asIScriptGeneric* generic);
		static void Test2(asIScriptGeneric* generic);
		static void Test3(asIScriptGeneric* generic);
	};
}