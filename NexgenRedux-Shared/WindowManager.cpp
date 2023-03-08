#include "WindowManager.h"
#include "AngelScriptRunner.h"
#include "XboxOGDeviceHelper.h"
#include "OpenGLDeviceHelper.h"

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
	uint32_t m_maxWindowContainerID = 0;
	std::map<uint32_t, WindowManager::WindowContainer> m_windowContainerMap;
}

void WindowManager::Close(void) 
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	OpenGLDeviceHelper::Close();
#elif defined NEXGEN_OG 
	XboxOGDeviceHelper::Close();
#elif defined NEXGEN_360
#endif
}

void WindowManager::PollEvents(void)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	OpenGLDeviceHelper::PollEvents();
#elif defined NEXGEN_OG 
	XboxOGDeviceHelper::PollEvents();
#endif
}

bool WindowManager::GetAvailableMonitorCount(uint32_t& monitorCount)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetAvailableMonitorCount(monitorCount);
#elif defined NEXGEN_OG 
	return XboxOGDeviceHelper::GetAvailableMonitorCount(monitorCount);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
    return OpenGLDeviceHelper::GetMonitorVideoMode(monitorIndex, monitorVideoMode);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetMonitorVideoMode(monitorIndex, monitorVideoMode);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
   return OpenGLDeviceHelper::GetMonitorVideoModes(monitorIndex, monitorVideoModes);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetMonitorVideoModes(monitorIndex, monitorVideoModes);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
    return OpenGLDeviceHelper::WindowCreateWithVideoMode(monitorVideoMode, title, windowHandle);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::WindowCreateWithVideoMode(monitorVideoMode, windowHandle);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::WindowCreateWithSize(width, height, title, windowHandle);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::WindowCreateWithSize(width, height, windowHandle);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetWindowSize(windowHandle, width, height);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetWindowSize(windowHandle, width, height);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::SetCursorMode(uint32_t windowHandle, uint32_t mode)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::SetCursorMode(windowHandle, mode);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::SetCursorMode(windowHandle, mode);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::WindowClose(uint32_t windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::WindowClose(windowHandle);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::WindowClose(windowHandle);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::RenderLoop(void)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

	if (GetWindowCount() > 0) {
		bool exitRequested = false;
		while (exitRequested == false)
		{
			uint64_t now = TimeUtility::GetMillisecondsNow();
			uint64_t previousNow = now;

			std::vector<uint32_t> windowHandles = GetWindowHandles();

			for (uint32_t i = 0; i < windowHandles.size(); i++)
			{
				uint32_t windowHandle = windowHandles.at(i);
				double dt = TimeUtility::GetDurationSeconds(previousNow, now);
				if (AngelScriptRunner::ExecuteRender(windowHandle, dt) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteRender failed.");
				}
				if (OpenGLDeviceHelper::WindowRender(windowHandle, exitRequested) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowRender failed.");
				}
			}
			PollEvents();
			previousNow = now;
		}
	}

    glfwTerminate();
    return true;

#elif defined NEXGEN_OG

	if (GetWindowCount() > 0) 
	{
		bool exitRequested = false;
		while (exitRequested == false)
		{
			uint64_t now = TimeUtility::GetMillisecondsNow();
			uint64_t previousNow = now;

			std::vector<uint32_t> windowHandles = GetWindowHandles();

			for (uint32_t i = 0; i < windowHandles.size(); i++)
			{
				uint32_t windowHandle = windowHandles.at(i);
				double dt = TimeUtility::GetDurationSeconds(previousNow, now);
				if (AngelScriptRunner::ExecuteRender(windowHandle, dt) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteRender failed.");
				}
				if (XboxOGDeviceHelper::WindowRender(windowHandles.at(i), exitRequested) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowRender failed.");
				}
			}
			PollEvents();
			previousNow = now;
		}
	}

	return true;
	
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetKeyPressed(windowHandle, key, pressed);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetKeyPressed(windowHandle, key, pressed);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed)
{
    #if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetMouseButtonPressed(windowHandle, button, pressed);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetMouseButtonPressed(windowHandle, button, pressed);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetMouseCursorPosition(windowHandle, xPos, yPos);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetMouseCursorPosition(windowHandle, xPos, yPos);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::SetMouseCursorPosition(windowHandle, xPos, yPos);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::SetMouseCursorPosition(windowHandle, xPos, yPos);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::GetClipboardString(std::string& value)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetClipboardString(value);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetClipboardString(value);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool WindowManager::SetClipboardString(std::string value)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::SetClipboardString(value);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::SetClipboardString(value);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

// Privates

uint32_t WindowManager::GetWindowCount(void)
{
	return (uint32_t)m_windowContainerMap.size();
}

bool WindowManager::GetWindowHandle(void* window, uint32_t& windowHandle)
{
	for (std::map<uint32_t, WindowContainer>::iterator it = m_windowContainerMap.begin(); it != m_windowContainerMap.end(); ++it)
	{
		if (it->second.window == window)
		{
			windowHandle = it->first;
			return true;
		} 
	}
	return false;
}

std::vector<uint32_t> WindowManager::GetWindowHandles(void)
{
	std::vector<uint32_t> windowHandles;
	for (std::map<uint32_t, WindowContainer>::iterator it = m_windowContainerMap.begin(); it != m_windowContainerMap.end(); ++it)
	{
		 windowHandles.push_back(it->first);
	}
	return windowHandles;
}

uint32_t WindowManager::AddWindowContainer(WindowManager::WindowContainer windowContainer)
{
	uint32_t result = m_maxWindowContainerID;
	m_windowContainerMap.insert(std::pair<uint32_t, WindowContainer>(result, windowContainer));
	m_maxWindowContainerID++;
	return result;
}

WindowManager::WindowContainer* WindowManager::GetWindowContainer(uint32_t windowHandle)
{
	std::map<uint32_t, WindowContainer>::iterator it = m_windowContainerMap.find(windowHandle);
	if (it == m_windowContainerMap.end()) 
	{
		return NULL;
	}
	return (WindowContainer*)&it->second;
}

void WindowManager::DeleteWindowContainer(uint32_t windowHandle)
{
	m_windowContainerMap.erase(windowHandle);
}
