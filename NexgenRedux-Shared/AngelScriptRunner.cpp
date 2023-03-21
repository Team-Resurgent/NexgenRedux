#include "AngelScriptRunner.h"
#include "AngelScriptMethods.h"
#include "WindowManager.h"
#include "MathUtility.h"
#include "EntityEngine/NodeSprite.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/StringUtility.h>
#include <Gensys/TimeUtility.h>
#include <AngelScript/angelscript.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>
#include <AngelScript/addons/scriptmath/scriptmath.h>
#include <AngelScript/addons/scriptarray/scriptarray.h>
#include <AngelScript/addons/scriptdictionary/scriptdictionary.h>

#include <cstring>

using namespace Gensys;
using namespace NexgenRedux;
using namespace AngelScript;

namespace {


	AngelScriptMethods* m_angelScriptMethods = NULL;

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

	asIScriptFunction *m_joystickConnectCallback = NULL;
}

AngelScriptRunner::AngelScriptRunner(WindowManager *windowManager)
{
	m_windowManager = windowManager;
	m_angelScriptMethods = new AngelScriptMethods(windowManager);
}

AngelScriptRunner::~AngelScriptRunner()
{
	delete m_angelScriptMethods;
	if (m_windowIconifyCallback != NULL) 
	{
		m_windowIconifyCallback->Release();
	}
	if (m_windowMaximizeCallback != NULL) 
	{
		m_windowMaximizeCallback->Release();
	}
	if (m_windowSizeCallback != NULL) 
	{
		m_windowSizeCallback->Release();
	}
	if (m_windowFocusCallback != NULL) 
	{
		m_windowFocusCallback->Release();
	}
	if (m_windowKeyboardKeyCallback != NULL) 
	{
		m_windowKeyboardKeyCallback->Release();
	}
	if (m_windowKeyboardCharacterCallback != NULL) 
	{
		m_windowKeyboardCharacterCallback->Release();
	}
	if (m_windowMouseCursorPositionCallback != NULL) 
	{
		m_windowMouseCursorPositionCallback->Release();
	}
	if (m_windowMouseCursorEnterCallback != NULL) 
	{
		m_windowMouseCursorEnterCallback->Release();
	}
	if (m_windowMouseButtonCallback != NULL) 
	{
		m_windowMouseButtonCallback->Release();
	}
	if (m_windowMouseScrollCallback != NULL) 
	{
		m_windowMouseScrollCallback->Release();
	}
	if (m_windowDropCallback != NULL) 
	{
		m_windowDropCallback->Release();
	}
	if (m_joystickConnectCallback != NULL) 
	{
		m_joystickConnectCallback->Release();
	}
	if (m_engine != NULL) 
	{
		m_engine->ShutDownAndRelease();
	}
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

bool CompileScript(asIScriptEngine* engine, std::wstring launchFolder)
{
	int result;

	std::wstring mediaDirectory;
	if (FileSystem::GetMediaDirectory(mediaDirectory) == false)
	{
		return false;
	}

	std::string script;
	std::wstring scriptFile = FileSystem::CombinePath(FileSystem::CombinePath(mediaDirectory, launchFolder), L"main.as");
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

void AngelScriptRunner::NodeSpriteConstructor(NodeSprite& nodeSprite)
{
	new (&nodeSprite)NodeSprite();
}

void AngelScriptRunner::NodeSpriteDestructor(NodeSprite& nodeSprite)
{
	nodeSprite.~NodeSprite();
}

bool AngelScriptRunner::Init(std::wstring launchFolder)
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

	if (m_engine->RegisterObjectType("MonitorVideoMode", sizeof(MonitorVideoMode), asOBJ_VALUE | asOBJ_POD) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint monitorIndex", asOFFSET(MonitorVideoMode, monitorIndex)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint width", asOFFSET(MonitorVideoMode, width)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint height", asOFFSET(MonitorVideoMode, height)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint redBits", asOFFSET(MonitorVideoMode, redBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint greenBits", asOFFSET(MonitorVideoMode, greenBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint blueBits", asOFFSET(MonitorVideoMode, blueBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint refreshRate", asOFFSET(MonitorVideoMode, refreshRate)) < 0) { return false; }

	if (m_engine->RegisterObjectType("JoystickButtonStates", sizeof(JoystickButtonStates), asOBJ_VALUE | asOBJ_POD) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonA", asOFFSET(JoystickButtonStates, buttonA)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonB", asOFFSET(JoystickButtonStates, buttonB)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonX", asOFFSET(JoystickButtonStates, buttonX)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonY", asOFFSET(JoystickButtonStates, buttonY)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonLeftBumperOrWhite", asOFFSET(JoystickButtonStates, buttonLeftBumperOrWhite)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonRightBumperOrBlack", asOFFSET(JoystickButtonStates, buttonRightBumperOrBlack)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonBack", asOFFSET(JoystickButtonStates, buttonBack)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonStart", asOFFSET(JoystickButtonStates, buttonStart)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonGuide", asOFFSET(JoystickButtonStates, buttonGuide)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonLeftThumb", asOFFSET(JoystickButtonStates, buttonLeftThumb)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonRightThumb", asOFFSET(JoystickButtonStates, buttonRightThumb)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonDpadUp", asOFFSET(JoystickButtonStates, buttonDpadUp)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonDpadRight", asOFFSET(JoystickButtonStates, buttonDpadRight)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonDpadDown", asOFFSET(JoystickButtonStates, buttonDpadDown)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickButtonStates", "uint buttonDpadLeft", asOFFSET(JoystickButtonStates, buttonDpadLeft)) < 0) { return false; }

	if (m_engine->RegisterObjectType("JoystickAxisStates", sizeof(JoystickAxisStates), asOBJ_VALUE | asOBJ_POD) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftX", asOFFSET(JoystickAxisStates, axisLeftX)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftY", asOFFSET(JoystickAxisStates, axisLeftY)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightX", asOFFSET(JoystickAxisStates, axisRightX)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightY", asOFFSET(JoystickAxisStates, axisRightY)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftTrigger", asOFFSET(JoystickAxisStates, axisLeftTrigger)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightTrigger", asOFFSET(JoystickAxisStates, axisRightTrigger)) < 0) { return false; }

	if (m_engine->RegisterObjectType("Time", sizeof(TimeUtility::Time), asOBJ_VALUE | asOBJ_POD) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 month", asOFFSET(TimeUtility::Time, month)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 day", asOFFSET(TimeUtility::Time, day)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 year", asOFFSET(TimeUtility::Time, year)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 hour", asOFFSET(TimeUtility::Time, hour)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 minute", asOFFSET(TimeUtility::Time, minute)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 second", asOFFSET(TimeUtility::Time, second)) < 0) { return false; }

	if (m_engine->RegisterObjectType("Vec2D", sizeof(MathUtility::Vec2D), asOBJ_VALUE | asOBJ_POD) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec2D", "double x", asOFFSET(MathUtility::Vec2D, x)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec2D", "double y", asOFFSET(MathUtility::Vec2D, y)) < 0) { return false; }

	if (m_engine->RegisterObjectType("SizeI", sizeof(MathUtility::SizeI), asOBJ_VALUE | asOBJ_POD) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("SizeI", "int width", asOFFSET(MathUtility::SizeI, width)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("SizeI", "int height", asOFFSET(MathUtility::SizeI, height)) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("void SeedRandom()", asFUNCTION(AngelScriptMethods::SeedRandom), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SeedRandomWithValue(int value)", asFUNCTION(AngelScriptMethods::SeedRandomWithValue), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("double GetRandomDouble()", asFUNCTION(AngelScriptMethods::GetRandomDouble), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("int GetRandomIntInRange(int start, int end)", asFUNCTION(AngelScriptMethods::GetRandomIntInRange), asCALL_GENERIC) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("void DebugPrint(int logLevel, string &in message)", asFUNCTION(AngelScriptMethods::DebugPrint), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void DebugPrintIf(bool condition, int logLevel, string &in message)", asFUNCTION(AngelScriptMethods::DebugPrintIf), asCALL_GENERIC) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("uint GetAvailableMonitorCount(void)", asFUNCTION(AngelScriptMethods::GetAvailableMonitorCount), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("MonitorVideoMode GetMonitorVideoMode(uint monitorIndex)", asFUNCTION(AngelScriptMethods::GetMonitorVideoMode), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("array<MonitorVideoMode> @GetMonitorVideoModes(uint monitorIndex)", asFUNCTION(AngelScriptMethods::GetMonitorVideoModes), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, string &in title)", asFUNCTION(AngelScriptMethods::WindowCreateWithVideoMode), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint WindowCreateWithSize(uint width, uint height, string &in title)", asFUNCTION(AngelScriptMethods::WindowCreateWithSize), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("SizeI GetWindowSize(uint windowHandle)", asFUNCTION(AngelScriptMethods::GetWindowSize), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetCursorMode(uint windowHandle, uint mode)", asFUNCTION(AngelScriptMethods::SetCursorMode), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void WindowClose(uint windowHandle)", asFUNCTION(AngelScriptMethods::WindowClose), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint GetKeyPressed(uint windowHandle, uint key)", asFUNCTION(AngelScriptMethods::GetKeyPressed), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint GetMouseButtonPressed(uint windowHandle, uint button)", asFUNCTION(AngelScriptMethods::GetMouseButtonPressed), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("Vec2D GetMouseCursorPosition(uint windowHandle)", asFUNCTION(AngelScriptMethods::GetMouseCursorPosition), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetMouseCursorPosition(uint windowHandle, double xPos, double yPos)", asFUNCTION(AngelScriptMethods::SetMouseCursorPosition), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("string GetClipboardString(void)", asFUNCTION(AngelScriptMethods::GetClipboardString), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetClipboardString(string)", asFUNCTION(AngelScriptMethods::SetClipboardString), asCALL_GENERIC) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("Time GetTimeNow()", asFUNCTION(AngelScriptMethods::GetTimeNow), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint64 GetMillisecondsNow()", asFUNCTION(AngelScriptMethods::GetMillisecondsNow), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("double GetDurationSeconds(uint64 start, uint64 end)", asFUNCTION(AngelScriptMethods::GetDurationSeconds), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("double CalculateFramesPerSecond(uint frameCount, double duration)", asFUNCTION(AngelScriptMethods::CalculateFramesPerSecond), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SleepMilliseconds(uint milliseconds)", asFUNCTION(AngelScriptMethods::SleepMilliseconds), asCALL_GENERIC) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("bool JoystickIsPresent(uint joystickID)", asFUNCTION(AngelScriptMethods::JoystickIsPresent), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("bool JoystickIsGamepad(uint joystickID)", asFUNCTION(AngelScriptMethods::JoystickIsGamepad), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("JoystickButtonStates GetJoystickButtonStates(uint joystickID)", asFUNCTION(AngelScriptMethods::GetJoystickButtonStates), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("JoystickAxisStates GetJoystickAxisStates(uint joystickID)", asFUNCTION(AngelScriptMethods::GetJoystickAxisStates), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint GetJoystickHatCount(uint joystickID)", asFUNCTION(AngelScriptMethods::GetJoystickHatCount), asCALL_GENERIC) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint GetJoystickHatDirection(uint joystickID, uint hatIndex)", asFUNCTION(AngelScriptMethods::GetJoystickHatDirection), asCALL_GENERIC) < 0) { return false; }

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
	if (m_engine->RegisterFuncdef("void JoystickConnectCallback(uint joystickID, uint event)") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SetJoystickConnectCallback(JoystickConnectCallback @joystickConnectCallback)", asFUNCTION(AngelScriptRunner::SetJoystickConnectCallback), asCALL_GENERIC) < 0) { return false; }

	if (m_engine->RegisterObjectType("NodeSprite", sizeof(NodeSprite), asOBJ_VALUE | asOBJ_APP_CLASS) < 0) { return false; }


	if (m_engine->RegisterObjectBehaviour("NodeSprite", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(NodeSpriteConstructor), asCALL_CDECL_OBJLAST) < 0) { return false; }
	if (m_engine->RegisterObjectBehaviour("NodeSprite", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(NodeSpriteDestructor), asCALL_CDECL_OBJLAST) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("NodeSprite", "void SetTexturePath(string value)", asMETHOD(NodeSprite, SetTexturePath), asCALL_THISCALL) < 0) { return false; }


	if (CompileScript(m_engine, launchFolder) == false)
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

	bool success = Execute(context);
	context->Release();
	return success;
}

bool AngelScriptRunner::ExecuteRender(uint32_t windowHandle, double dt)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	asIScriptFunction *renderFunction = m_engine->GetModule(0)->GetFunctionByDecl("void Render(uint, double)");
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
	context->SetArgDouble(1, dt);

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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

	bool success = Execute(context);
	context->Release();
	return success;
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
	CScriptArray *array = CScriptArray::Create(arrayType, (uint32_t)paths.size());
	for (uint32_t i = 0; i < paths.size(); i++)
	{
		((std::string*)(array->At(i)))->assign(paths.at(i));
	}

	context->SetArgObject(1, array);

	bool success = Execute(context);
	context->Release();
	array->Release();
	return success;
}

bool AngelScriptRunner::ExecuteJoystickConnectCallback(uint32_t joystickID, uint32_t connected)
{
	asIScriptContext *context = m_engine->CreateContext();
	if (context == NULL) 
	{
		return false;
	}

	if (m_joystickConnectCallback == NULL)
	{
		context->Release();
		return false;
	}

	if (context->Prepare(m_joystickConnectCallback) < 0) 
	{
		context->Release();
		return false;
	}

	context->SetArgDWord(0, joystickID);
	context->SetArgDWord(1, connected);

	bool success = Execute(context);
	context->Release();
	return success;
}

// Privates

bool AngelScriptRunner::Execute(asIScriptContext *context)
{
	uint32_t result = context->Execute();
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

void AngelScriptRunner::SetWindowIconifyCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowIconifyCallback != NULL) 
	{
		m_windowIconifyCallback->Release();
	}
   	m_windowIconifyCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMaximizeCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowMaximizeCallback != NULL) 
	{
		m_windowMaximizeCallback->Release();
	}
   	m_windowMaximizeCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowSizeCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowSizeCallback != NULL) 
	{
		m_windowSizeCallback->Release();
	}
   	m_windowSizeCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowFocusCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowFocusCallback != NULL) 
	{
		m_windowFocusCallback->Release();
	}
   	m_windowFocusCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowKeyboardKeyCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowKeyboardKeyCallback != NULL) 
	{
		m_windowKeyboardKeyCallback->Release();
	}
   	m_windowKeyboardKeyCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowKeyboardCharacterCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowKeyboardCharacterCallback != NULL) 
	{
		m_windowKeyboardCharacterCallback->Release();
	}
   	m_windowKeyboardCharacterCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMouseCursorPositionCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowMouseCursorPositionCallback != NULL) 
	{
		m_windowMouseCursorPositionCallback->Release();
	}
   	m_windowMouseCursorPositionCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMouseCursorEnterCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowMouseCursorEnterCallback != NULL) 
	{
		m_windowMouseCursorEnterCallback->Release();
	}
   	m_windowMouseCursorEnterCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMouseButtonCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowMouseButtonCallback != NULL) 
	{
		m_windowMouseButtonCallback->Release();
	}
   	m_windowMouseButtonCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowMouseScrollCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowMouseScrollCallback != NULL) 
	{
		m_windowMouseScrollCallback->Release();
	}
   	m_windowMouseScrollCallback = callbackFunction;
}

void AngelScriptRunner::SetWindowDropCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_windowDropCallback != NULL) 
	{
		m_windowDropCallback->Release();
	}
   	m_windowDropCallback = callbackFunction;
}

void AngelScriptRunner::SetJoystickConnectCallback(asIScriptGeneric* gen)
{
	asIScriptFunction* callbackFunction = (asIScriptFunction*)gen->GetArgObject(0);
	if (m_joystickConnectCallback != NULL) 
	{
		m_joystickConnectCallback->Release();
	}
   	m_joystickConnectCallback = callbackFunction;
}
