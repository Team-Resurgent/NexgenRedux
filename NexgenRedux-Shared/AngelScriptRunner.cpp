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

void MessageCallback(asSMessageInfo* msg, void* param)
{
	const char *type = "ERR";
	if (msg->type == asMSGTYPE_WARNING)
	{
		type = "WARN";
	}
	else if (msg->type == asMSGTYPE_INFORMATION)
	{ 
		type = "INFO";
	}

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(type));
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(msg->section));
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(msg->message));
}

void LineCallback(asIScriptContext* ctx, uint32_t* timeOut)
{
	//if (*timeOut < timeGetTime()) 
	//{
	//	ctx->Abort();
	//}
}

void PrintString(asIScriptGeneric* gen)
{
	std::string *str = (std::string*)gen->GetArgAddress(0);
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(*str));
}

void AngelScriptRunner::ConfigureEngine(asIScriptEngine* engine)
{
	RegisterStdString(engine);
	int r = engine->RegisterGlobalFunction("void Print(string &in)", asFUNCTION(PrintString), asCALL_GENERIC);
}

bool AngelScriptRunner::CompileScript(asIScriptEngine* engine)
{
	int r;

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
	r = mod->AddScriptSection("script", &script[0], script.length());
	if (r < 0) 
	{
		return false;
	}
	
	r = mod->Build();
	if (r < 0)
	{
		return false;
	}

	return true;
}


bool AngelScriptRunner::Run(void)
{
	asIScriptEngine *engine = asCreateScriptEngine();
	if( engine == 0 )
	{
		return false;
	}

	engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

	ConfigureEngine(engine);

	int r = CompileScript(engine);
	if (r < 0)
	{
		engine->Release();
		return false;
	}

	asIScriptContext *ctx = engine->CreateContext();
	if( ctx == 0 ) 
	{
		engine->Release();
		return false;
	}

	uint32_t timeOut;
	r = ctx->SetLineCallback(asFUNCTION(LineCallback), &timeOut, asCALL_CDECL);
	if (r < 0)
	{
		ctx->Release();
		engine->Release();
		return false;
	}

	asIScriptFunction *func = engine->GetModule(0)->GetFunctionByDecl("float calc(float, float)");
	if (func == 0)
	{
		ctx->Release();
		engine->Release();
		return false;
	}

	r = ctx->Prepare(func);
	if (r < 0) 
	{
		ctx->Release();
		engine->Release();
		return false;
	}

	ctx->SetArgFloat(0, 3.14159265359f);
	ctx->SetArgFloat(1, 2.71828182846f);

	//timeOut = timeGetTime() + 1000;
	r = ctx->Execute();
	if (r != asEXECUTION_FINISHED)
	{
		if (r == asEXECUTION_ABORTED) 
		{
			int x = 1;
		}
		else if( r == asEXECUTION_EXCEPTION )
		{
			asIScriptFunction *func = ctx->GetExceptionFunction();
		}
		else 
		{
			int x = 1;
		}
	}
	else
	{
		float returnValue = ctx->GetReturnFloat();
		int x = 1;
	}

	ctx->Release();
	engine->ShutDownAndRelease();
	return true;
}