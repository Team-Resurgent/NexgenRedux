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
		bool Run(void);
	private:
		//void MessageCallback(asSMessageInfo* msg, void* param);
		//void LineCallback(asIScriptContext* ctx, unsigned long* timeOut);
		//void PrintString(asIScriptGeneric* gen);
		void ConfigureEngine(asIScriptEngine* engine);
		bool CompileScript(asIScriptEngine* engine);
	};
}