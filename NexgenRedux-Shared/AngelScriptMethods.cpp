#include "AngelScriptMethods.h"
#include "WindowManager.h"

#include <Gensys/DebugUtility.h>
#include <AngelScript/angelscript.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>

using namespace Gensys;
using namespace NexgenRedux;
using namespace AngelScript;

void AngelScriptMethods::DebugPrint(asIScriptGeneric* generic)
{
	DebugUtility::LogLevel logLevel = (DebugUtility::LogLevel)generic->GetArgDWord(0);
	std::string *message = (std::string*)generic->GetArgAddress(1);
	DebugUtility::LogMessage(logLevel, *message);
}

void AngelScriptMethods::WindowCreate(asIScriptGeneric* generic)
{
	uint32_t width = generic->GetArgDWord(0);
	uint32_t height = generic->GetArgDWord(1);
	std::string* title = (std::string*)generic->GetArgAddress(2);
	uint32_t windowHandle;
	if (WindowManager::WindowCreate(width, height, *title, windowHandle) == false) 
	{
		asIScriptContext *ctx = asGetActiveContext();
		if (ctx) 
		{
			ctx->SetException("WindowCreate failed.");
			return;
		}
	}
	*(uint32_t*)generic->GetAddressOfReturnLocation() = windowHandle;
}