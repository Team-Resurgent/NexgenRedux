#pragma once

#include <GenSys/Int.h>

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
		static bool ExecuteInit(void);
		static bool ExecuteRender(uint32_t windowHandle, float dt);
		static void Close(void);
	private:
		static bool ProcessExecuteResult(asIScriptContext *context, uint32_t result);
	};
}