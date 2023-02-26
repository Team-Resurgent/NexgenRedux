#include "AngelScriptRunner.h"

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

	result = CompileScript(m_engine);
	if (result < 0)
	{
		m_engine->Release();
		return false;
	}

	return true;
}

bool AngelScriptRunner::ExecuteCalc(void)
{
	int result;

	asIScriptContext *context = m_engine->CreateContext();
	if (context == 0) 
	{
		m_engine->Release();
		return false;
	}

	asIScriptFunction *calcFunction = m_engine->GetModule(0)->GetFunctionByDecl("float calc(float, float)");
	if (calcFunction == 0)
	{
		context->Release();
		m_engine->Release();
		return false;
	}

	result = context->Prepare(calcFunction);
	if (result < 0) 
	{
		context->Release();
		m_engine->Release();
		return false;
	}

	context->SetArgFloat(0, 3.14159265359f);
	context->SetArgFloat(1, 2.71828182846f);

	bool success = false;

	result = context->Execute();
	if (result == asEXECUTION_FINISHED)
	{
		float returnValue = context->GetReturnFloat();
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Calc Result = %f.", returnValue);
		success = true;
	}
	else if (result == asCONTEXT_NOT_PREPARED)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "AngelScript - Context not prepared.");
		success = false;
	}
	else if (result == asEXECUTION_ABORTED) 
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_WARNING, "AngelScript - Execution aborted.");
		success = false;
	}
	else if (result == asEXECUTION_SUSPENDED) 
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_WARNING, "AngelScript - Execution suspended.");
		success = false;
	}
	else if (result == asEXECUTION_EXCEPTION)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_WARNING, "AngelScript - Exception occured '%s'.", context->GetExceptionString());
		success = false;
	}

	context->Release();
	return success;
}

void AngelScriptRunner::Close(void)
{
	m_engine->ShutDownAndRelease();
}