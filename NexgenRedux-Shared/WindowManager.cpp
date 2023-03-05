#include "WindowManager.h"
#include "AngelScriptRunner.h"
#include "XboxOGDeviceHelper.h"
#include "OpenGLDeviceHelper.h"

#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>

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
	while (m_windowContainerMap.size() > 0)
	{
		std::map<uint32_t, WindowContainer>::iterator it = m_windowContainerMap.begin();
		WindowManager::WindowClose(it->first);
	}
}

bool WindowManager::GetAvailableMonitorCount(uint32_t& monitorCount)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetAvailableMonitorCount(monitorCount);
#elif defined NEXGEN_OG || defined NEXGEN_360
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

bool WindowManager::SetCursorMode(uint32_t windowHandle, uint32_t mode)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::SetCursorMode(windowHandle, mode);
#elif defined NEXGEN_OG
	return true;
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

// void Render()
// {

//     glfwSetWindowIconifyCallback(window, window_iconify);
//     glfwSetWindowMaximizeCallback(window, window_maximize);
//     glfwSetWindowSizeCallback(window, window_size);
//     glfwSetWindowFocusCallback(window, window_focus);
//     glfwSetKeyCallback(window, key_callback);
//     glfwSetCharCallback(window, character_callback);
//     glfwSetCursorPosCallback(window, cursor_position_callback);
//     glfwSetCursorEnterCallback(window, cursor_enter_callback);
//     glfwSetMouseButtonCallback(window, mouse_button_callback);
//     glfwSetScrollCallback(window, scroll_callback);
//     glfwSetDropCallback(window, drop_callback);
//     glfwSetJoystickCallback(joystick_callback);
//     glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

bool WindowManager::RenderLoop(void)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

	if (GetWindowCount() > 0) {
		bool exitRequested = false;
		while (exitRequested == false)
		{
			std::vector<uint32_t> windowHandles = GetWindowHandles();

			for (uint32_t i = 0; i < windowHandles.size(); i++)
			{
				uint32_t windowHandle = windowHandles.at(i);
				AngelScriptRunner::ExecuteRender(windowHandle, 0.123f);
				OpenGLDeviceHelper::WindowRender(windowHandle, exitRequested);
			}
			glfwPollEvents();
		}
	}

    glfwTerminate();
    return true;

#elif defined NEXGEN_OG

	if (GetWindowCount() > 0) {
		bool exitRequested = false;
		while (exitRequested == false)
		{
			std::vector<uint32_t> windowHandles = GetWindowHandles();

			for (uint32_t i = 0; i < windowHandles.size(); i++)
			{
				uint32_t windowHandle = windowHandles.at(i);
				AngelScriptRunner::ExecuteRender(windowHandle, 0.123f);
				XboxOGDeviceHelper::WindowRender(windowHandles.at(i), exitRequested);
			}
		}
	}

	return true;
	
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

// Privates

uint32_t WindowManager::GetWindowCount(void)
{
	return m_windowContainerMap.size();
}

bool WindowManager::GetWindowHandle(WindowContainer windowContainer, uint32_t& windowHandle)
{
	for (std::map<uint32_t, WindowContainer>::iterator it = m_windowContainerMap.begin(); it != m_windowContainerMap.end(); ++it)
	{
		if (it->second.window == windowContainer.window)
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
	if (it == m_windowContainerMap.end()) {
		return NULL;
	}
	return (WindowContainer*)&it->second;
}

void WindowManager::DeleteWindowContainer(uint32_t windowHandle)
{
	m_windowContainerMap.erase(windowHandle);
}
