#include "AngelScriptRunner.h"
#include "AngelScriptMethods.h"
#include "WindowManager.h"
#include "MathUtility.h"
#include "EntityEngine/SceneManager.h"
#include "EntityEngine/NodeManager.h"
#include "EntityEngine/Sprite.h"

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
		DebugUtility::LogMessage(logLevel, StringUtility::FormatString("%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message));
	} 
	else 
	{
		DebugUtility::LogMessage(logLevel, StringUtility::FormatString("(%d, %d) : %s", msg->row, msg->col, msg->message));
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

// Vec2D

void AngelScriptRunner::Vec2DConstructorDefault(MathUtility::Vec2D* Vec2D)
{
    new (Vec2D)MathUtility::Vec2D();
}

void AngelScriptRunner::Vec2DConstructorValues(double x, double y, MathUtility::Vec2D* Vec2D)
{
    new (Vec2D)MathUtility::Vec2D(x, y);
}

void AngelScriptRunner::Vec2DConstructorCopy(const MathUtility::Vec2D& other, MathUtility::Vec2D* Vec2D)
{
    new (Vec2D)MathUtility::Vec2D(other);
}

// Vec2F

void AngelScriptRunner::Vec2FConstructorDefault(MathUtility::Vec2F* Vec2F)
{
    new (Vec2F)MathUtility::Vec2F();
}

void AngelScriptRunner::Vec2FConstructorValues(float x, float y, MathUtility::Vec2F* Vec2F)
{
    new (Vec2F)MathUtility::Vec2F(x, y);
}

void AngelScriptRunner::Vec2FConstructorCopy(const MathUtility::Vec2F& other, MathUtility::Vec2F* Vec2F)
{
    new (Vec2F)MathUtility::Vec2F(other);
}

// Vec3F

void AngelScriptRunner::Vec3FConstructorDefault(MathUtility::Vec3F* vec3F)
{
	new (vec3F)MathUtility::Vec3F();
}

void AngelScriptRunner::Vec3FConstructorValues(float x, float y, float z, MathUtility::Vec3F* vec3F)
{
	new (vec3F)MathUtility::Vec3F(x, y, z);
}

void AngelScriptRunner::Vec3FConstructorCopy(const MathUtility::Vec3F& other, MathUtility::Vec3F* vec3F)
{
	new (vec3F)MathUtility::Vec3F(other);
}

// Vec4F

void AngelScriptRunner::Vec4FConstructorDefault(MathUtility::Vec4F* vec4F)
{
	new (vec4F)MathUtility::Vec4F();
}

void AngelScriptRunner::Vec4FConstructorValues(float x, float y, float z, float w, MathUtility::Vec4F* vec4F)
{
	new (vec4F)MathUtility::Vec4F(x, y, z, w);
}

void AngelScriptRunner::Vec4FConstructorCopy(const MathUtility::Vec4F& other, MathUtility::Vec4F* vec4F)
{
	new (vec4F)MathUtility::Vec4F(other);
}

// SizeI

void AngelScriptRunner::SizeIConstructorDefault(MathUtility::SizeI* sizeI)
{
    new (sizeI)MathUtility::SizeI();
}

void AngelScriptRunner::SizeIConstructorValues(uint32_t x, uint32_t y, MathUtility::SizeI* sizeI)
{
    new (sizeI)MathUtility::SizeI(x, y);
}

void AngelScriptRunner::SizeIConstructorCopy(const MathUtility::SizeI& other, MathUtility::SizeI* sizeI)
{
    new (sizeI)MathUtility::SizeI(other);
}

// SizeF

void AngelScriptRunner::SizeFConstructorDefault(MathUtility::SizeF* sizeF)
{
    new (sizeF)MathUtility::SizeF();
}

void AngelScriptRunner::SizeFConstructorValues(float x, float y, MathUtility::SizeF* sizeF)
{
    new (sizeF)MathUtility::SizeF(x, y);
}

void AngelScriptRunner::SizeFConstructorCopy(const MathUtility::SizeF& other, MathUtility::SizeF* sizeF)
{
    new (sizeF)MathUtility::SizeF(other);
}

// RectI

void AngelScriptRunner::RectIConstructorDefault(MathUtility::RectI* rectI)
{
	new (rectI)MathUtility::RectI();
}

void AngelScriptRunner::RectIConstructorValues(uint32_t x, uint32_t y, uint32_t width, uint32_t height, MathUtility::RectI* rectI)
{
	new (rectI)MathUtility::RectI(x, y, width, height);
}

void AngelScriptRunner::RectIConstructorCopy(const MathUtility::RectI& other, MathUtility::RectI* rectI)
{
	new (rectI)MathUtility::RectI(other);
}

// RectF

void AngelScriptRunner::RectFConstructorDefault(MathUtility::RectF* rectF)
{
	new (rectF)MathUtility::RectF();
}

void AngelScriptRunner::RectFConstructorValues(float x, float y, float width, float height, MathUtility::RectF* rectF)
{
	new (rectF)MathUtility::RectF(x, y, width, height);
}

void AngelScriptRunner::RectFConstructorCopy(const MathUtility::RectF& other, MathUtility::RectF* rectF)
{
	new (rectF)MathUtility::RectF(other);
}

// Color3F

void AngelScriptRunner::Color3FConstructorDefault(MathUtility::Color3F* color3F)
{
    new (color3F)MathUtility::Color3F();
}

void AngelScriptRunner::Color3FConstructorValues(float r, float g, float b, MathUtility::Color3F* color3F)
{
    new (color3F)MathUtility::Color3F(r, g, b);
}

void AngelScriptRunner::Color3FConstructorCopy(const MathUtility::Color3F& other, MathUtility::Color3F* color3F)
{
    new (color3F)MathUtility::Color3F(other);
}

// Color4F

void AngelScriptRunner::Color4FConstructorDefault(MathUtility::Color4F* color4F)
{
    new (color4F)MathUtility::Color4F();
}

void AngelScriptRunner::Color4FConstructorValues(float r, float g, float b, float a, MathUtility::Color4F* color4F)
{
    new (color4F)MathUtility::Color4F(r, g, b, a);
}

void AngelScriptRunner::Color4FConstructorCopy(const MathUtility::Color4F& other, MathUtility::Color4F* color4F)
{
    new (color4F)MathUtility::Color4F(other);
}

template<class A, class B>
B* refCast(A* a)
{
    if (a == NULL) 
	{
		return NULL;
	}
    B* b = dynamic_cast<B*>(a);
    return b;
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

	if (m_engine->RegisterObjectType("MonitorVideoMode", sizeof(MonitorVideoMode), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint monitorIndex", asOFFSET(MonitorVideoMode, monitorIndex)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint width", asOFFSET(MonitorVideoMode, width)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint height", asOFFSET(MonitorVideoMode, height)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint redBits", asOFFSET(MonitorVideoMode, redBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint greenBits", asOFFSET(MonitorVideoMode, greenBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint blueBits", asOFFSET(MonitorVideoMode, blueBits)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("MonitorVideoMode", "uint refreshRate", asOFFSET(MonitorVideoMode, refreshRate)) < 0) { return false; }

	if (m_engine->RegisterObjectType("JoystickButtonStates", sizeof(JoystickButtonStates), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
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

	if (m_engine->RegisterObjectType("JoystickAxisStates", sizeof(JoystickAxisStates), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftX", asOFFSET(JoystickAxisStates, axisLeftX)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftY", asOFFSET(JoystickAxisStates, axisLeftY)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightX", asOFFSET(JoystickAxisStates, axisRightX)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightY", asOFFSET(JoystickAxisStates, axisRightY)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftTrigger", asOFFSET(JoystickAxisStates, axisLeftTrigger)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightTrigger", asOFFSET(JoystickAxisStates, axisRightTrigger)) < 0) { return false; }

	if (m_engine->RegisterObjectType("Time", sizeof(TimeUtility::Time), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 month", asOFFSET(TimeUtility::Time, month)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 day", asOFFSET(TimeUtility::Time, day)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 year", asOFFSET(TimeUtility::Time, year)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 hour", asOFFSET(TimeUtility::Time, hour)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 minute", asOFFSET(TimeUtility::Time, minute)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Time", "uint16 second", asOFFSET(TimeUtility::Time, second)) < 0) { return false; }

    if (m_engine->RegisterObjectType("Vec2D", sizeof(MathUtility::Vec2D), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec2D", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec2DConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec2D", asBEHAVE_CONSTRUCT, "void f(double, double)", asFUNCTION(Vec2DConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec2D", asBEHAVE_CONSTRUCT, "void f(Vec2D &in)", asFUNCTION(Vec2DConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec2D", "float x", asOFFSET(MathUtility::Vec2D, x)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec2D", "float y", asOFFSET(MathUtility::Vec2D, y)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "Vec2D opDiv(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator/, (const MathUtility::Vec2D &) const, MathUtility::Vec2D), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "Vec2D opMul(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator*, (const MathUtility::Vec2D &) const, MathUtility::Vec2D), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "Vec2D opAdd(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator+, (const MathUtility::Vec2D &) const, MathUtility::Vec2D), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "Vec2D opSub(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator-, (const MathUtility::Vec2D &) const, MathUtility::Vec2D), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "Vec2D &opDivAssign(const Vec2D &in)", asMETHODPR(MathUtility::Vec2D, operator/=, (const MathUtility::Vec2D &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "Vec2D& opMulAssign(const Vec2D &in)", asMETHODPR(MathUtility::Vec2D, operator*=, (const MathUtility::Vec2D &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "Vec2D& opAddAssign(const Vec2D &in)", asMETHODPR(MathUtility::Vec2D, operator+=, (const MathUtility::Vec2D &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "Vec2D& opSubAssign(const Vec2D &in)", asMETHODPR(MathUtility::Vec2D, operator-=, (const MathUtility::Vec2D &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "bool opEq(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator==, (const MathUtility::Vec2D &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2D", "bool opNotEq(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator!=, (const MathUtility::Vec2D &) const, bool), asCALL_THISCALL) < 0) { return false; }

	if (m_engine->RegisterObjectType("Vec2F", sizeof(MathUtility::Vec2F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec2F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec2FConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec2F", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(Vec2FConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec2F", asBEHAVE_CONSTRUCT, "void f(Vec2F &in)", asFUNCTION(Vec2FConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec2F", "float x", asOFFSET(MathUtility::Vec2F, x)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec2F", "float y", asOFFSET(MathUtility::Vec2F, y)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "Vec2F opDiv(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator/, (const MathUtility::Vec2F &) const, MathUtility::Vec2F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "Vec2F opMul(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator*, (const MathUtility::Vec2F &) const, MathUtility::Vec2F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "Vec2F opAdd(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator+, (const MathUtility::Vec2F &) const, MathUtility::Vec2F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "Vec2F opSub(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator-, (const MathUtility::Vec2F &) const, MathUtility::Vec2F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "Vec2F &opDivAssign(const Vec2F &in)", asMETHODPR(MathUtility::Vec2F, operator/=, (const MathUtility::Vec2F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "Vec2F& opMulAssign(const Vec2F &in)", asMETHODPR(MathUtility::Vec2F, operator*=, (const MathUtility::Vec2F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "Vec2F& opAddAssign(const Vec2F &in)", asMETHODPR(MathUtility::Vec2F, operator+=, (const MathUtility::Vec2F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "Vec2F& opSubAssign(const Vec2F &in)", asMETHODPR(MathUtility::Vec2F, operator-=, (const MathUtility::Vec2F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "bool opEq(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator==, (const MathUtility::Vec2F &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec2F", "bool opNotEq(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator!=, (const MathUtility::Vec2F &) const, bool), asCALL_THISCALL) < 0) { return false; }

    if (m_engine->RegisterObjectType("Vec3F", sizeof(MathUtility::Vec3F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec3F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3FConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec3F", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(Vec3FConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec3F", asBEHAVE_CONSTRUCT, "void f(Vec3F &in)", asFUNCTION(Vec3FConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec3F", "float x", asOFFSET(MathUtility::Vec3F, x)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec3F", "float y", asOFFSET(MathUtility::Vec3F, y)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec3F", "float z", asOFFSET(MathUtility::Vec3F, z)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F opDiv(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator/, (const MathUtility::Vec3F &) const, MathUtility::Vec3F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F opMul(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator*, (const MathUtility::Vec3F &) const, MathUtility::Vec3F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F opAdd(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator+, (const MathUtility::Vec3F &) const, MathUtility::Vec3F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F opSub(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator-, (const MathUtility::Vec3F &) const, MathUtility::Vec3F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F &opDivAssign(const Vec3F &in)", asMETHODPR(MathUtility::Vec3F, operator/=, (const MathUtility::Vec3F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F& opMulAssign(const Vec3F &in)", asMETHODPR(MathUtility::Vec3F, operator*=, (const MathUtility::Vec3F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F& opAddAssign(const Vec3F &in)", asMETHODPR(MathUtility::Vec3F, operator+=, (const MathUtility::Vec3F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F& opSubAssign(const Vec3F &in)", asMETHODPR(MathUtility::Vec3F, operator-=, (const MathUtility::Vec3F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "bool opEq(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator==, (const MathUtility::Vec3F &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "bool opNotEq(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator!=, (const MathUtility::Vec3F &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "float Dot(const Vec3F &in)", asMETHOD(MathUtility::Vec3F, Dot), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F Cross(const Vec3F &in)", asMETHOD(MathUtility::Vec3F, Cross), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec3F", "Vec3F Normal()", asMETHOD(MathUtility::Vec3F, Normal), asCALL_THISCALL) < 0) { return false; }

    if (m_engine->RegisterObjectType("Vec4F", sizeof(MathUtility::Vec4F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec4F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec4FConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec4F", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(Vec4FConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Vec4F", asBEHAVE_CONSTRUCT, "void f(Vec4F &in)", asFUNCTION(Vec4FConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec4F", "float x", asOFFSET(MathUtility::Vec4F, x)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec4F", "float y", asOFFSET(MathUtility::Vec4F, y)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Vec4F", "float z", asOFFSET(MathUtility::Vec4F, z)) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("Vec4F", "float w", asOFFSET(MathUtility::Vec4F, w)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "Vec4F opDiv(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator/, (const MathUtility::Vec4F &) const, MathUtility::Vec4F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "Vec4F opMul(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator*, (const MathUtility::Vec4F &) const, MathUtility::Vec4F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "Vec4F opAdd(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator+, (const MathUtility::Vec4F &) const, MathUtility::Vec4F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "Vec4F opSub(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator-, (const MathUtility::Vec4F &) const, MathUtility::Vec4F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "Vec4F &opDivAssign(const Vec4F &in)", asMETHODPR(MathUtility::Vec4F, operator/=, (const MathUtility::Vec4F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "Vec4F& opMulAssign(const Vec4F &in)", asMETHODPR(MathUtility::Vec4F, operator*=, (const MathUtility::Vec4F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "Vec4F& opAddAssign(const Vec4F &in)", asMETHODPR(MathUtility::Vec4F, operator+=, (const MathUtility::Vec4F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "Vec4F& opSubAssign(const Vec4F &in)", asMETHODPR(MathUtility::Vec4F, operator-=, (const MathUtility::Vec4F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "bool opEq(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator==, (const MathUtility::Vec4F &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Vec4F", "bool opNotEq(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator!=, (const MathUtility::Vec4F &) const, bool), asCALL_THISCALL) < 0) { return false; }

    if (m_engine->RegisterObjectType("SizeI", sizeof(MathUtility::SizeI), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK | asOBJ_APP_CLASS_ALLFLOATS) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("SizeI", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(SizeIConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("SizeI", asBEHAVE_CONSTRUCT, "void f(int, int)", asFUNCTION(SizeIConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("SizeI", asBEHAVE_CONSTRUCT, "void f(SizeI &in)", asFUNCTION(SizeIConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("SizeI", "int width", asOFFSET(MathUtility::SizeI, width)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("SizeI", "int height", asOFFSET(MathUtility::SizeI, height)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("SizeI", "bool opEq(const SizeI &in) const", asMETHODPR(MathUtility::SizeI, operator==, (const MathUtility::SizeI &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("SizeI", "bool opNotEq(const SizeI &in) const", asMETHODPR(MathUtility::SizeI, operator!=, (const MathUtility::SizeI &) const, bool), asCALL_THISCALL) < 0) { return false; }

    if (m_engine->RegisterObjectType("SizeF", sizeof(MathUtility::SizeF), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK | asOBJ_APP_CLASS_ALLFLOATS) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("SizeF", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(SizeFConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("SizeF", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(SizeFConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("SizeF", asBEHAVE_CONSTRUCT, "void f(SizeF &in)", asFUNCTION(SizeFConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("SizeF", "float width", asOFFSET(MathUtility::SizeF, width)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("SizeF", "float height", asOFFSET(MathUtility::SizeF, height)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("SizeF", "bool opEq(const SizeF &in) const", asMETHODPR(MathUtility::SizeF, operator==, (const MathUtility::SizeF &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("SizeF", "bool opNotEq(const SizeF &in) const", asMETHODPR(MathUtility::SizeF, operator!=, (const MathUtility::SizeF &) const, bool), asCALL_THISCALL) < 0) { return false; }

    if (m_engine->RegisterObjectType("RectI", sizeof(MathUtility::RectI), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK | asOBJ_APP_CLASS_ALLFLOATS) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("RectI", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(RectIConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("RectI", asBEHAVE_CONSTRUCT, "void f(int, int, int, int)", asFUNCTION(RectIConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("RectI", asBEHAVE_CONSTRUCT, "void f(RectI &in)", asFUNCTION(RectIConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("RectI", "int x", asOFFSET(MathUtility::RectI, x)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("RectI", "int y", asOFFSET(MathUtility::RectI, y)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("RectI", "int width", asOFFSET(MathUtility::RectI, width)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("RectI", "int height", asOFFSET(MathUtility::RectI, height)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("RectI", "bool opEq(const RectI &in) const", asMETHODPR(MathUtility::RectI, operator==, (const MathUtility::RectI &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("RectI", "bool opNotEq(const RectI &in) const", asMETHODPR(MathUtility::RectI, operator!=, (const MathUtility::RectI &) const, bool), asCALL_THISCALL) < 0) { return false; }

	if (m_engine->RegisterObjectType("RectF", sizeof(MathUtility::RectF), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK | asOBJ_APP_CLASS_ALLFLOATS) < 0) { return false; }
	if (m_engine->RegisterObjectBehaviour("RectF", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(RectFConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
	if (m_engine->RegisterObjectBehaviour("RectF", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(RectFConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
	if (m_engine->RegisterObjectBehaviour("RectF", asBEHAVE_CONSTRUCT, "void f(RectF &in)", asFUNCTION(RectFConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
	if (m_engine->RegisterObjectProperty("RectF", "float x", asOFFSET(MathUtility::RectF, x)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("RectF", "float y", asOFFSET(MathUtility::RectF, y)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("RectF", "float width", asOFFSET(MathUtility::RectF, width)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("RectF", "float height", asOFFSET(MathUtility::RectF, height)) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("RectF", "bool opEq(const RectF &in) const", asMETHODPR(MathUtility::RectF, operator==, (const MathUtility::RectF &) const, bool), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("RectF", "bool opNotEq(const RectF &in) const", asMETHODPR(MathUtility::RectF, operator!=, (const MathUtility::RectF &) const, bool), asCALL_THISCALL) < 0) { return false; }

    if (m_engine->RegisterObjectType("Color3F", sizeof(MathUtility::Color3F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Color3F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Color3FConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Color3F", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(Color3FConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Color3F", asBEHAVE_CONSTRUCT, "void f(Color3F &in)", asFUNCTION(Color3FConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Color3F", "float r", asOFFSET(MathUtility::Color3F, r)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Color3F", "float g", asOFFSET(MathUtility::Color3F, g)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Color3F", "float b", asOFFSET(MathUtility::Color3F, b)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "Color3F opDiv(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator/, (const MathUtility::Color3F &) const, MathUtility::Color3F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "Color3F opMul(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator*, (const MathUtility::Color3F &) const, MathUtility::Color3F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "Color3F opAdd(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator+, (const MathUtility::Color3F &) const, MathUtility::Color3F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "Color3F opSub(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator-, (const MathUtility::Color3F &) const, MathUtility::Color3F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "Color3F &opDivAssign(const Color3F &in)", asMETHODPR(MathUtility::Color3F, operator/=, (const MathUtility::Color3F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "Color3F& opMulAssign(const Color3F &in)", asMETHODPR(MathUtility::Color3F, operator*=, (const MathUtility::Color3F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "Color3F& opAddAssign(const Color3F &in)", asMETHODPR(MathUtility::Color3F, operator+=, (const MathUtility::Color3F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "Color3F& opSubAssign(const Color3F &in)", asMETHODPR(MathUtility::Color3F, operator-=, (const MathUtility::Color3F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "bool opEq(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator==, (const MathUtility::Color3F &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color3F", "bool opNotEq(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator!=, (const MathUtility::Color3F &) const, bool), asCALL_THISCALL) < 0) { return false; }

    if (m_engine->RegisterObjectType("Color4F", sizeof(MathUtility::Color4F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Color4F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Color4FConstructorDefault), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Color4F", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(Color4FConstructorValues), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectBehaviour("Color4F", asBEHAVE_CONSTRUCT, "void f(Color4F &in)", asFUNCTION(Color4FConstructorCopy), asCALL_CDECL_OBJLAST) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Color4F", "float r", asOFFSET(MathUtility::Color4F, r)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Color4F", "float g", asOFFSET(MathUtility::Color4F, g)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Color4F", "float b", asOFFSET(MathUtility::Color4F, b)) < 0) { return false; }
    if (m_engine->RegisterObjectProperty("Color4F", "float a", asOFFSET(MathUtility::Color4F, a)) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "Color4F opDiv(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator/, (const MathUtility::Color4F &) const, MathUtility::Color4F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "Color4F opMul(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator*, (const MathUtility::Color4F &) const, MathUtility::Color4F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "Color4F opAdd(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator+, (const MathUtility::Color4F &) const, MathUtility::Color4F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "Color4F opSub(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator-, (const MathUtility::Color4F &) const, MathUtility::Color4F), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "Color4F &opDivAssign(const Color4F &in)", asMETHODPR(MathUtility::Color4F, operator/=, (const MathUtility::Color4F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "Color4F& opMulAssign(const Color4F &in)", asMETHODPR(MathUtility::Color4F, operator*=, (const MathUtility::Color4F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "Color4F& opAddAssign(const Color4F &in)", asMETHODPR(MathUtility::Color4F, operator+=, (const MathUtility::Color4F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "Color4F& opSubAssign(const Color4F &in)", asMETHODPR(MathUtility::Color4F, operator-=, (const MathUtility::Color4F &), void), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "bool opEq(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator==, (const MathUtility::Color4F &) const, bool), asCALL_THISCALL) < 0) { return false; }
    if (m_engine->RegisterObjectMethod("Color4F", "bool opNotEq(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator!=, (const MathUtility::Color4F &) const, bool), asCALL_THISCALL) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("void SeedRandom()", asFUNCTION(MathUtility::SeedRandom), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SeedRandomWithValue(int value)", asFUNCTION(MathUtility::SeedRandomWithValue), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("double GetRandomDouble()", asFUNCTION(MathUtility::GetRandomDouble), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("int GetRandomIntInRange(int start, int end)", asFUNCTION(MathUtility::GetRandomIntInRange), asCALL_CDECL) < 0) { return false; }

	if (m_engine->RegisterGlobalFunction("void DebugPrintIf(const bool condition, const int logLevel, const string message)", asFUNCTIONPR(DebugUtility::LogMessageIf, (const bool, const DebugUtility::LogLevel, const std::string), void), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void DebugPrint(const int logLevel, const string message)", asFUNCTIONPR(DebugUtility::LogMessage, (const DebugUtility::LogLevel, const std::string), void), asCALL_CDECL) < 0) { return false; }

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

	if (m_engine->RegisterGlobalFunction("Time GetTimeNow()", asFUNCTION(TimeUtility::GetTimeNow), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint64 GetMillisecondsNow()", asFUNCTION(TimeUtility::GetMillisecondsNow), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("double GetDurationSeconds(uint64 start, uint64 end)", asFUNCTION(TimeUtility::GetDurationSeconds), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("double CalculateFramesPerSecond(uint frameCount, double duration)", asFUNCTION(TimeUtility::CalculateFramesPerSecond), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SleepMilliseconds(uint milliseconds)", asFUNCTION(TimeUtility::SleepMilliseconds), asCALL_CDECL) < 0) { return false; }

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

	if (m_engine->RegisterObjectType("Node", sizeof(Node), asOBJ_REF | asOBJ_NOCOUNT) < 0) { return false; }
	
	if (m_engine->RegisterObjectType("Sprite", sizeof(0), asOBJ_REF | asOBJ_NOCOUNT) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "uint GetID()", asMETHOD(Sprite, GetID), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "Vec3F& GetAnchor()", asMETHOD(Sprite, GetAnchor), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "void SetAnchor(Vec3F &in)", asMETHOD(Sprite, SetAnchor), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "Vec3F& GetRotation()", asMETHOD(Sprite, GetRotation), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "void SetRotation(Vec3F &in)", asMETHOD(Sprite, SetRotation), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "Vec3F& GetSkew()", asMETHOD(Sprite, GetSkew), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "void SetSkew(Vec3F &in)", asMETHOD(Sprite, SetSkew), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "Vec3F& GetScale()", asMETHOD(Sprite, GetScale), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "void SetScale(Vec3F &in)", asMETHOD(Sprite, SetScale), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "Vec3F& GetPosition()", asMETHOD(Sprite, GetPosition), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "void SetPosition(Vec3F &in)", asMETHOD(Sprite, SetPosition), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "string GetTexturePath()", asMETHOD(Sprite, GetTexturePath), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "void SetTexturePath(string value)", asMETHOD(Sprite, SetTexturePath), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "RectF& GetUV()", asMETHOD(Sprite, GetUV), asCALL_THISCALL) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "void SetUV(RectF &in)", asMETHOD(Sprite, SetUV), asCALL_THISCALL) < 0) { return false; }

	if (m_engine->SetDefaultNamespace("SceneManager") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("uint SceneManager::CreateScene(bool)", asFUNCTION(SceneManager::CreateScene), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void SceneManager::SetCurrentScene(uint)", asFUNCTION(SceneManager::SetCurrentScene), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("bool SceneManager::AssignNode(Node@, uint)", asFUNCTION(SceneManager::AssignNode), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("bool SceneManager::Update(float)", asFUNCTION(SceneManager::Update), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("bool SceneManager::Render()", asFUNCTION(SceneManager::Render), asCALL_CDECL) < 0) { return false; }

	if (m_engine->SetDefaultNamespace("NodeManager") < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("Sprite& NodeManager::CreateSprite()", asFUNCTION(NodeManager::CreateSprite), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("bool NodeManager::AssignNode(Node@, uint)", asFUNCTION(NodeManager::AssignNode), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("bool NodeManager::AssignNodeAt(Node@, uint, uint)", asFUNCTION(NodeManager::AssignNodeAt), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void NodeManager::DeleteNode(uint)", asFUNCTION(NodeManager::DeleteNode), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("Node@ NodeManager::GetNode(uint)", asFUNCTION(NodeManager::GetNode), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void NodeManager::PurgeNodes()", asFUNCTION(NodeManager::PurgeNodes), asCALL_CDECL) < 0) { return false; }
	if (m_engine->RegisterGlobalFunction("void NodeManager::CheckForOrphans()", asFUNCTION(NodeManager::CheckForOrphans), asCALL_CDECL) < 0) { return false; }

	if (m_engine->RegisterObjectMethod("Node", "Sprite@ opCast()", asFUNCTION((refCast<Node,Sprite>)), asCALL_CDECL_OBJLAST) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Node", "Sprite@ opImplCast()", asFUNCTION((refCast<Node,Sprite>)), asCALL_CDECL_OBJLAST) < 0) { return false; }
	if (m_engine->RegisterObjectMethod("Sprite", "Node@ opImplCast()", asFUNCTION((refCast<Sprite,Node>)), asCALL_CDECL_OBJLAST) < 0) { return false; }
	
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
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("AngelScript - Exception occured '%s' in function '%s'.", context->GetExceptionString(), context->GetExceptionFunction()->GetDeclaration()));
		return false;
	}
	
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("AngelScript - Unknown error occured '%i'.", result));
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
