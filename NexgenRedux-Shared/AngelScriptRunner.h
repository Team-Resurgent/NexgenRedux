#pragma once

#include "WindowManager.h"
#include "EntityEngine/SceneManager.h"
#include "EntityEngine/NodeManager.h"
#include "EntityEngine/Sprite.h"

#include <Gensys/Int.h>

#include <AngelScript/angelscript.h>

#include <string>
#include <vector>

using namespace AngelScript;

namespace NexgenRedux
{
	class WindowManager;
	class AngelScriptRunner
	{
	public:

		typedef struct Vec2
		{
			float x;
			float y;
		} Vec2;

		AngelScriptRunner(WindowManager *windowManager);
		~AngelScriptRunner();

		static bool Init(std::wstring launchFolder);
		static bool ExecuteInit(void);
		static bool ExecuteRender(uint32_t windowHandle, double dt);
		static bool ExecuteWindowIconifyCallback(uint32_t windowHandle, uint32_t iconified);
		static bool ExecuteWindowMaximizeCallback(uint32_t windowHandle, uint32_t maximized);
		static bool ExecuteWindowSizeCallback(uint32_t windowHandle, uint32_t width, uint32_t height);
		static bool ExecuteWindowFocusCallback(uint32_t windowHandle, uint32_t focused);
		static bool ExecuteWindowKeyboardKeyCallback(uint32_t windowHandle, uint32_t key, uint32_t scancode, uint32_t action, uint32_t modifier);
		static bool ExecuteWindowKeyboardCharacterCallback(uint32_t windowHandle, uint32_t codepoint);
		static bool ExecuteWindowMouseCursorPositionCallback(uint32_t windowHandle, double xPos, double yPos);
		static bool ExecuteWindowMouseCursorEnterCallback(uint32_t windowHandle, uint32_t entered);
		static bool ExecuteWindowMouseButtonCallback(uint32_t windowHandle, uint32_t button, uint32_t action, uint32_t modifier);
		static bool ExecuteWindowMouseScrollCallback(uint32_t windowHandle, double xOffset, double yOffset);
		static bool ExecuteWindowDropCallback(uint32_t windowHandle, std::vector<std::string> paths);
		static bool ExecuteJoystickConnectCallback(uint32_t joystickID, uint32_t connected);

	private:
		static bool Execute(asIScriptContext *context);	
		static void SetWindowIconifyCallback(asIScriptGeneric* gen);
		static void SetWindowMaximizeCallback(asIScriptGeneric* gen);
		static void SetWindowSizeCallback(asIScriptGeneric* gen);
		static void SetWindowFocusCallback(asIScriptGeneric* gen);
		static void SetWindowKeyboardKeyCallback(asIScriptGeneric* gen);
		static void SetWindowKeyboardCharacterCallback(asIScriptGeneric* gen);
		static void SetWindowMouseCursorPositionCallback(asIScriptGeneric* gen);
		static void SetWindowMouseCursorEnterCallback(asIScriptGeneric* gen);
		static void SetWindowMouseButtonCallback(asIScriptGeneric* gen);
		static void SetWindowMouseScrollCallback(asIScriptGeneric* gen);
		static void SetWindowDropCallback(asIScriptGeneric* gen);
		static void SetJoystickConnectCallback(asIScriptGeneric* gen);


		static void DebugPrint(asIScriptGeneric* gen);
		static void DebugPrintIf(asIScriptGeneric* gen);

		static void GetAvailableMonitorCount(asIScriptGeneric* gen);
		static void GetMonitorVideoMode(asIScriptGeneric* gen);
		static void GetMonitorVideoModes(asIScriptGeneric* gen);
		static void WindowCreateWithVideoMode(asIScriptGeneric* gen);
		static void WindowCreateWithSize(asIScriptGeneric* gen);
		static void GetWindowSize(asIScriptGeneric* gen);
		static void SetCursorMode(asIScriptGeneric* gen);
		static void WindowClose(asIScriptGeneric* gen);
		static void GetKeyPressed(asIScriptGeneric* gen);
		static void GetMouseButtonPressed(asIScriptGeneric* gen);
		static void GetMouseCursorPosition(asIScriptGeneric* gen);
		static void SetMouseCursorPosition(asIScriptGeneric* gen);
		static void GetClipboardString(asIScriptGeneric* gen);
		static void SetClipboardString(asIScriptGeneric* gen);

		static void GetTimeNow(asIScriptGeneric* gen);
		static void GetMillisecondsNow(asIScriptGeneric* gen);
		static void GetDurationSeconds(asIScriptGeneric* gen);
		static void CalculateFramesPerSecond(asIScriptGeneric* gen);
		static void SleepMilliseconds(asIScriptGeneric* gen);

		static void SeedRandom(asIScriptGeneric* gen);
		static void SeedRandomWithValue(asIScriptGeneric* gen);
		static void GetRandomDouble(asIScriptGeneric* gen);
		static void GetRandomIntInRange(asIScriptGeneric* gen);

		static void JoystickIsPresent(asIScriptGeneric* gen);
		static void JoystickIsGamepad(asIScriptGeneric* gen);
		static void GetJoystickButtonStates(asIScriptGeneric* gen);
		static void GetJoystickAxisStates(asIScriptGeneric* gen);
		static void GetJoystickHatCount(asIScriptGeneric* gen);
		static void GetJoystickHatDirection(asIScriptGeneric* gen);

		static void Vec2DConstructorDefault(MathUtility::Vec2D* Vec2D);
        static void Vec2DConstructorValues(double x, double y, MathUtility::Vec2D* Vec2D);
        static void Vec2DConstructorCopy(const MathUtility::Vec2D& other, MathUtility::Vec2D* Vec2D);

		static void Vec2FConstructorDefault(MathUtility::Vec2F* vec2F);
		static void Vec2FConstructorValues(float x, float y, MathUtility::Vec2F* vec2F);
		static void Vec2FConstructorCopy(const MathUtility::Vec2F& other, MathUtility::Vec2F* vec2F);

		static void Vec3FConstructorDefault(MathUtility::Vec3F* vec3F);
		static void Vec3FConstructorValues(float x, float y, float z, MathUtility::Vec3F* vec3F);
		static void Vec3FConstructorCopy(const MathUtility::Vec3F& other, MathUtility::Vec3F* vec3F);

		static void Vec4FConstructorDefault(MathUtility::Vec4F* vec4F);
		static void Vec4FConstructorValues(float x, float y, float z, float w, MathUtility::Vec4F* vec4F);
		static void Vec4FConstructorCopy(const MathUtility::Vec4F& other, MathUtility::Vec4F* vec4F);

		static void SizeIConstructorDefault(MathUtility::SizeI* sizeI);
        static void SizeIConstructorValues(uint32_t x, uint32_t y, MathUtility::SizeI* sizeI);
        static void SizeIConstructorCopy(const MathUtility::SizeI& other, MathUtility::SizeI* sizeI);

		static void SizeFConstructorDefault(MathUtility::SizeF* sizeF);
        static void SizeFConstructorValues(float x, float y, MathUtility::SizeF* sizeF);
        static void SizeFConstructorCopy(const MathUtility::SizeF& other, MathUtility::SizeF* sizeF);

		static void RectIConstructorDefault(MathUtility::RectI* rectI);
		static void RectIConstructorValues(uint32_t x, uint32_t y, uint32_t width, uint32_t height, MathUtility::RectI* rectI);
		static void RectIConstructorCopy(const MathUtility::RectI& other, MathUtility::RectI* rectI);

		static void RectFConstructorDefault(MathUtility::RectF* rectF);
		static void RectFConstructorValues(float x, float y, float width, float height, MathUtility::RectF* rectF);
		static void RectFConstructorCopy(const MathUtility::RectF& other, MathUtility::RectF* rectF);

		static void Color3FConstructorDefault(MathUtility::Color3F* color3F);
        static void Color3FConstructorValues(float r, float g, float b, MathUtility::Color3F* color3F);
        static void Color3FConstructorCopy(const MathUtility::Color3F& other, MathUtility::Color3F* color3F);

        static void Color4FConstructorDefault(MathUtility::Color4F* color4F);
        static void Color4FConstructorValues(float r, float g, float b, float a, MathUtility::Color4F* color4F);
        static void Color4FConstructorCopy(const MathUtility::Color4F& other, MathUtility::Color4F* color4F);

	private:
		WindowManager *m_windowManager;
	};
}