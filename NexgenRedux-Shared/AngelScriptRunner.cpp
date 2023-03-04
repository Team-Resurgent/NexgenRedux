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
	asIScriptFunction *m_windowMouseButtonCallback = NULL;
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

	//if (m_engine->RegisterFuncdef("void WindowIconifyCallback()") < 0) { return false; }
	//if (m_engine->RegisterGlobalFunction("void SetWindowIconifyCallback(WindowIconifyCallback @cb)", asFUNCTION(SetWindowIconifyCallback), asCALL_CDECL) < 0) { return false; }


	if (m_engine->RegisterFuncdef("void WindowMouseButtonCallback(uint windowHandle, uint button, uint action, uint modifier)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetWindowMouseButtonCallback(WindowMouseButtonCallback @windowMouseButtonCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseButtonCallback), asCALL_GENERIC) < 0) { return false; }

    // void WindowIconify(GLFWwindow* window, int iconified)
    // void WindowMaximize(GLFWwindow* window, int maximized)
    // void WindowSize(GLFWwindow* window, int width, int height)
    // void WindowFocus(GLFWwindow* window, int focused)
    // void WindowKeyboardKey(GLFWwindow* window, int key, int scancode, int action, int mods)
    // void WindowKeyboardCharacter(GLFWwindow* window, unsigned int codepoint)
    // void WindowMouseCursorPosition(GLFWwindow* window, double xpos, double ypos)
    // void WindowMouseCursorEnter(GLFWwindow* window, int entered)
    // void WindowMouseButton(GLFWwindow* window, int button, int action, int mods)
    // void WindowMouseScroll(GLFWwindow* window, double xoffset, double yoffset)
    // void WindowDrop(GLFWwindow* window, int count, const char** paths)
    // void WindowJoystick(int jid, int event)

        // glfwSetWindowIconifyCallback(window, WindowIconify);
        // glfwSetWindowMaximizeCallback(window, WindowMaximize);
        // glfwSetWindowSizeCallback(window, WindowSize);
        // glfwSetWindowFocusCallback(window, WindowFocus);
        // glfwSetKeyCallback(window, WindowKeyboardKey);
        // glfwSetCharCallback(window, WindowKeyboardCharacter);
        // glfwSetCursorPosCallback(window, WindowMouseCursorPosition);
        // glfwSetCursorEnterCallback(window, WindowMouseCursorEnter);
        // glfwSetMouseButtonCallback(window, WindowMouseButton);
        // glfwSetScrollCallback(window, WindowMouseScroll);
        // glfwSetDropCallback(window, WindowDrop);
        // glfwSetJoystickCallback(WindowJoystick);

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

void AngelScriptRunner::SetWindowMouseButtonCallback(asIScriptGeneric* generic)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)generic->GetArgObject(0);
	if (m_windowMouseButtonCallback != NULL) 
	{
		m_windowMouseButtonCallback->Release();
	}
   	m_windowMouseButtonCallback = callbackFunction;
}