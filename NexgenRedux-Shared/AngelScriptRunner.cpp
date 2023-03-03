#include "AngelScriptRunner.h"
#include "WindowManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/StringUtility.h>
#include <AngelScript/angelscript.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>

using namespace Gensys;
using namespace NexgenRedux;
using namespace AngelScript;

namespace {

	asIScriptEngine* m_engine;

}

void DebugPrint(asIScriptGeneric* generic)
{
	DebugUtility::LogLevel logLevel = (DebugUtility::LogLevel)generic->GetArgDWord(0);
	std::string *message = (std::string*)generic->GetArgAddress(1);
	DebugUtility::LogMessage(logLevel, *message);
}

void WindowCreate(asIScriptGeneric* generic)
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

void MessageCallback(asSMessageInfo* msg, void* param)
{
	DebugUtility::LogLevel logLevel = DebugUtility::LOGLEVEL_ERROR;
	if (msg->type == asMSGTYPE_WARNING)
	{
		logLevel = DebugUtility::LOGLEVEL_WARNING;
	}
	else if (msg->type == asMSGTYPE_INFORMATION)
	{ 
		logLevel = DebugUtility::LOGLEVEL_INFO;
	}
	if (strlen(msg->section) > 0)
	{
		DebugUtility::LogMessage(logLevel, "%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message);
	} 
	else 
	{
		DebugUtility::LogMessage(logLevel, "(%d, %d) : %s", msg->row, msg->col, msg->message);
	}
}

bool CompileScript(asIScriptEngine* engine)
{
	int result;

	std::wstring mediaDirectory;
	if (FileSystem::GetMediaDirectory(mediaDirectory) == false)
	{
		return false;
	}

	std::string script;
	std::wstring scriptFile = FileSystem::CombinePath(FileSystem::CombinePath(mediaDirectory, L"Default"), L"main.nxg");
	if (FileSystem::FileReadAllAsString(scriptFile, &script) == false)
	{
		return false;
	}

	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	result = mod->AddScriptSection("script", &script[0], script.length());
	if (result < 0) 
	{
		return false;
	}
	
	result = mod->Build();
	if (result < 0)
	{
		return false;
	}

	return true;
}

bool AngelScriptRunner::Init(void)
{
	int result;

	m_engine = asCreateScriptEngine();
	if (m_engine == NULL)
	{
		return false;
	}

	m_engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

	RegisterStdString(m_engine);

	result = m_engine->RegisterGlobalFunction("void DebugPrint(int logLevel, string &in)", asFUNCTION(DebugPrint), asCALL_GENERIC) < 0;
	if (result < 0)
	{
		return false;
	}

	result = m_engine->RegisterGlobalFunction("uint WindowCreate(uint width, uint height, string &in)", asFUNCTION(WindowCreate), asCALL_GENERIC) < 0;
	if (result < 0)
	{
		return false;
	}

	result = CompileScript(m_engine);
	if (result < 0)
	{
		m_engine->Release();
		return false;
	}

	return true;
}



bool AngelScriptRunner::ExecuteInit(void)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	asIScriptFunction *initFunction = m_engine->GetModule(0)->GetFunctionByDecl("void Init()");
	if (initFunction == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(initFunction) < 0) 
	{
		context->Release();
		return false;
	}

	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteRender(uint32_t windowHandle, float dt)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	asIScriptFunction *renderFunction = m_engine->GetModule(0)->GetFunctionByDecl("void Render(uint, float)");
	if (renderFunction == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(renderFunction) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgFloat(1, dt);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteCalc(void)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	asIScriptFunction *calcFunction = m_engine->GetModule(0)->GetFunctionByDecl("float calc(float, float)");
	if (calcFunction == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(calcFunction) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgFloat(0, 3.14159265359f);
	context->SetArgFloat(1, 2.71828182846f);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

void AngelScriptRunner::Close(void)
{
	m_engine->ShutDownAndRelease();
}


bool AngelScriptRunner::ProcessExecuteResult(asIScriptContext *context, uint32_t result)
{
	if (result == asEXECUTION_FINISHED)
	{
		return true;
	}
	else if (result == asCONTEXT_NOT_PREPARED)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "AngelScript - Context not prepared.");
		return false;
	}
	else if (result == asEXECUTION_ABORTED) 
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_WARNING, "AngelScript - Execution aborted.");
		return false;
	}
	else if (result == asEXECUTION_SUSPENDED) 
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_WARNING, "AngelScript - Execution suspended.");
		return false;
	}
	else if (result == asEXECUTION_EXCEPTION)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "AngelScript - Exception occured '%s'.", context->GetExceptionString());
		return false;
	}
	
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "AngelScript - Unknown error occured '%i'.", result);
	return false;
}