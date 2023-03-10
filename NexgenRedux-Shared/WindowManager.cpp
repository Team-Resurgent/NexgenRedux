#include "WindowManager.h"
#include "AngelScriptRunner.h"
#include "XboxOGWindowHelper.h"
#include "OpenGLWindowHelper.h"

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
	OpenGLWindowHelper::Close();
#elif defined NEXGEN_OG 
	XboxOGWindowHelper::Close();
#elif defined NEXGEN_360
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

bool WindowManager::RenderLoop(void)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLWindowHelper::RenderLoop();
#elif defined NEXGEN_OG
	return XboxOGWindowHelper::RenderLoop();
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
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
