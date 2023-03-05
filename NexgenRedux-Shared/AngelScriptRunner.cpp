#include "AngelScriptRunner.h"
#include "AngelScriptMethods.h"
#include "WindowManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/StringUtility.h>
#include <AngelScript/angelscript.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>
#include <AngelScript/addons/scriptmath/scriptmath.h>
#include <AngelScript/addons/scriptarray/scriptarray.h>
#include <AngelScript/addons/scriptdictionary/scriptdictionary.h>

using namespace Gensys;
using namespace NexgenRedux;
using namespace AngelScript;

namespace {

	asIScriptEngine* m_engine = NULL;
	asIScriptFunction *m_windowIconifyCallback = NULL;
	asIScriptFunction *m_windowMaximizeCallback = NULL;
	asIScriptFunction *m_windowSizeCallback = NULL;
	asIScriptFunction *m_windowFocusCallback = NULL;
	asIScriptFunction *m_windowKeyboardKeyCallback = NULL;
	asIScriptFunction *m_windowKeyboardCharacterCallback = NULL;
	asIScriptFunction *m_windowMouseCursorPositionCallback = NULL;
	asIScriptFunction *m_windowMouseCursorEnterCallback = NULL;
	asIScriptFunction *m_windowMouseButtonCallback = NULL;
	asIScriptFunction *m_windowMouseScrollCallback = NULL;
	asIScriptFunction *m_windowDropCallback = NULL;
	asIScriptFunction *m_windowJoystickCallback = NULL;
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
	std::wstring scriptFile = FileSystem::CombinePath(FileSystem::CombinePath(mediaDirectory, L"Default"), L"main.as");
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
	m_engine = asCreateScriptEngine();
	if (m_engine == NULL)
	{
		return false;
	}

	RegisterStdString(m_engine);
	RegisterScriptMath(m_engine);
	RegisterScriptArray(m_engine, true);
	RegisterScriptDictionary(m_engine);

	if (m_engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL) < 0) { return false; }

	if (m_engine->RegisterObjectType("MonitorVideoMode", sizeof(WindowManager::MonitorVideoMode), asOBJ_VALUE | asOBJ_POD) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint monitorIndex", asOFFSET(WindowManager::MonitorVideoMode, monitorIndex)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint width", asOFFSET(WindowManager::MonitorVideoMode, width)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint height", asOFFSET(WindowManager::MonitorVideoMode, height)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint redBits", asOFFSET(WindowManager::MonitorVideoMode, redBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint greenBits", asOFFSET(WindowManager::MonitorVideoMode, greenBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint blueBits", asOFFSET(WindowManager::MonitorVideoMode, blueBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint refreshRate", asOFFSET(WindowManager::MonitorVideoMode, refreshRate)) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("void DebugPrint(int logLevel, string &in)", asFUNCTION(AngelScriptMethods::DebugPrint), asCALL_GENERIC) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("uint GetAvailableMonitorCount(void)", asFUNCTION(AngelScriptMethods::GetAvailableMonitorCount), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("MonitorVideoMode GetMonitorVideoMode(uint monitorIndex)", asFUNCTION(AngelScriptMethods::GetMonitorVideoMode), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("array<MonitorVideoMode> @GetMonitorVideoModes(uint monitorIndex)", asFUNCTION(AngelScriptMethods::GetMonitorVideoModes), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, string &in)", asFUNCTION(AngelScriptMethods::WindowCreateWithVideoMode), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint WindowCreateWithSize(uint width, uint height, string &in)", asFUNCTION(AngelScriptMethods::WindowCreateWithSize), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void WindowClose(uint windowHandle)", asFUNCTION(AngelScriptMethods::WindowClose), asCALL_GENERIC) < 0) { return false; }

	if (m_engine->RegisterFuncdef("void WindowIconifyCallback(uint windowHandle, uint iconified)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowIconifyCallback(WindowIconifyCallback @windowIconifyCallback)", asFUNCTION(AngelScriptRunner::SetWindowIconifyCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowMaximizeCallback(uint windowHandle, uint maximized)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowMaximizeCallback(WindowMaximizeCallback @windowMaximizeCallback)", asFUNCTION(AngelScriptRunner::SetWindowMaximizeCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowSizeCallback(uint windowHandle, uint width, uint height)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowSizeCallback(WindowSizeCallback @windowSizeCallback)", asFUNCTION(AngelScriptRunner::SetWindowSizeCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowFocusCallback(uint windowHandle, uint focused)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowFocusCallback(WindowFocusCallback @windowFocusCallback)", asFUNCTION(AngelScriptRunner::SetWindowFocusCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowKeyboardKeyCallback(uint windowHandle, uint key, uint scancode, uint action, uint modifier)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowKeyboardKeyCallback(WindowKeyboardKeyCallback @windowKeyboardKeyCallback)", asFUNCTION(AngelScriptRunner::SetWindowKeyboardKeyCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowKeyboardCharacterCallback(uint windowHandle, uint codepoint)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowKeyboardCharacterCallback(WindowKeyboardCharacterCallback @windowKeyboardCharacterCallback)", asFUNCTION(AngelScriptRunner::SetWindowKeyboardCharacterCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowMouseCursorPositionCallback(uint windowHandle, double xPos, double yPos)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowMouseCursorPositionCallback(WindowMouseCursorPositionCallback @windowMouseCursorPositionCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseCursorPositionCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowMouseCursorEnterCallback(uint windowHandle, uint entered)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowMouseCursorEnterCallback(WindowMouseCursorEnterCallback @windowMouseCursorEnterCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseCursorEnterCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowMouseButtonCallback(uint windowHandle, uint button, uint action, uint modifier)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowMouseButtonCallback(WindowMouseButtonCallback @windowMouseButtonCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseButtonCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowMouseScrollCallback(uint windowHandle, double xOffset, double yOffset)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowMouseScrollCallback(WindowMouseScrollCallback @windowMouseScrollCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseScrollCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowDropCallback(uint windowHandle, array<string> paths)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowDropCallback(WindowDropCallback @windowDropCallback)", asFUNCTION(AngelScriptRunner::SetWindowDropCallback), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterFuncdef("void WindowJoystickCallback(uint windowHandle, uint joystickID, uint event)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowJoystickCallback(WindowJoystickCallback @windowJoystickCallback)", asFUNCTION(AngelScriptRunner::SetWindowJoystickCallback), asCALL_GENERIC) < 0) { return false; }

	//if (m_engine->RegisterGlobalFunction("array<uint> @Test1(void)", asFUNCTION(AngelScriptMethods::Test1), asCALL_GENERIC) < 0) { return false; }
	//if (m_engine->RegisterGlobalFunction("Vec2 Test2(void)", asFUNCTION(AngelScriptMethods::Test2), asCALL_GENERIC) < 0) { return false; }
	//if (m_engine->RegisterGlobalFunction("array<Vec2> @Test3(void)", asFUNCTION(AngelScriptMethods::Test3), asCALL_GENERIC) < 0) { return false; }


	if (CompileScript(m_engine) == false)
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

bool AngelScriptRunner::ExecuteWindowIconifyCallback(uint32_t windowHandle, uint32_t iconified)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowIconifyCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowIconifyCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, iconified);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowMaximizeCallback(uint32_t windowHandle, uint32_t maximized)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowMaximizeCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowMaximizeCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, maximized);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowSizeCallback(uint32_t windowHandle, uint32_t width, uint32_t height)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowSizeCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowSizeCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, width);
	context->SetArgDWord(2, height);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowFocusCallback(uint32_t windowHandle, uint32_t focused)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowFocusCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowFocusCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, focused);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowKeyboardKeyCallback(uint32_t windowHandle, uint32_t key, uint32_t scancode, uint32_t action, uint32_t modifier)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowKeyboardKeyCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowKeyboardKeyCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, key);
	context->SetArgDWord(2, scancode);
	context->SetArgDWord(3, action);
	context->SetArgDWord(4, modifier);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowKeyboardCharacterCallback(uint32_t windowHandle, uint32_t codepoint)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowKeyboardCharacterCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowKeyboardCharacterCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, codepoint);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowMouseCursorPositionCallback(uint32_t windowHandle, double xPos, double yPos)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowMouseCursorPositionCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowMouseCursorPositionCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDouble(1, xPos);
	context->SetArgDouble(2, yPos);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowMouseCursorEnterCallback(uint32_t windowHandle, uint32_t entered)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowMouseCursorEnterCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowMouseCursorEnterCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, entered);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowMouseButtonCallback(uint32_t windowHandle, uint32_t button, uint32_t action, uint32_t modifier)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowMouseButtonCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowMouseButtonCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, button);
	context->SetArgDWord(2, action);
	context->SetArgDWord(3, modifier);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowMouseScrollCallback(uint32_t windowHandle, double xOffset, double yOffset)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowMouseScrollCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowMouseScrollCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDouble(1, xOffset);
	context->SetArgDouble(2, yOffset);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowDropCallback(uint32_t windowHandle, std::vector<std::string> paths)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowDropCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowDropCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);

	asITypeInfo *arrayType = m_engine->GetTypeInfoByDecl("array<string>");
	CScriptArray *array = CScriptArray::Create(arrayType, paths.size());
	for (uint32_t i = 0; i < paths.size(); i++)
	{
		((std::string*)(array->At(i)))->assign(paths.at(i));
	}

	context->SetArgObject(1, array);

	uint32_t result = context->Execute();
	context->Release();
	array->Release();

	return ProcessExecuteResult(context, result);
}

bool AngelScriptRunner::ExecuteWindowJoystickCallback(uint32_t windowHandle, uint32_t joystickID, uint32_t event)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_windowJoystickCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_windowJoystickCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, windowHandle);
	context->SetArgDWord(1, joystickID);
	context->SetArgDWord(2, event);
	uint32_t result = context->Execute();
	context->Release();

	return ProcessExecuteResult(context, result);
}

void AngelScriptRunner::Close(void)
{
	if (m_engine != NULL) 
	{
		m_engine->ShutDownAndRelease();
	}
	if (m_windowMouseButtonCallback != NULL) 
	{
		m_windowMouseButtonCallback->Release();
	}
}

// Privates

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

void AngelScriptRunner::SetWindowIconifyCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowIconifyCallback != NULL) 
	{
		m_windowIconifyCallback->Release();
	}
   	m_windowIconifyCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMaximizeCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowMaximizeCallback != NULL) 
	{
		m_windowMaximizeCallback->Release();
	}
   	m_windowMaximizeCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowSizeCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowSizeCallback != NULL) 
	{
		m_windowSizeCallback->Release();
	}
   	m_windowSizeCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowFocusCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowFocusCallback != NULL) 
	{
		m_windowFocusCallback->Release();
	}
   	m_windowFocusCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowKeyboardKeyCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowKeyboardKeyCallback != NULL) 
	{
		m_windowKeyboardKeyCallback->Release();
	}
   	m_windowKeyboardKeyCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowKeyboardCharacterCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowKeyboardCharacterCallback != NULL) 
	{
		m_windowKeyboardCharacterCallback->Release();
	}
   	m_windowKeyboardCharacterCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMouseCursorPositionCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowMouseCursorPositionCallback != NULL) 
	{
		m_windowMouseCursorPositionCallback->Release();
	}
   	m_windowMouseCursorPositionCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMouseCursorEnterCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowMouseCursorEnterCallback != NULL) 
	{
		m_windowMouseCursorEnterCallback->Release();
	}
   	m_windowMouseCursorEnterCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMouseButtonCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowMouseButtonCallback != NULL) 
	{
		m_windowMouseButtonCallback->Release();
	}
   	m_windowMouseButtonCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMouseScrollCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowMouseScrollCallback != NULL) 
	{
		m_windowMouseScrollCallback->Release();
	}
   	m_windowMouseScrollCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowDropCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowDropCallback != NULL) 
	{
		m_windowDropCallback->Release();
	}
   	m_windowDropCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowJoystickCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowJoystickCallback != NULL) 
	{
		m_windowJoystickCallback->Release();
	}
   	m_windowJoystickCallback = callbackFunction;
}
