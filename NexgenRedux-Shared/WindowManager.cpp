#include "WindowManager.h"
#include "AngelScriptRunner.h"
#include "XboxOGWindowHelper.h"
#include "OpenGLWindowHelper.h"
#include "RenderStateManager.h"
#include "MeshUtility.h"
#include "IWindowHelper.h"

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

namespace 
{
	WindowManager* m_instance = NULL;
}

WindowManager::WindowManager()
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	m_windowHelper = new OpenGLWindowHelper();
#else
	m_windowHelper = new XboxOGWindowHelper();
#endif
}

WindowManager::~WindowManager()
{
	delete m_windowHelper;
}

WindowManager* WindowManager::GetInstance()
{
	if (m_instance == NULL) 
	{
    	m_instance = new WindowManager();
    }
    return m_instance;
}

void WindowManager::Close()
{
	delete m_instance;
	m_instance = NULL;
}

IWindowHelper* WindowManager::GetWindowHelper(void)
{
	return m_windowHelper;
}

std::vector<uint32_t> WindowManager::GetWindowHandles(void)
{
	return m_windowHelper->GetWindowHandles();
}

bool WindowManager::GetAvailableMonitorCount(uint32_t& monitorCount)
{
	return m_windowHelper->GetAvailableMonitorCount(monitorCount);
}

bool WindowManager::GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode)
{
	return m_windowHelper->GetMonitorVideoMode(monitorIndex, monitorVideoMode);
}

bool WindowManager::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes)
{
	return m_windowHelper->GetMonitorVideoModes(monitorIndex, monitorVideoModes);
}

bool WindowManager::WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle)
{
	return m_windowHelper->WindowCreateWithVideoMode(monitorVideoMode, title, windowHandle);
}

bool WindowManager::WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle)
{
	return m_windowHelper->WindowCreateWithSize(width, height, title, windowHandle);
}

bool WindowManager::GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height)
{
	return m_windowHelper->GetWindowSize(windowHandle, width, height);
}

bool WindowManager::SetCursorMode(uint32_t windowHandle, uint32_t mode)
{
	return m_windowHelper->SetCursorMode(windowHandle, mode);
}

bool WindowManager::WindowClose(uint32_t windowHandle)
{
	return m_windowHelper->WindowClose(windowHandle);
}

bool WindowManager::WindowPreRender(uint32_t& windowHandle, bool& exitRequested)
{
	return m_windowHelper->WindowPreRender(windowHandle, exitRequested);
}

bool WindowManager::WindowPostRender(uint32_t& windowHandle)
{ 
	return m_windowHelper->WindowPostRender(windowHandle);
}

void WindowManager::PollEvents(void)
{
	m_windowHelper->PollEvents();
}

bool WindowManager::RenderLoop()
{
	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

	renderStateManager->Init();

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

			for (uint32_t i = 0; i < windowHandles.size(); i++)
			{
				uint32_t windowHandle = windowHandles.at(i);
				double dt = TimeUtility::GetDurationSeconds(previousNow, now);
                if (WindowPreRender(windowHandle, exitRequested) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowPreRender failed.");
					return false;
				}

				SceneManager::Update((float)dt);

				if (AngelScriptRunner::ExecuteRender(windowHandle, dt) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteRender failed.");
					return false;
				}

				SceneManager::Render();

				if (WindowPostRender(windowHandle) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowPostRender failed.");
					return false;
				}
			}
			PollEvents();
			previousNow = now;
		}
	}
    return true;
}

bool WindowManager::GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed)
{
	return m_windowHelper->GetKeyPressed(windowHandle, key, pressed);
}

bool WindowManager::GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed)
{
	return m_windowHelper->GetMouseButtonPressed(windowHandle, button, pressed);
}

bool WindowManager::GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos)
{
	return m_windowHelper->GetMouseCursorPosition(windowHandle, xPos, yPos);
}

bool WindowManager::SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos)
{
	return m_windowHelper->SetMouseCursorPosition(windowHandle, xPos, yPos);
}

bool WindowManager::GetClipboardString(std::string& value)
{
	return m_windowHelper->GetClipboardString(value);
}

bool WindowManager::SetClipboardString(std::string value)
{
	return m_windowHelper->SetClipboardString(value);
}