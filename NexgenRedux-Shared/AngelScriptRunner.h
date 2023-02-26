#pragma once

#include <AngelScript/angelscript.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>

using namespace AngelScript;

namespace NexgenRedux
{
	class AngelScriptRunner
	{
	public:
		static bool Init(void);
		static bool ExecuteCalc(void);
		static void Close(void);
	};
}