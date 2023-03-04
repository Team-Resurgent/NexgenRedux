#pragma once

#include <AngelScript/angelscript.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>

using namespace AngelScript;

namespace NexgenRedux
{
	class AngelScriptMethods
	{
	public:
		static void DebugPrint(asIScriptGeneric* generic);
		static void WindowCreate(asIScriptGeneric* generic);
	};
}