#pragma once

#include <Gensys/Int.h>

#include <AngelScript/angelscript.h>

using namespace AngelScript;

namespace NexgenRedux
{
	class AngelScriptRunner
	{
	public:

		typedef struct Vec2
		{
			float x;
			float y;
		} Vec2;

		static bool Init(void);
		static bool ExecuteInit(void);
		static bool ExecuteRender(uint32_t windowHandle, float dt);
		static bool ExecuteWindowMouseButtonCallback(uint32_t windowHandle, uint32_t button, uint32_t action, uint32_t modifier);
		static void Close(void);
	private:
		static bool ProcessExecuteResult(asIScriptContext *context, uint32_t result);
		static void SetWindowMouseButtonCallback(asIScriptGeneric* generic);
	};
}