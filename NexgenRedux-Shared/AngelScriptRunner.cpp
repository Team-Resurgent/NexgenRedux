#include "AngelScriptRunner.h"
#include "AngelScriptMethods.h"
#include "AngelScriptDebugger.h"
#include "WindowManager.h"
#include "MathUtility.h"
#include "ConfigLoader.h"
#include "EntityEngine/SceneManager.h"
#include "EntityEngine/FontManager.h"
#include "EntityEngine/NodeManager.h"
#include "EntityEngine/OrthoCamera.h"
#include "EntityEngine/PerspectiveCamera.h"
#include "EntityEngine/Fog.h"
#include "EntityEngine/Lighting.h"
#include "EntityEngine/Sprite.h"
#include "EntityEngine/Text.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/StringUtility.h>
#include <Gensys/TimeUtility.h>
#include <Gensys/Memory.h>
#include <AngelScript/angelscript.h>
#include <AngelScript/addons/scriptbuilder/scriptbuilder.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>
#include <AngelScript/addons/scriptmath/scriptmath.h>
#include <AngelScript/addons/scriptarray/scriptarray.h>
#include <AngelScript/addons/contextmgr/contextmgr.h>
#include <AngelScript/addons/scriptdictionary/scriptdictionary.h>

#if defined NEXGEN_WIN
#include <DocGen/docgen.h>
#endif

// Angelscript DocGen https://github.com/marcakafoddex/angelscript-docgen

// https://github.com/codecat/asdbg/blob/master/asdbg_host/asdbg.h

// TODO: add node defaults to documenation

#include <cstring>
#include <vector>

using namespace Gensys;
using namespace NexgenRedux;
using namespace AngelScript;

namespace {

	asIScriptEngine* m_engine = NULL;
	CScriptBuilder* m_builder = NULL;
	CContextMgr* contextMgr = NULL;

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

int IncludeHandler(const char *include, const char *from, CScriptBuilder* builder, void *user_data)
{
	std::wstring mediaDirectory;
	if (FileSystem::GetMediaDirectory(mediaDirectory) == false)
	{
		return -1;
	}

	std::wstring scriptFile;
	if (ConfigLoader::MapPath(L"skin:", scriptFile) == false)
	{
		return false;
	}
	scriptFile = FileSystem::CombinePath(scriptFile, StringUtility::ToWideString(include));

	std::string script;
	if (FileSystem::FileReadAllAsString(scriptFile, &script) == false)
	{
		return -1;
	}


	int result;

	result = m_builder->AddSectionFromMemory(StringUtility::ToString(scriptFile).c_str(), script.c_str(), (unsigned int)script.size(), 0); if (result < 0) { return -1; }

	return 0;
}

bool CompileScript(asIScriptEngine* engine)
{
	std::wstring mediaDirectory;
	if (FileSystem::GetMediaDirectory(mediaDirectory) == false)
	{
		return false;
	}

	std::wstring scriptFile;
	if (ConfigLoader::MapPath(L"skin:", scriptFile) == false)
	{
		return false;
	}
	scriptFile = FileSystem::CombinePath(scriptFile, L"main.as");

	std::string script;
	if (FileSystem::FileReadAllAsString(scriptFile, &script) == false)
	{
		return false;
	}

	int result;

	m_builder->SetIncludeCallback(IncludeHandler, NULL);

	result = m_builder->StartNewModule(engine, "main"); if (result < 0) { return false; }
	result = m_builder->AddSectionFromMemory(StringUtility::ToString(scriptFile).c_str(), script.c_str(), (unsigned int)script.size(), 0); if (result < 0) { return false; }
	result = m_builder->BuildModule(); if (result < 0) { return false; }

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

bool AngelScriptRunner::Init()
{
	m_engine = asCreateScriptEngine();
	if (m_engine == NULL)
	{
		return false;
	}

	m_builder = new CScriptBuilder();

#if defined NEXGEN_WIN
    ScriptDocumentationOptions docgenOptions;
    docgenOptions.projectName = "Nexgen Redux";
	docgenOptions.outputFile = "NexgenRedux.html";
	docgenOptions.documentationName = "AngelScript Interface";
    docgenOptions.includeArrayInterface = true; 
    docgenOptions.includeStringInterface = true;  
    docgenOptions.includeWeakRefInterface = true; 
    docgenOptions.htmlSafe = false;

	docgenOptions.mainBackgroundColor = "#1e1e1e";
	docgenOptions.mainForegroundColor = "#f1f1f1";
	docgenOptions.headerBackgroundColor = "#2d2d30";
	docgenOptions.headerForegroundColor = "#f1f1f1";
	docgenOptions.headerBorder = "2px solid #337AB7";
	docgenOptions.textHeaderColor = "#337AB7";
	docgenOptions.linkColor = "#C0FFFF";
	docgenOptions.blockBackgroundColor = "#222222";
	docgenOptions.blockBorder = "1px solid #b1b1b1";
	docgenOptions.generatedDateTimeColor = "#666666";
	docgenOptions.asKeywordColor = "#337AB7";
	docgenOptions.asNumberColor = "#F0AD4E";
	docgenOptions.asValueTypeColor = "#5BC0DE";
	docgenOptions.asClassTypeColor = "#5CB85C";	

	docgenOptions.additionalCss = nullptr;							
	docgenOptions.additionalJavascript = nullptr;		

    DocumentationGenerator docGen(m_engine, docgenOptions);
#endif

	RegisterStdString(m_engine);
	RegisterScriptMath(m_engine);
	RegisterScriptArray(m_engine, true);
	RegisterScriptDictionary(m_engine);
   
	int result;

	result = m_engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL); if (result < 0) { return false; }
	
	result = m_engine->RegisterEnum("LogLevel"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("LogLevel", "LogLevelInfo", DebugUtility::LOGLEVEL_INFO); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("LogLevel", "LogLevelWarning", DebugUtility::LOGLEVEL_WARNING); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("LogLevel", "LogLevelError", DebugUtility::LOGLEVEL_ERROR); if (result < 0) { return false; }
	
	result = m_engine->RegisterEnum("JoystickButtonState"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("JoystickButtonState", "JoystickButtonStateNone", JoystickButtonStateNone); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("JoystickButtonState", "JoystickButtonStatePressed", JoystickButtonStatePressed); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("KeyButtonState"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("KeyButtonState", "KeyButtonStateNone", KeyButtonStateNone); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("KeyButtonState", "KeyButtonStatePressed", KeyButtonStatePressed); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("MouseButtonState"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("MouseButtonState", "MouseButtonStateNone", MouseButtonStateNone); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("MouseButtonState", "MouseButtonStatePressed", MouseButtonStateNone); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("CursorMode"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("CursorMode", "CursorModeNormal", CursorModeNormal); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("CursorMode", "CursorModeHidden", CursorModeHidden); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("CursorMode", "CursorModeDisabled", CursorModeDisabled); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("CursorMode", "CursorModeCaptured", CursorModeCaptured); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("BlendOperation"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendOperation", "BlendOperationDisabled", BlendOperationDisabled); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendOperation", "BlendOperationAdd", BlendOperationAdd); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendOperation", "BlendOperationSubtract", BlendOperationSubtract); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendOperation", "BlendOperationReverseSubtract", BlendOperationReverseSubtract); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("BlendFactor"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorZero", BlendFactorZero); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorOne", BlendFactorOne); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorSrcColor", BlendFactorSrcColor); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorOneMinusSrcColor", BlendFactorOneMinusSrcColor); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorDstColor", BlendFactorDstColor); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorOneMinusDstColor", BlendFactorOneMinusDstColor); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorSrcAlpha", BlendFactorSrcAlpha); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorOneMinusSrcAlpha", BlendFactorOneMinusSrcAlpha); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorDstAlpha", BlendFactorDstAlpha); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorOneMinusDstAlpha", BlendFactorOneMinusDstAlpha); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorConstantColor", BlendFactorConstantColor); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorOneMinusConstantColor", BlendFactorOneMinusConstantColor); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorConstantAlpha", BlendFactorConstantAlpha); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorOneMinusConstantAlpha", BlendFactorOneMinusConstantAlpha); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("BlendFactor", "BlendFactorSrcAlphaSaturate", BlendFactorSrcAlphaSaturate); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("DrawModeOperation"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DrawModeOperation", "DrawModeOperationTriangles", DrawModeOperationTriangles); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DrawModeOperation", "DrawModeOperationLines", DrawModeOperationLines); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("CullingOperation"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("CullingOperation", "CullingOperationDisabled", CullingOperationDisabled); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("CullingOperation", "CullingOperationFront", CullingOperationFront); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("CullingOperation", "CullingOperationBack", CullingOperationBack); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("DepthOperation"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationDisabled", DepthOperationDisabled); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationNever", DepthOperationNever); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationLess", DepthOperationLess); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationEqual", DepthOperationEqual); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationLessOrEqual", DepthOperationLessOrEqual); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationGreater", DepthOperationGreater); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationNotEqual", DepthOperationNotEqual); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationGreaterOrEqual", DepthOperationGreaterOrEqual); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("DepthOperation", "DepthOperationAlways", DepthOperationAlways); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("FogOperation"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("FogOperation", "FogOperationDisabled", FogOperationDisabled); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("FogOperation", "FogOperationExponent", FogOperationExponent); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("FogOperation", "FogOperationExponent2", FogOperationExponent2); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("FogOperation", "FogOperationLinear", FogOperationLinear); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("LightsOperation"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("LightsOperation", "LightsOperationDisabled", LightsOperationDisabled); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("LightsOperation", "LightsOperationEnabled", LightsOperationEnabled); if (result < 0) { return false; }

	result = m_engine->RegisterEnum("LightOperation"); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("LightOperation", "LightOperationDisabled", LightOperationDisabled); if (result < 0) { return false; }
	result = m_engine->RegisterEnumValue("LightOperation", "LightOperationEnabled", LightOperationEnabled); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("MonitorVideoMode", sizeof(MonitorVideoMode), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; } 
#if defined NEXGEN_WIN
	result = docGen.DocumentObjectType(result, "<b>MonitorVideoMode</b> is an object containing video mode properties."); if (result < 0) { return false; }
#endif
	result = m_engine->RegisterObjectProperty("MonitorVideoMode", "uint monitorIndex", asOFFSET(MonitorVideoMode, monitorIndex)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("MonitorVideoMode", "uint width", asOFFSET(MonitorVideoMode, width)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("MonitorVideoMode", "uint height", asOFFSET(MonitorVideoMode, height)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("MonitorVideoMode", "uint redBits", asOFFSET(MonitorVideoMode, redBits)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("MonitorVideoMode", "uint greenBits", asOFFSET(MonitorVideoMode, greenBits)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("MonitorVideoMode", "uint blueBits", asOFFSET(MonitorVideoMode, blueBits)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("MonitorVideoMode", "uint refreshRate", asOFFSET(MonitorVideoMode, refreshRate)); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("JoystickButtonStates", sizeof(JoystickButtonStates), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
#if defined NEXGEN_WIN
	result = docGen.DocumentObjectType(result, "<b>JoystickButtonStates</b> is an object containing joystick button state properties."); if (result < 0) { return false; }
#endif 
	result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonA", asOFFSET(JoystickButtonStates, buttonA)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonB", asOFFSET(JoystickButtonStates, buttonB)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonX", asOFFSET(JoystickButtonStates, buttonX)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonY", asOFFSET(JoystickButtonStates, buttonY)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonLeftBumperOrWhite", asOFFSET(JoystickButtonStates, buttonLeftBumperOrWhite)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonRightBumperOrBlack", asOFFSET(JoystickButtonStates, buttonRightBumperOrBlack)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonBack", asOFFSET(JoystickButtonStates, buttonBack)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonStart", asOFFSET(JoystickButtonStates, buttonStart)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonGuide", asOFFSET(JoystickButtonStates, buttonGuide)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonLeftThumb", asOFFSET(JoystickButtonStates, buttonLeftThumb)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonRightThumb", asOFFSET(JoystickButtonStates, buttonRightThumb)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonDpadUp", asOFFSET(JoystickButtonStates, buttonDpadUp)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonDpadRight", asOFFSET(JoystickButtonStates, buttonDpadRight)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonDpadDown", asOFFSET(JoystickButtonStates, buttonDpadDown)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("JoystickButtonStates", "JoystickButtonState buttonDpadLeft", asOFFSET(JoystickButtonStates, buttonDpadLeft)); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("JoystickAxisStates", sizeof(JoystickAxisStates), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
#if defined NEXGEN_WIN
	result = docGen.DocumentObjectType(result, "<b>JoystickAxisStates</b> is an object containing joystick axis state properties."); if (result < 0) { return false; }
#endif
	result = m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftX", asOFFSET(JoystickAxisStates, axisLeftX)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftY", asOFFSET(JoystickAxisStates, axisLeftY)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightX", asOFFSET(JoystickAxisStates, axisRightX)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightY", asOFFSET(JoystickAxisStates, axisRightY)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisLeftTrigger", asOFFSET(JoystickAxisStates, axisLeftTrigger)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("JoystickAxisStates", "float axisRightTrigger", asOFFSET(JoystickAxisStates, axisRightTrigger)); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("Time", sizeof(TimeUtility::Time), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
#if defined NEXGEN_WIN
	result = docGen.DocumentObjectType(result, "<b>Time</b> is an object containing time and date properties."); if (result < 0) { return false; }
#endif   
	result = m_engine->RegisterObjectProperty("Time", "uint16 month", asOFFSET(TimeUtility::Time, month)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Time", "uint16 day", asOFFSET(TimeUtility::Time, day)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Time", "uint16 year", asOFFSET(TimeUtility::Time, year)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Time", "uint16 hour", asOFFSET(TimeUtility::Time, hour)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Time", "uint16 minute", asOFFSET(TimeUtility::Time, minute)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Time", "uint16 second", asOFFSET(TimeUtility::Time, second)); if (result < 0) { return false; }

    result = m_engine->RegisterObjectType("Vec2D", sizeof(MathUtility::Vec2D), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
#if defined NEXGEN_WIN	
	result = docGen.DocumentObjectType(result, "<b>Vec2D</b> is an object containing x and y double precision properties."); if (result < 0) { return false; }
#endif
	result = m_engine->RegisterObjectProperty("Vec2D", "double x", asOFFSET(MathUtility::Vec2D, x)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec2D", "double y", asOFFSET(MathUtility::Vec2D, y)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec2D", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec2DConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec2D", asBEHAVE_CONSTRUCT, "void f(double, double)", asFUNCTION(Vec2DConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec2D", asBEHAVE_CONSTRUCT, "void f(Vec2D &in)", asFUNCTION(Vec2DConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2D", "Vec2D opDiv(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator/, (const MathUtility::Vec2D &) const, MathUtility::Vec2D), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Vec2D", "Vec2D opMul(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator*, (const MathUtility::Vec2D &) const, MathUtility::Vec2D), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Vec2D", "Vec2D opAdd(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator+, (const MathUtility::Vec2D &) const, MathUtility::Vec2D), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Vec2D", "Vec2D opSub(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator-, (const MathUtility::Vec2D &) const, MathUtility::Vec2D), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Vec2D", "Vec2D opDivAssign(const Vec2D &in)", asMETHODPR(MathUtility::Vec2D, operator/=, (const MathUtility::Vec2D &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2D", "Vec2D opMulAssign(const Vec2D &in)", asMETHODPR(MathUtility::Vec2D, operator*=, (const MathUtility::Vec2D &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2D", "Vec2D opAddAssign(const Vec2D &in)", asMETHODPR(MathUtility::Vec2D, operator+=, (const MathUtility::Vec2D &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2D", "Vec2D opSubAssign(const Vec2D &in)", asMETHODPR(MathUtility::Vec2D, operator-=, (const MathUtility::Vec2D &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2D", "bool opEq(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator==, (const MathUtility::Vec2D &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2D", "bool opNotEq(const Vec2D &in) const", asMETHODPR(MathUtility::Vec2D, operator!=, (const MathUtility::Vec2D &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("Vec2F", sizeof(MathUtility::Vec2F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
#if defined NEXGEN_WIN
	result = docGen.DocumentObjectType(result, "<b>Vec2F</b> is an object containing x and y float precision properties."); if (result < 0) { return false; }
#endif
	result = m_engine->RegisterObjectBehaviour("Vec2F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec2FConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec2F", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(Vec2FConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec2F", asBEHAVE_CONSTRUCT, "void f(Vec2F &in)", asFUNCTION(Vec2FConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec2F", "float x", asOFFSET(MathUtility::Vec2F, x)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec2F", "float y", asOFFSET(MathUtility::Vec2F, y)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "Vec2F opDiv(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator/, (const MathUtility::Vec2F &) const, MathUtility::Vec2F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "Vec2F opMul(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator*, (const MathUtility::Vec2F &) const, MathUtility::Vec2F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "Vec2F opAdd(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator+, (const MathUtility::Vec2F &) const, MathUtility::Vec2F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "Vec2F opSub(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator-, (const MathUtility::Vec2F &) const, MathUtility::Vec2F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "Vec2F opDivAssign(const Vec2F &in)", asMETHODPR(MathUtility::Vec2F, operator/=, (const MathUtility::Vec2F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "Vec2F opMulAssign(const Vec2F &in)", asMETHODPR(MathUtility::Vec2F, operator*=, (const MathUtility::Vec2F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "Vec2F opAddAssign(const Vec2F &in)", asMETHODPR(MathUtility::Vec2F, operator+=, (const MathUtility::Vec2F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "Vec2F opSubAssign(const Vec2F &in)", asMETHODPR(MathUtility::Vec2F, operator-=, (const MathUtility::Vec2F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "bool opEq(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator==, (const MathUtility::Vec2F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec2F", "bool opNotEq(const Vec2F &in) const", asMETHODPR(MathUtility::Vec2F, operator!=, (const MathUtility::Vec2F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

    result = m_engine->RegisterObjectType("Vec3F", sizeof(MathUtility::Vec3F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
#if defined NEXGEN_WIN
	result = docGen.DocumentObjectType(result, "<b>Vec3F</b> is an object containing x, y and z float precision properties."); if (result < 0) { return false; }
#endif   
	result = m_engine->RegisterObjectBehaviour("Vec3F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec3FConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec3F", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(Vec3FConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec3F", asBEHAVE_CONSTRUCT, "void f(Vec3F &in)", asFUNCTION(Vec3FConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec3F", "float x", asOFFSET(MathUtility::Vec3F, x)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec3F", "float y", asOFFSET(MathUtility::Vec3F, y)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec3F", "float z", asOFFSET(MathUtility::Vec3F, z)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F opDiv(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator/, (const MathUtility::Vec3F &) const, MathUtility::Vec3F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F opMul(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator*, (const MathUtility::Vec3F &) const, MathUtility::Vec3F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F opAdd(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator+, (const MathUtility::Vec3F &) const, MathUtility::Vec3F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F opSub(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator-, (const MathUtility::Vec3F &) const, MathUtility::Vec3F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F opDivAssign(const Vec3F &in)", asMETHODPR(MathUtility::Vec3F, operator/=, (const MathUtility::Vec3F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F opMulAssign(const Vec3F &in)", asMETHODPR(MathUtility::Vec3F, operator*=, (const MathUtility::Vec3F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F opAddAssign(const Vec3F &in)", asMETHODPR(MathUtility::Vec3F, operator+=, (const MathUtility::Vec3F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F opSubAssign(const Vec3F &in)", asMETHODPR(MathUtility::Vec3F, operator-=, (const MathUtility::Vec3F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "bool opEq(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator==, (const MathUtility::Vec3F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "bool opNotEq(const Vec3F &in) const", asMETHODPR(MathUtility::Vec3F, operator!=, (const MathUtility::Vec3F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "float Dot(const Vec3F &in)", asMETHOD(MathUtility::Vec3F, Dot), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F Cross(const Vec3F &in)", asMETHOD(MathUtility::Vec3F, Cross), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec3F", "Vec3F Normal()", asMETHOD(MathUtility::Vec3F, Normal), asCALL_THISCALL); if (result < 0) { return false; }

    result = m_engine->RegisterObjectType("Vec4F", sizeof(MathUtility::Vec4F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
#if defined NEXGEN_WIN	
	result = docGen.DocumentObjectType(result, "<b>Vec4F</b> is an object containing x, y, z and w float precision properties."); if (result < 0) { return false; }
#endif 
	result = m_engine->RegisterObjectBehaviour("Vec4F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Vec4FConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec4F", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(Vec4FConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Vec4F", asBEHAVE_CONSTRUCT, "void f(Vec4F &in)", asFUNCTION(Vec4FConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec4F", "float x", asOFFSET(MathUtility::Vec4F, x)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec4F", "float y", asOFFSET(MathUtility::Vec4F, y)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Vec4F", "float z", asOFFSET(MathUtility::Vec4F, z)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("Vec4F", "float w", asOFFSET(MathUtility::Vec4F, w)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "Vec4F opDiv(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator/, (const MathUtility::Vec4F &) const, MathUtility::Vec4F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "Vec4F opMul(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator*, (const MathUtility::Vec4F &) const, MathUtility::Vec4F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "Vec4F opAdd(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator+, (const MathUtility::Vec4F &) const, MathUtility::Vec4F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "Vec4F opSub(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator-, (const MathUtility::Vec4F &) const, MathUtility::Vec4F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "Vec4F opDivAssign(const Vec4F &in)", asMETHODPR(MathUtility::Vec4F, operator/=, (const MathUtility::Vec4F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "Vec4F opMulAssign(const Vec4F &in)", asMETHODPR(MathUtility::Vec4F, operator*=, (const MathUtility::Vec4F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "Vec4F opAddAssign(const Vec4F &in)", asMETHODPR(MathUtility::Vec4F, operator+=, (const MathUtility::Vec4F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "Vec4F opSubAssign(const Vec4F &in)", asMETHODPR(MathUtility::Vec4F, operator-=, (const MathUtility::Vec4F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "bool opEq(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator==, (const MathUtility::Vec4F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Vec4F", "bool opNotEq(const Vec4F &in) const", asMETHODPR(MathUtility::Vec4F, operator!=, (const MathUtility::Vec4F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

    result = m_engine->RegisterObjectType("SizeI", sizeof(MathUtility::SizeI), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("SizeI", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(SizeIConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("SizeI", asBEHAVE_CONSTRUCT, "void f(int, int)", asFUNCTION(SizeIConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("SizeI", asBEHAVE_CONSTRUCT, "void f(SizeI &in)", asFUNCTION(SizeIConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("SizeI", "int width", asOFFSET(MathUtility::SizeI, width)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("SizeI", "int height", asOFFSET(MathUtility::SizeI, height)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("SizeI", "bool opEq(const SizeI &in) const", asMETHODPR(MathUtility::SizeI, operator==, (const MathUtility::SizeI &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("SizeI", "bool opNotEq(const SizeI &in) const", asMETHODPR(MathUtility::SizeI, operator!=, (const MathUtility::SizeI &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

    result = m_engine->RegisterObjectType("SizeF", sizeof(MathUtility::SizeF), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("SizeF", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(SizeFConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("SizeF", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(SizeFConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("SizeF", asBEHAVE_CONSTRUCT, "void f(SizeF &in)", asFUNCTION(SizeFConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("SizeF", "float width", asOFFSET(MathUtility::SizeF, width)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("SizeF", "float height", asOFFSET(MathUtility::SizeF, height)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("SizeF", "bool opEq(const SizeF &in) const", asMETHODPR(MathUtility::SizeF, operator==, (const MathUtility::SizeF &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("SizeF", "bool opNotEq(const SizeF &in) const", asMETHODPR(MathUtility::SizeF, operator!=, (const MathUtility::SizeF &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

    result = m_engine->RegisterObjectType("RectI", sizeof(MathUtility::RectI), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("RectI", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(RectIConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("RectI", asBEHAVE_CONSTRUCT, "void f(int, int, int, int)", asFUNCTION(RectIConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("RectI", asBEHAVE_CONSTRUCT, "void f(RectI &in)", asFUNCTION(RectIConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("RectI", "int x", asOFFSET(MathUtility::RectI, x)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("RectI", "int y", asOFFSET(MathUtility::RectI, y)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("RectI", "int width", asOFFSET(MathUtility::RectI, width)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("RectI", "int height", asOFFSET(MathUtility::RectI, height)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("RectI", "bool opEq(const RectI &in) const", asMETHODPR(MathUtility::RectI, operator==, (const MathUtility::RectI &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("RectI", "bool opNotEq(const RectI &in) const", asMETHODPR(MathUtility::RectI, operator!=, (const MathUtility::RectI &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("RectF", sizeof(MathUtility::RectF), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
	result = m_engine->RegisterObjectBehaviour("RectF", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(RectFConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectBehaviour("RectF", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(RectFConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectBehaviour("RectF", asBEHAVE_CONSTRUCT, "void f(RectF &in)", asFUNCTION(RectFConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectProperty("RectF", "float x", asOFFSET(MathUtility::RectF, x)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("RectF", "float y", asOFFSET(MathUtility::RectF, y)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("RectF", "float width", asOFFSET(MathUtility::RectF, width)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("RectF", "float height", asOFFSET(MathUtility::RectF, height)); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("RectF", "bool opEq(const RectF &in) const", asMETHODPR(MathUtility::RectF, operator==, (const MathUtility::RectF &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("RectF", "bool opNotEq(const RectF &in) const", asMETHODPR(MathUtility::RectF, operator!=, (const MathUtility::RectF &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

    result = m_engine->RegisterObjectType("Color3F", sizeof(MathUtility::Color3F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Color3F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Color3FConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Color3F", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(Color3FConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Color3F", asBEHAVE_CONSTRUCT, "void f(Color3F &in)", asFUNCTION(Color3FConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Color3F", "float r", asOFFSET(MathUtility::Color3F, r)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Color3F", "float g", asOFFSET(MathUtility::Color3F, g)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Color3F", "float b", asOFFSET(MathUtility::Color3F, b)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "Color3F opDiv(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator/, (const MathUtility::Color3F &) const, MathUtility::Color3F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "Color3F opMul(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator*, (const MathUtility::Color3F &) const, MathUtility::Color3F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "Color3F opAdd(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator+, (const MathUtility::Color3F &) const, MathUtility::Color3F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "Color3F opSub(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator-, (const MathUtility::Color3F &) const, MathUtility::Color3F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "Color3F opDivAssign(const Color3F &in)", asMETHODPR(MathUtility::Color3F, operator/=, (const MathUtility::Color3F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "Color3F opMulAssign(const Color3F &in)", asMETHODPR(MathUtility::Color3F, operator*=, (const MathUtility::Color3F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "Color3F opAddAssign(const Color3F &in)", asMETHODPR(MathUtility::Color3F, operator+=, (const MathUtility::Color3F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "Color3F opSubAssign(const Color3F &in)", asMETHODPR(MathUtility::Color3F, operator-=, (const MathUtility::Color3F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "bool opEq(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator==, (const MathUtility::Color3F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color3F", "bool opNotEq(const Color3F &in) const", asMETHODPR(MathUtility::Color3F, operator!=, (const MathUtility::Color3F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

    result = m_engine->RegisterObjectType("Color4F", sizeof(MathUtility::Color4F), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Color4F", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Color4FConstructorDefault), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Color4F", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(Color4FConstructorValues), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectBehaviour("Color4F", asBEHAVE_CONSTRUCT, "void f(Color4F &in)", asFUNCTION(Color4FConstructorCopy), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Color4F", "float r", asOFFSET(MathUtility::Color4F, r)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Color4F", "float g", asOFFSET(MathUtility::Color4F, g)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Color4F", "float b", asOFFSET(MathUtility::Color4F, b)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectProperty("Color4F", "float a", asOFFSET(MathUtility::Color4F, a)); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "Color4F opDiv(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator/, (const MathUtility::Color4F &) const, MathUtility::Color4F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "Color4F opMul(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator*, (const MathUtility::Color4F &) const, MathUtility::Color4F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "Color4F opAdd(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator+, (const MathUtility::Color4F &) const, MathUtility::Color4F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "Color4F opSub(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator-, (const MathUtility::Color4F &) const, MathUtility::Color4F), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "Color4F opDivAssign(const Color4F &in)", asMETHODPR(MathUtility::Color4F, operator/=, (const MathUtility::Color4F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "Color4F opMulAssign(const Color4F &in)", asMETHODPR(MathUtility::Color4F, operator*=, (const MathUtility::Color4F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "Color4F opAddAssign(const Color4F &in)", asMETHODPR(MathUtility::Color4F, operator+=, (const MathUtility::Color4F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "Color4F opSubAssign(const Color4F &in)", asMETHODPR(MathUtility::Color4F, operator-=, (const MathUtility::Color4F &), void), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "bool opEq(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator==, (const MathUtility::Color4F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }
    result = m_engine->RegisterObjectMethod("Color4F", "bool opNotEq(const Color4F &in) const", asMETHODPR(MathUtility::Color4F, operator!=, (const MathUtility::Color4F &) const, bool), asCALL_THISCALL); if (result < 0) { return false; }

	result = m_engine->RegisterGlobalFunction("void SeedRandom()", asFUNCTION(MathUtility::SeedRandom), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SeedRandomWithValue(int value)", asFUNCTION(MathUtility::SeedRandomWithValue), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("double GetRandomDouble()", asFUNCTION(MathUtility::GetRandomDouble), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("int GetRandomIntInRange(int start, int end)", asFUNCTION(MathUtility::GetRandomIntInRange), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("float PI()", asFUNCTION(MathUtility::PI), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("float DegreesToRadians(float degrees)", asFUNCTION(MathUtility::DegreesToRadians), asCALL_CDECL); if (result < 0) { return false; }

	result = m_engine->RegisterGlobalFunction("void DebugPrintIf(const bool condition, const LogLevel logLevel, const string message)", asFUNCTIONPR(DebugUtility::LogMessageIf, (const bool, const DebugUtility::LogLevel, const std::string), void), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void DebugPrint(const LogLevel logLevel, const string message)", asFUNCTIONPR(DebugUtility::LogMessage, (const DebugUtility::LogLevel, const std::string), void), asCALL_CDECL); if (result < 0) { return false; }

	result = m_engine->RegisterGlobalFunction("uint64 GetTotalVirtualMemory()", asFUNCTION(Memory::GetTotalVirtualMemory), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("uint64 GetFreeVirtualMemory()", asFUNCTION(Memory::GetFreeVirtualMemory), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("uint64 GetTotalPhysicalMemory()", asFUNCTION(Memory::GetTotalPhysicalMemory), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("uint64 GetFreePhysicalMemory()", asFUNCTION(Memory::GetFreePhysicalMemory), asCALL_CDECL); if (result < 0) { return false; }

	result = m_engine->RegisterGlobalFunction("uint GetAvailableMonitorCount(void)", asFUNCTION(AngelScriptMethods::GetAvailableMonitorCount), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("MonitorVideoMode GetMonitorVideoMode(uint monitorIndex)", asFUNCTION(AngelScriptMethods::GetMonitorVideoMode), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("array<MonitorVideoMode> @GetMonitorVideoModes(uint monitorIndex)", asFUNCTION(AngelScriptMethods::GetMonitorVideoModes), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, string &in title)", asFUNCTION(AngelScriptMethods::WindowCreateWithVideoMode), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void WindowCreateWithSize(uint width, uint height, string &in title)", asFUNCTION(AngelScriptMethods::WindowCreateWithSize), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("SizeI GetWindowSize()", asFUNCTION(AngelScriptMethods::GetWindowSize), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetCursorMode(CursorMode)", asFUNCTION(AngelScriptMethods::SetCursorMode), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void WindowClose()", asFUNCTION(AngelScriptMethods::WindowClose), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("KeyButtonState GetKeyPressed(uint key)", asFUNCTION(AngelScriptMethods::GetKeyPressed), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("MouseButtonState GetMouseButtonPressed(uint button)", asFUNCTION(AngelScriptMethods::GetMouseButtonPressed), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("Vec2D GetMouseCursorPosition()", asFUNCTION(AngelScriptMethods::GetMouseCursorPosition), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetMouseCursorPosition(double xPos, double yPos)", asFUNCTION(AngelScriptMethods::SetMouseCursorPosition), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("string GetClipboardString(void)", asFUNCTION(AngelScriptMethods::GetClipboardString), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetClipboardString(string)", asFUNCTION(AngelScriptMethods::SetClipboardString), asCALL_GENERIC); if (result < 0) { return false; }

	result = m_engine->RegisterGlobalFunction("Time GetTimeNow()", asFUNCTION(TimeUtility::GetTimeNow), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("uint64 GetMillisecondsNow()", asFUNCTION(TimeUtility::GetMillisecondsNow), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("double GetDurationSeconds(uint64 start, uint64 end)", asFUNCTION(TimeUtility::GetDurationSeconds), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("double CalculateFramesPerSecond(uint frameCount, double duration)", asFUNCTION(TimeUtility::CalculateFramesPerSecond), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SleepMilliseconds(uint milliseconds)", asFUNCTION(TimeUtility::SleepMilliseconds), asCALL_CDECL); if (result < 0) { return false; }

	result = m_engine->RegisterGlobalFunction("bool JoystickIsPresent(uint joystickID)", asFUNCTION(AngelScriptMethods::JoystickIsPresent), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("bool JoystickIsGamepad(uint joystickID)", asFUNCTION(AngelScriptMethods::JoystickIsGamepad), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("JoystickButtonStates GetJoystickButtonStates(uint joystickID)", asFUNCTION(AngelScriptMethods::GetJoystickButtonStates), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("JoystickAxisStates GetJoystickAxisStates(uint joystickID)", asFUNCTION(AngelScriptMethods::GetJoystickAxisStates), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("uint GetJoystickHatCount(uint joystickID)", asFUNCTION(AngelScriptMethods::GetJoystickHatCount), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("uint GetJoystickHatDirection(uint joystickID, uint hatIndex)", asFUNCTION(AngelScriptMethods::GetJoystickHatDirection), asCALL_GENERIC); if (result < 0) { return false; }

	result = m_engine->RegisterFuncdef("void WindowIconifyCallback(uint iconified)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowIconifyCallback(WindowIconifyCallback @windowIconifyCallback)", asFUNCTION(AngelScriptRunner::SetWindowIconifyCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowMaximizeCallback(uint maximized)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowMaximizeCallback(WindowMaximizeCallback @windowMaximizeCallback)", asFUNCTION(AngelScriptRunner::SetWindowMaximizeCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowSizeCallback(uint width, uint height)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowSizeCallback(WindowSizeCallback @windowSizeCallback)", asFUNCTION(AngelScriptRunner::SetWindowSizeCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowFocusCallback(uint focused)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowFocusCallback(WindowFocusCallback @windowFocusCallback)", asFUNCTION(AngelScriptRunner::SetWindowFocusCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowKeyboardKeyCallback(uint key, uint scancode, uint action, uint modifier)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowKeyboardKeyCallback(WindowKeyboardKeyCallback @windowKeyboardKeyCallback)", asFUNCTION(AngelScriptRunner::SetWindowKeyboardKeyCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowKeyboardCharacterCallback(uint codepoint)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowKeyboardCharacterCallback(WindowKeyboardCharacterCallback @windowKeyboardCharacterCallback)", asFUNCTION(AngelScriptRunner::SetWindowKeyboardCharacterCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowMouseCursorPositionCallback(double xPos, double yPos)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowMouseCursorPositionCallback(WindowMouseCursorPositionCallback @windowMouseCursorPositionCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseCursorPositionCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowMouseCursorEnterCallback(uint entered)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowMouseCursorEnterCallback(WindowMouseCursorEnterCallback @windowMouseCursorEnterCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseCursorEnterCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowMouseButtonCallback(uint button, uint action, uint modifier)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowMouseButtonCallback(WindowMouseButtonCallback @windowMouseButtonCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseButtonCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowMouseScrollCallback(double xOffset, double yOffset)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowMouseScrollCallback(WindowMouseScrollCallback @windowMouseScrollCallback)", asFUNCTION(AngelScriptRunner::SetWindowMouseScrollCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void WindowDropCallback(array<string> paths)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetWindowDropCallback(WindowDropCallback @windowDropCallback)", asFUNCTION(AngelScriptRunner::SetWindowDropCallback), asCALL_GENERIC); if (result < 0) { return false; }
	result = m_engine->RegisterFuncdef("void JoystickConnectCallback(uint joystickID, uint event)"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SetJoystickConnectCallback(JoystickConnectCallback @joystickConnectCallback)", asFUNCTION(AngelScriptRunner::SetJoystickConnectCallback), asCALL_GENERIC); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("Node", sizeof(Node), asOBJ_REF | asOBJ_NOCOUNT); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("OrthoCameraNode", sizeof(0), asOBJ_REF | asOBJ_NOCOUNT); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "uint GetID()", asMETHOD(OrthoCamera, GetID), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Vec3F GetAnchor()", asMETHOD(OrthoCamera, GetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetAnchor(Vec3F)", asMETHOD(OrthoCamera, SetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Vec3F GetRotation()", asMETHOD(OrthoCamera, GetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetRotation(Vec3F)", asMETHOD(OrthoCamera, SetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Vec3F GetSkew()", asMETHOD(OrthoCamera, GetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetSkew(Vec3F)", asMETHOD(OrthoCamera, SetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Vec3F GetScale()", asMETHOD(OrthoCamera, GetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetScale(Vec3F)", asMETHOD(OrthoCamera, SetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Vec3F GetPosition()", asMETHOD(OrthoCamera, GetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetPosition(Vec3F)", asMETHOD(OrthoCamera, SetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Color4F GetClearColor()", asMETHOD(OrthoCamera, GetClearColor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetClearColor(Color4F)", asMETHOD(OrthoCamera, SetClearColor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Vec3F GetEye()", asMETHOD(OrthoCamera, GetEye), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetEye(Vec3F)", asMETHOD(OrthoCamera, SetEye), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Vec3F GetTarget()", asMETHOD(OrthoCamera, GetTarget), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetTarget(Vec3F)", asMETHOD(OrthoCamera, SetTarget), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Vec3F GetUp()", asMETHOD(OrthoCamera, GetUp), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetUp(Vec3F)", asMETHOD(OrthoCamera, SetUp), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "float GetLeft()", asMETHOD(OrthoCamera, GetLeft), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetLeft(float)", asMETHOD(OrthoCamera, SetLeft), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "float GetRight()", asMETHOD(OrthoCamera, GetRight), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetRight(float)", asMETHOD(OrthoCamera, SetRight), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "float GetBottom()", asMETHOD(OrthoCamera, GetBottom), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetBottom(float)", asMETHOD(OrthoCamera, SetBottom), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "float GetTop()", asMETHOD(OrthoCamera, GetTop), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetTop(float)", asMETHOD(OrthoCamera, SetTop), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "float GetZNear()", asMETHOD(OrthoCamera, GetZNear), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetZNear(float)", asMETHOD(OrthoCamera, SetZNear), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "float GetZFar()", asMETHOD(OrthoCamera, GetZFar), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "void SetZFar(float)", asMETHOD(OrthoCamera, SetZFar), asCALL_THISCALL); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("PerspectiveCameraNode", sizeof(0), asOBJ_REF | asOBJ_NOCOUNT); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "uint GetID()", asMETHOD(PerspectiveCamera, GetID), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Vec3F GetAnchor()", asMETHOD(PerspectiveCamera, GetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetAnchor(Vec3F)", asMETHOD(PerspectiveCamera, SetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Vec3F GetRotation()", asMETHOD(PerspectiveCamera, GetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetRotation(Vec3F)", asMETHOD(PerspectiveCamera, SetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Vec3F GetSkew()", asMETHOD(PerspectiveCamera, GetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetSkew(Vec3F)", asMETHOD(PerspectiveCamera, SetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Vec3F GetScale()", asMETHOD(PerspectiveCamera, GetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetScale(Vec3F)", asMETHOD(PerspectiveCamera, SetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Vec3F GetPosition()", asMETHOD(PerspectiveCamera, GetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetPosition(Vec3F)", asMETHOD(PerspectiveCamera, SetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Color4F GetClearColor()", asMETHOD(PerspectiveCamera, GetClearColor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetClearColor(Color4F)", asMETHOD(PerspectiveCamera, SetClearColor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Vec3F GetEye()", asMETHOD(PerspectiveCamera, GetEye), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetEye(Vec3F)", asMETHOD(PerspectiveCamera, SetEye), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Vec3F GetTarget()", asMETHOD(PerspectiveCamera, GetTarget), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetTarget(Vec3F)", asMETHOD(PerspectiveCamera, SetTarget), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Vec3F GetUp()", asMETHOD(PerspectiveCamera, GetUp), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetUp(Vec3F)", asMETHOD(PerspectiveCamera, SetUp), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "float GetFOV()", asMETHOD(PerspectiveCamera, GetFOV), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetFOV(float)", asMETHOD(PerspectiveCamera, SetFOV), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "float GetAspectRatio()", asMETHOD(PerspectiveCamera, GetAspectRatio), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetAspectRatio(float)", asMETHOD(PerspectiveCamera, SetAspectRatio), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "float GetZNear()", asMETHOD(PerspectiveCamera, GetZNear), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetZNear(float)", asMETHOD(PerspectiveCamera, SetZNear), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "float GetZFar()", asMETHOD(PerspectiveCamera, GetZFar), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "void SetZFar(float)", asMETHOD(PerspectiveCamera, SetZFar), asCALL_THISCALL); if (result < 0) { return false; }
	
	result = m_engine->RegisterObjectType("SpriteNode", sizeof(0), asOBJ_REF | asOBJ_NOCOUNT); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "uint GetID()", asMETHOD(Sprite, GetID), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "Vec3F GetAnchor()", asMETHOD(Sprite, GetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetAnchor(Vec3F)", asMETHOD(Sprite, SetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "Vec3F GetRotation()", asMETHOD(Sprite, GetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetRotation(Vec3F)", asMETHOD(Sprite, SetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "Vec3F GetSkew()", asMETHOD(Sprite, GetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetSkew(Vec3F)", asMETHOD(Sprite, SetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "Vec3F GetScale()", asMETHOD(Sprite, GetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetScale(Vec3F)", asMETHOD(Sprite, SetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "Vec3F GetPosition()", asMETHOD(Sprite, GetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetPosition(Vec3F)", asMETHOD(Sprite, SetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "bool GetVisible()", asMETHOD(Sprite, GetVisible), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetVisible(bool)", asMETHOD(Sprite, SetVisible), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "string GetTexturePath()", asMETHOD(Sprite, GetTexturePath), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetTexturePath(string)", asMETHOD(Sprite, SetTexturePath), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "RectF GetUV()", asMETHOD(Sprite, GetUV), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetUV(RectF)", asMETHOD(Sprite, SetUV), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "SizeF GetSize()", asMETHOD(Sprite, GetSize), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetSize(SizeF)", asMETHOD(Sprite, SetSize), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "Color4F GetTint()", asMETHOD(Sprite, GetTint), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetTint(Color4F)", asMETHOD(Sprite, SetTint), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "BlendOperation GetBlend()", asMETHOD(Sprite, GetBlend), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetBlend(BlendOperation)", asMETHOD(Sprite, SetBlend), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "BlendFactor GetBlendFactorSrc()", asMETHOD(Sprite, GetBlendFactorSrc), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetBlendFactorSrc(BlendFactor)", asMETHOD(Sprite, SetBlendFactorSrc), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "BlendFactor GetBlendFactorDst()", asMETHOD(Sprite, GetBlendFactorDst), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetBlendFactorDst(BlendFactor)", asMETHOD(Sprite, SetBlendFactorDst), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "DrawModeOperation GetDrawMode()", asMETHOD(Sprite, GetDrawMode), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetDrawMode(DrawModeOperation)", asMETHOD(Sprite, SetDrawMode), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "CullingOperation GetCulling()", asMETHOD(Sprite, GetCulling), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetCulling(CullingOperation)", asMETHOD(Sprite, SetCulling), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "DepthOperation GetDepth()", asMETHOD(Sprite, GetDepth), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "void SetDepth(DepthOperation)", asMETHOD(Sprite, SetDepth), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "bool HitTest(double, double, OrthoCameraNode@)", asMETHOD(Sprite, HitTest), asCALL_THISCALL); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("TextNode", sizeof(0), asOBJ_REF | asOBJ_NOCOUNT); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "uint GetID()", asMETHOD(Text, GetID), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "Vec3F GetAnchor()", asMETHOD(Text, GetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetAnchor(Vec3F)", asMETHOD(Text, SetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "Vec3F GetRotation()", asMETHOD(Text, GetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetRotation(Vec3F)", asMETHOD(Text, SetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "Vec3F GetSkew()", asMETHOD(Text, GetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetSkew(Vec3F)", asMETHOD(Text, SetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "Vec3F GetScale()", asMETHOD(Text, GetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetScale(Vec3F)", asMETHOD(Text, SetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "Vec3F GetPosition()", asMETHOD(Text, GetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetPosition(Vec3F)", asMETHOD(Text, SetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "bool GetVisible()", asMETHOD(Text, GetVisible), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetVisible(bool)", asMETHOD(Text, SetVisible), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "string GetText()", asMETHOD(Text, GetText), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetText(string)", asMETHOD(Text, SetText), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "string GetFontName()", asMETHOD(Text, GetFontName), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetFontName(string)", asMETHOD(Text, SetFontName), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "uint GetFontStyle()", asMETHOD(Text, GetFontStyle), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetFontStyle(uint)", asMETHOD(Text, SetFontStyle), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "uint GetFontSize()", asMETHOD(Text, GetFontSize), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetFontSize(uint)", asMETHOD(Text, SetFontSize), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "SizeF GetTextSize()", asMETHOD(Text, GetTextSize), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "Color4F GetTint()", asMETHOD(Text, GetTint), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetTint(Color4F)", asMETHOD(Text, SetTint), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "BlendOperation GetBlend()", asMETHOD(Text, GetBlend), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetBlend(BlendOperation)", asMETHOD(Text, SetBlend), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "BlendFactor GetBlendFactorSrc()", asMETHOD(Text, GetBlendFactorSrc), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetBlendFactorSrc(BlendFactor)", asMETHOD(Text, SetBlendFactorSrc), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "BlendFactor GetBlendFactorDst()", asMETHOD(Text, GetBlendFactorDst), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetBlendFactorDst(BlendFactor)", asMETHOD(Text, SetBlendFactorDst), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "DrawModeOperation GetDrawMode()", asMETHOD(Text, GetDrawMode), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetDrawMode(DrawModeOperation)", asMETHOD(Text, SetDrawMode), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "CullingOperation GetCulling()", asMETHOD(Text, GetCulling), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetCulling(CullingOperation)", asMETHOD(Text, SetCulling), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "DepthOperation GetDepth()", asMETHOD(Text, GetDepth), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "void SetDepth(DepthOperation)", asMETHOD(Text, SetDepth), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "bool HitTest(double, double, OrthoCameraNode@)", asMETHOD(Text, HitTest), asCALL_THISCALL); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("FogNode", sizeof(0), asOBJ_REF | asOBJ_NOCOUNT); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "uint GetID()", asMETHOD(Fog, GetID), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "Vec3F GetAnchor()", asMETHOD(Fog, GetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetAnchor(Vec3F)", asMETHOD(Fog, SetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "Vec3F GetRotation()", asMETHOD(Fog, GetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetRotation(Vec3F)", asMETHOD(Fog, SetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "Vec3F GetSkew()", asMETHOD(Fog, GetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetSkew(Vec3F)", asMETHOD(Fog, SetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "Vec3F GetScale()", asMETHOD(Fog, GetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetScale(Vec3F)", asMETHOD(Fog, SetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "Vec3F GetPosition()", asMETHOD(Fog, GetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetPosition(Vec3F)", asMETHOD(Fog, SetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "FogOperation GetFog()", asMETHOD(Fog, GetFog), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetFog(FogOperation)", asMETHOD(Fog, SetFog), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "Color3F GetFogColor()", asMETHOD(Fog, GetFogColor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetFogColor(Color3F)", asMETHOD(Fog, SetFogColor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "float GetFogStart()", asMETHOD(Fog, GetFogStart), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetFogStart(float)", asMETHOD(Fog, SetFogStart), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "float GetFogEnd()", asMETHOD(Fog, GetFogEnd), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetFogEnd(float)", asMETHOD(Fog, SetFogEnd), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "float GetFogDensity()", asMETHOD(Fog, GetFogDensity), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "void SetFogDensity(float)", asMETHOD(Fog, SetFogDensity), asCALL_THISCALL); if (result < 0) { return false; }

	result = m_engine->RegisterObjectType("LightingNode", sizeof(0), asOBJ_REF | asOBJ_NOCOUNT); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "uint GetID()", asMETHOD(Lighting, GetID), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetAnchor()", asMETHOD(Lighting, GetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetAnchor(Vec3F)", asMETHOD(Lighting, SetAnchor), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetRotation()", asMETHOD(Lighting, GetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetRotation(Vec3F)", asMETHOD(Lighting, SetRotation), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetSkew()", asMETHOD(Lighting, GetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetSkew(Vec3F)", asMETHOD(Lighting, SetSkew), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetScale()", asMETHOD(Lighting, GetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetScale(Vec3F)", asMETHOD(Lighting, SetScale), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetPosition()", asMETHOD(Lighting, GetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetPosition(Vec3F)", asMETHOD(Lighting, SetPosition), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "LightsOperation GetLights()", asMETHOD(Lighting, GetLights), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLights(LightsOperation)", asMETHOD(Lighting, SetLights), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Color3F GetAmbientLight()", asMETHOD(Lighting, GetAmbientLight), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetAmbientLight(Color3F)", asMETHOD(Lighting, SetAmbientLight), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "LightOperation GetLight1()", asMETHOD(Lighting, GetLight1), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight1(LightOperation)", asMETHOD(Lighting, SetLight1), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetLight1Position()", asMETHOD(Lighting, GetLight1Position), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight1Position(Vec3F)", asMETHOD(Lighting, SetLight1Position), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "float GetLight1Distance()", asMETHOD(Lighting, GetLight1Distance), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight1Distance(float)", asMETHOD(Lighting, SetLight1Distance), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Color4F GetLight1Diffuse()", asMETHOD(Lighting, GetLight1Diffuse), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight1Diffuse(Color4F)", asMETHOD(Lighting, SetLight1Diffuse), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "LightOperation GetLight2()", asMETHOD(Lighting, GetLight2), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight2(LightOperation)", asMETHOD(Lighting, SetLight2), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetLight2Position()", asMETHOD(Lighting, GetLight2Position), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight2Position(Vec3F)", asMETHOD(Lighting, SetLight2Position), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "float GetLight2Distance()", asMETHOD(Lighting, GetLight2Distance), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight2Distance(float)", asMETHOD(Lighting, SetLight2Distance), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Color4F GetLight2Diffuse()", asMETHOD(Lighting, GetLight2Diffuse), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight2Diffuse(Color4F)", asMETHOD(Lighting, SetLight2Diffuse), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "LightOperation GetLight3()", asMETHOD(Lighting, GetLight3), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight3(LightOperation)", asMETHOD(Lighting, SetLight3), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetLight3Position()", asMETHOD(Lighting, GetLight3Position), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight3Position(Vec3F)", asMETHOD(Lighting, SetLight3Position), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "float GetLight3Distance()", asMETHOD(Lighting, GetLight3Distance), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight3Distance(float)", asMETHOD(Lighting, SetLight3Distance), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Color4F GetLight3Diffuse()", asMETHOD(Lighting, GetLight3Diffuse), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight3Diffuse(Color4F)", asMETHOD(Lighting, SetLight3Diffuse), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "LightOperation GetLight4()", asMETHOD(Lighting, GetLight4), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight4(LightOperation)", asMETHOD(Lighting, SetLight4), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Vec3F GetLight4Position()", asMETHOD(Lighting, GetLight4Position), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight4Position(Vec3F)", asMETHOD(Lighting, SetLight4Position), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "float GetLight4Distance()", asMETHOD(Lighting, GetLight4Distance), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight4Distance(float)", asMETHOD(Lighting, SetLight4Distance), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Color4F GetLight4Diffuse()", asMETHOD(Lighting, GetLight4Diffuse), asCALL_THISCALL); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "void SetLight4Diffuse(Color4F)", asMETHOD(Lighting, SetLight4Diffuse), asCALL_THISCALL); if (result < 0) { return false; }

	result = m_engine->SetDefaultNamespace("SceneManager"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("uint SceneManager::CreateScene(bool)", asFUNCTION(SceneManager::CreateScene), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void SceneManager::SetCurrentScene(uint)", asFUNCTION(SceneManager::SetCurrentScene), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("bool SceneManager::AssignNode(Node@, uint)", asFUNCTION(SceneManager::AssignNode), asCALL_CDECL); if (result < 0) { return false; }

	result = m_engine->SetDefaultNamespace("FontManager"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("bool FontManager::LoadFont(string)", asFUNCTION(FontManager::LoadFont), asCALL_CDECL); if (result < 0) { return false; }

	result = m_engine->SetDefaultNamespace("NodeManager"); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("OrthoCameraNode& NodeManager::CreateOrthoCamera()", asFUNCTION(NodeManager::CreateOrthoCamera), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("PerspectiveCameraNode& NodeManager::CreatePerspectiveCamera()", asFUNCTION(NodeManager::CreatePerspectiveCamera), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("FogNode@ NodeManager::CreateFog()", asFUNCTION(NodeManager::CreateFog), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("LightingNode@ NodeManager::CreateLighting()", asFUNCTION(NodeManager::CreateLighting), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("SpriteNode@ NodeManager::CreateSprite()", asFUNCTION(NodeManager::CreateSprite), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("TextNode@ NodeManager::CreateText()", asFUNCTION(NodeManager::CreateText), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("bool NodeManager::AssignNode(Node@, uint)", asFUNCTION(NodeManager::AssignNode), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("bool NodeManager::AssignNodeAt(Node@, uint, uint)", asFUNCTION(NodeManager::AssignNodeAt), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("void NodeManager::DeleteNode(uint)", asFUNCTION(NodeManager::DeleteNode), asCALL_CDECL); if (result < 0) { return false; }
	result = m_engine->RegisterGlobalFunction("Node@ NodeManager::GetNode(uint)", asFUNCTION(NodeManager::GetNode), asCALL_CDECL); if (result < 0) { return false; }

	result = m_engine->RegisterObjectMethod("Node", "OrthoCameraNode@ opCast()", asFUNCTION((refCast<Node ,OrthoCamera>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Node", "PerspectiveCameraNode@ opCast()", asFUNCTION((refCast<Node ,PerspectiveCamera>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Node", "FogNode@ opCast()", asFUNCTION((refCast<Node, Fog>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Node", "LightingNode@ opCast()", asFUNCTION((refCast<Node, Lighting>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Node", "SpriteNode@ opCast()", asFUNCTION((refCast<Node, Sprite>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("Node", "TextNode@ opCast()", asFUNCTION((refCast<Node, Text>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }

	result = m_engine->RegisterObjectMethod("Node", "OrthoCameraNode@ opImplCast()", asFUNCTION((refCast<Node, OrthoCamera>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("OrthoCameraNode", "Node@ opImplCast()", asFUNCTION((refCast<OrthoCamera, Node>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }

	result = m_engine->RegisterObjectMethod("Node", "PerspectiveCameraNode@ opImplCast()", asFUNCTION((refCast<Node, PerspectiveCamera>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("PerspectiveCameraNode", "Node@ opImplCast()", asFUNCTION((refCast<PerspectiveCamera, Node>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }

	result = m_engine->RegisterObjectMethod("Node", "FogNode@ opImplCast()", asFUNCTION((refCast<Node, Fog>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("FogNode", "Node@ opImplCast()", asFUNCTION((refCast<Fog, Node>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }

	result = m_engine->RegisterObjectMethod("Node", "LightingNode@ opImplCast()", asFUNCTION((refCast<Node, Lighting>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("LightingNode", "Node@ opImplCast()", asFUNCTION((refCast<Lighting, Node>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }

	result = m_engine->RegisterObjectMethod("Node", "SpriteNode@ opImplCast()", asFUNCTION((refCast<Node, Sprite>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("SpriteNode", "Node@ opImplCast()", asFUNCTION((refCast<Sprite, Node>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	
	result = m_engine->RegisterObjectMethod("Node", "TextNode@ opImplCast()", asFUNCTION((refCast<Node, Text>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }
	result = m_engine->RegisterObjectMethod("TextNode", "Node@ opImplCast()", asFUNCTION((refCast<Text, Node>)), asCALL_CDECL_OBJLAST); if (result < 0) { return false; }

	contextMgr = new CContextMgr();
	contextMgr->RegisterCoRoutineSupport(m_engine);
	result = m_engine->SetContextCallbacks(AngelScriptDebugger::RequestContextCallback, AngelScriptDebugger::ReturnContextCallback, 0); if (result < 0) { return false; }


#if defined NEXGEN_WIN	
	result = docGen.Generate(); if (result < 0) { return false; }
#endif

	if (CompileScript(m_engine) == false)
	{
		m_engine->Release();
		return false;
	}

	return true;
}

void AngelScriptRunner::Close()
{
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
	if (m_builder != NULL) 
	{
		delete m_builder;
	}
	if (m_engine != NULL) 
	{
		m_engine->ShutDownAndRelease();
	}
	if (contextMgr != NULL)
	{
		delete contextMgr;
	}

	AngelScriptDebugger::Close();
}

bool AngelScriptRunner::ExecuteInit(void)
{
	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptFunction *initFunction = module->GetFunctionByDecl("void Init()");
	if (initFunction == NULL)
	{
		return false;
	}

	if (ConfigLoader::Debug() == true)
	{
		AngelScriptDebugger::Init(m_engine);
	}

	if (module->ResetGlobalVars(0) < 0)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, initFunction, true);
	if (context == NULL) 
	{
		return false;
	}

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteRender(double dt)
{
	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptFunction *renderFunction = module->GetFunctionByDecl("void Render(double)");
	if (renderFunction == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, renderFunction, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDouble(0, dt);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowIconifyCallback(uint32_t iconified)
{
	if (m_windowIconifyCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowIconifyCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDWord(0, iconified);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowMaximizeCallback(uint32_t maximized)
{
	if (m_windowMaximizeCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowMaximizeCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDWord(0, maximized);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowSizeCallback(uint32_t width, uint32_t height)
{
	if (m_windowSizeCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowSizeCallback, true);
	if (context == NULL) 
	{
		return false;
	}
	
	context->SetArgDWord(0, width);
	context->SetArgDWord(1, height);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowFocusCallback(uint32_t focused)
{
	if (m_windowFocusCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowFocusCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDWord(0, focused);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowKeyboardKeyCallback(uint32_t key, uint32_t scancode, uint32_t action, uint32_t modifier)
{
	if (m_windowKeyboardKeyCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowKeyboardKeyCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDWord(0, key);
	context->SetArgDWord(1, scancode);
	context->SetArgDWord(2, action);
	context->SetArgDWord(3, modifier);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowKeyboardCharacterCallback(uint32_t codepoint)
{
	if (m_windowKeyboardCharacterCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowKeyboardCharacterCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDWord(0, codepoint);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowMouseCursorPositionCallback(double xPos, double yPos)
{
	if (m_windowMouseCursorPositionCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowMouseCursorPositionCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDouble(0, xPos);
	context->SetArgDouble(1, yPos);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowMouseCursorEnterCallback(uint32_t entered)
{
	if (m_windowMouseCursorEnterCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowMouseCursorEnterCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDWord(0, entered);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowMouseButtonCallback(uint32_t button, uint32_t action, uint32_t modifier)
{
	if (m_windowMouseButtonCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowMouseButtonCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDWord(0, button);
	context->SetArgDWord(1, action);
	context->SetArgDWord(2, modifier);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowMouseScrollCallback(double xOffset, double yOffset)
{
	if (m_windowMouseScrollCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowMouseScrollCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDouble(0, xOffset);
	context->SetArgDouble(1, yOffset);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

bool AngelScriptRunner::ExecuteWindowDropCallback(std::vector<std::string> paths)
{
	if (m_windowDropCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_windowDropCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	asITypeInfo *arrayType = m_engine->GetTypeInfoByDecl("array<string>");
	CScriptArray *array = CScriptArray::Create(arrayType, (uint32_t)paths.size());
	for (uint32_t i = 0; i < paths.size(); i++)
	{
		((std::string*)(array->At(i)))->assign(paths.at(i));
	}

	context->SetArgObject(1, array);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	array->Release();
	return success;
}

bool AngelScriptRunner::ExecuteJoystickConnectCallback(uint32_t joystickID, uint32_t connected)
{
	if (m_joystickConnectCallback == NULL)
	{
		return false;
	}

	asIScriptModule *module = m_engine->GetModule("main");
	if (module == NULL)
	{
		return false;
	}

	asIScriptContext *context = contextMgr->AddContext(m_engine, m_joystickConnectCallback, true);
	if (context == NULL) 
	{
		return false;
	}

	context->SetArgDWord(0, joystickID);
	context->SetArgDWord(1, connected);

	bool success = Execute(context);
	contextMgr->DoneWithContext(context);
	return success;
}

// Privates

bool AngelScriptRunner::Execute(asIScriptContext *context)
{
	while(contextMgr->ExecuteScripts());

	uint32_t result = context->GetState();
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
