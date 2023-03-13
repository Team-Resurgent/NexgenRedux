#include "WindowManager.h"
#include "AngelScriptRunner.h"
#include "XboxOGWindowHelper.h"
#include "OpenGLWindowHelper.h"
#include "RenderStateManager.h"
#include "MeshUtility.h"

#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>
#include <Gensys/TimeUtility.h>

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#endif

#include <map>
#include <vector>

using namespace Gensys;
using namespace NexgenRedux;

// namespace 
// {
// 	uint32_t m_maxWindowContainerID = 0;
// 	std::map<uint32_t, WindowManager::WindowContainer> m_windowContainerMap;
// }

void WindowManager::Close(void) 
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	OpenGLWindowHelper::Close();
#elif defined NEXGEN_OG 
	XboxOGWindowHelper::Close();
#elif defined NEXGEN_360
	return;
#endif
}

std::vector<uint32_t> WindowManager::GetWindowHandles(void)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::GetWindowHandles();
#elif defined NEXGEN_OG 
	return XboxOGWindowHelper::GetWindowHandles();
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetAvailableMonitorCount(uint32_t& monitorCount)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::GetAvailableMonitorCount(monitorCount);
#elif defined NEXGEN_OG 
	return XboxOGWindowHelper::GetAvailableMonitorCount(monitorCount);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
    return OpenGLWindowHelper::GetMonitorVideoMode(monitorIndex, monitorVideoMode);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::GetMonitorVideoMode(monitorIndex, monitorVideoMode);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
   return OpenGLWindowHelper::GetMonitorVideoModes(monitorIndex, monitorVideoModes);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::GetMonitorVideoModes(monitorIndex, monitorVideoModes);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
    return OpenGLWindowHelper::WindowCreateWithVideoMode(monitorVideoMode, title, windowHandle);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::WindowCreateWithVideoMode(monitorVideoMode, windowHandle);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::WindowCreateWithSize(width, height, title, windowHandle);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::WindowCreateWithSize(width, height, windowHandle);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::GetWindowSize(windowHandle, width, height);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::GetWindowSize(windowHandle, width, height);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::SetCursorMode(uint32_t windowHandle, uint32_t mode)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::SetCursorMode(windowHandle, mode);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::SetCursorMode(windowHandle, mode);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::WindowClose(uint32_t windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::WindowClose(windowHandle);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::WindowClose(windowHandle);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::WindowPreRender(uint32_t& windowHandle, bool& exitRequested)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::WindowPreRender(windowHandle, exitRequested);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::WindowPreRender(windowHandle, exitRequested);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::WindowPostRender(uint32_t& windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::WindowPostRender(windowHandle);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::WindowPostRender(windowHandle);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

void WindowManager::PollEvents(void)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	OpenGLWindowHelper::PollEvents();
#elif defined NEXGEN_OG
	XboxOGWindowHelper::PollEvents();
#elif defined NEXGEN_360
#endif
}

bool WindowManager::RenderLoop(void)
{
    uint32_t textureID;
    if (RenderStateManager::LoadTexture(L"skin:background.png", textureID) == false)
    {
        return false;
    }

    uint32_t meshID = MeshUtility::CreateQuadXY(MathUtility::Vec3F(0, 0, 0), MathUtility::SizeF(640, 480), MathUtility::RectF(0, 0, 1, 1), textureID);

	std::vector<uint32_t> windowHandles = GetWindowHandles();

	float f = 0;

	if (windowHandles.size() > 0) 
	{
		bool exitRequested = false;
		while (exitRequested == false)
		{
			uint64_t now = TimeUtility::GetMillisecondsNow();
			uint64_t previousNow = now;

			windowHandles = GetWindowHandles();

            MathUtility::Vec3F eye = MathUtility::Vec3F(0, 0, 2);
			MathUtility::Vec3F target = MathUtility::Vec3F(0, 0, 0);
			MathUtility::Vec3F up = MathUtility::Vec3F(0, 1, 0);
            MathUtility::Matrix4x4 modelMatrix = MathUtility::Matrix4x4();
			MathUtility::Matrix4x4 viewMatrix = MathUtility::Matrix4x4::LookAt(eye, target, up);
			MathUtility::Matrix4x4 projectionMatrix = MathUtility::Matrix4x4::OrthoOffCenter(0, 640, 0, 480, 1, 100);

            RenderStateManager::Init();
			RenderStateManager::SetShader("Default");
            RenderStateManager::SetModelMatrix(modelMatrix);
            RenderStateManager::SetViewMatrix(viewMatrix);
            RenderStateManager::SetProjectionMatrix(projectionMatrix);
            RenderStateManager::SetAmbientLight(MathUtility::Color3I(25, 25, 25));
            RenderStateManager::SetTint(MathUtility::Color4I(255, 255, 255, 255));
            RenderStateManager::SetBlend(RenderStateManager::BlendOperationDisabled);
            RenderStateManager::SetBlendFactors(RenderStateManager::BlendFactorOne, RenderStateManager::BlendFactorOne);
            RenderStateManager::SetCulling(RenderStateManager::CullingOperationDisabled);
            RenderStateManager::SetDepth(RenderStateManager::DepthOperationLess);
            RenderStateManager::SetLights(RenderStateManager::LightsOperationDisabled);
            RenderStateManager::SetLight1(RenderStateManager::LightOperationDisabled);
            RenderStateManager::SetLightInstance1(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4I(0, 0, 0, 0));
            RenderStateManager::SetLight2(RenderStateManager::LightOperationDisabled);
            RenderStateManager::SetLightInstance2(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4I(0, 0, 0, 0));
            RenderStateManager::SetLight3(RenderStateManager::LightOperationDisabled);
            RenderStateManager::SetLightInstance3(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4I(0, 0, 0, 0));
            RenderStateManager::SetLight4(RenderStateManager::LightOperationDisabled);
            RenderStateManager::SetLightInstance4(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4I(0, 0, 0, 0));
            RenderStateManager::SetFog(RenderStateManager::FogOperationDisabled);
            RenderStateManager::SetFogInstance(MathUtility::Color3I(0, 0, 0), 0, 0, 0);
            RenderStateManager::SetViewport(MathUtility::RectI(0, 0, 640, 480));
            RenderStateManager::SetScissor(RenderStateManager::ScissorOperationDisabled, MathUtility::RectI());
            RenderStateManager::SetDrawMode(RenderStateManager::DrawModeTriangles);

			for (uint32_t i = 0; i < windowHandles.size(); i++)
			{
				uint32_t windowHandle = windowHandles.at(i);
				double dt = TimeUtility::GetDurationSeconds(previousNow, now);
                if (WindowManager::WindowPreRender(windowHandle, exitRequested) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowPreRender failed.");
					return false;
				}

				modelMatrix = MathUtility::Matrix4x4::Translate(MathUtility::Vec3F(-320, -240, 0));
				modelMatrix *= MathUtility::Matrix4x4::RotateZ(f);
				modelMatrix *= MathUtility::Matrix4x4::Translate(MathUtility::Vec3F(320, 240, 0));
				RenderStateManager::SetModelMatrix(modelMatrix);
				f+=0.5;

                RenderStateManager::RenderMesh(meshID);

				if (AngelScriptRunner::ExecuteRender(windowHandle, dt) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteRender failed.");
					return false;
				}
				if (WindowManager::WindowPostRender(windowHandle) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowPostRender failed.");
					return false;
				}
			}
			WindowManager::PollEvents();
			previousNow = now;
		}
	}
    return true;
}

bool WindowManager::GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::GetKeyPressed(windowHandle, key, pressed);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::GetKeyPressed(windowHandle, key, pressed);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed)
{
    #if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::GetMouseButtonPressed(windowHandle, button, pressed);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::GetMouseButtonPressed(windowHandle, button, pressed);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::GetMouseCursorPosition(windowHandle, xPos, yPos);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::GetMouseCursorPosition(windowHandle, xPos, yPos);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::SetMouseCursorPosition(windowHandle, xPos, yPos);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::SetMouseCursorPosition(windowHandle, xPos, yPos);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetClipboardString(std::string& value)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::GetClipboardString(value);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::GetClipboardString(value);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::SetClipboardString(std::string value)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::SetClipboardString(value);
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::SetClipboardString(value);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}