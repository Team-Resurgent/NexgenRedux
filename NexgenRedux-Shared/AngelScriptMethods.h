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

		static void Test1(asIScriptGeneric* generic);
		static void Test2(asIScriptGeneric* generic);
		static void Test3(asIScriptGeneric* generic);
	};
}