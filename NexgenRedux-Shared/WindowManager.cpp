#include "WindowManager.h"
#include "XboxOGDeviceHelper.h"
#include "OpenGLDeviceHelper.h"

#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>

#include <GLAD/glad.h>
#include <GLFW/glfw3.h>

#include <map>
#include <vector>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
    typedef struct WindowContainer
	{
		void* window;
	} WindowContainer;

	uint32_t m_maxWindowContainerID = 0;
	std::map<uint32_t, WindowContainer> m_windowContainerMap;

	uint32_t AddWindowContainer(WindowContainer windowContainer)
	{
		uint32_t result = m_maxWindowContainerID;
		m_windowContainerMap.insert(std::pair<uint32_t, WindowContainer>(result, windowContainer));
		m_maxWindowContainerID++;
		return result;
	}

	WindowContainer* GetWindowContainer(uint32_t windowHandle)
	{
		std::map<uint32_t, WindowContainer>::iterator it = m_windowContainerMap.find(windowHandle);
		if (it == m_windowContainerMap.end()) {
			return NULL;
		}
		return (WindowContainer*)&it->second;
	}
}

void WindowManager::Dispose(void) 
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

bool WindowManager::WindowCreate(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    GLFWwindow* window;
	if (OpenGLDeviceHelper::WindowCreate(monitorVideoMode, title, &window) == false)
	{
		return false;
	}

    WindowContainer windowContainer;
    windowContainer.window = window;
    windowHandle = AddWindowContainer(windowContainer);

    return true;

#elif defined NEXGEN_OG

	if (m_windowContainerMap.size() > 0)
	{
		return false;
	}

	IDirect3DDevice8 *d3dDevice;
	if (XboxOGDeviceHelper::WindowCreate(monitorVideoMode, &d3dDevice) == false)
	{
		return false;
	}

	WindowContainer windowContainer;
    windowContainer.window = d3dDevice;
    windowHandle = AddWindowContainer(windowContainer);

	while (true) {
		D3DCOLOR color = D3DCOLOR_RGBA(255, 0, 0, 255);
		HRESULT hr = d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, color, 0, 0);
		hr = d3dDevice->Present(NULL, NULL, NULL, NULL);
	}

	return true;

#elif defined NEXGEN_360

	return true;

#else

	return false;

#endif
}

bool WindowManager::WindowCreate(int width, int height, std::string title, uint32_t& windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    GLFWwindow* window;
	if (OpenGLDeviceHelper::WindowCreate(width, height, title, &window) == false)
	{
		return false;
	}

    WindowContainer windowContainer;
    windowContainer.window = window;
    windowHandle = AddWindowContainer(windowContainer);

    return true;

#elif defined NEXGEN_OG

	if (m_windowContainerMap.size() > 0)
	{
		return false;
	}

	IDirect3DDevice8 *d3dDevice;
	if (XboxOGDeviceHelper::WindowCreate(width, height, &d3dDevice) == false)
	{
		return false;
	}

	WindowContainer windowContainer;
    windowContainer.window = d3dDevice;
    windowHandle = AddWindowContainer(windowContainer);

	while (true) {
		D3DCOLOR color = D3DCOLOR_RGBA(255, 0, 0, 255);
		HRESULT hr = d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, color, 0, 0);
		hr = d3dDevice->Present(NULL, NULL, NULL, NULL);
	}

	return true;

#elif defined NEXGEN_360

	return true;

#else

	return false;

#endif
}

bool WindowManager::WindowClose(uint32_t windowHandle)
{
	WindowContainer* windowContainer = GetWindowContainer(windowHandle);
	if (windowContainer == NULL) {
		return false;
	}

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

	OpenGLDeviceHelper::WindowDispose(windowContainer->window);
	m_windowContainerMap.erase(windowHandle);
    return true;

#elif defined NEXGEN_OG

	XboxOGDeviceHelper::WindowDispose(windowContainer->window);
	m_windowContainerMap.erase(windowHandle);
	return true;

#elif defined NEXGEN_360

	m_windowContainerMap.erase(windowHandle);
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

bool WindowManager::RenderLoop()
{

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    bool exitRequested = false;
    while (exitRequested == false)
    {
        for (std::map<uint32_t, WindowContainer>::iterator it = m_windowContainerMap.begin(); it != m_windowContainerMap.end(); ++it)
        {
            WindowContainer* windowContainer = (WindowContainer*)&it->second;
            GLFWwindow* window = (GLFWwindow*)windowContainer->window;
			int closeFlag = glfwWindowShouldClose(window);
            if (closeFlag != 0)
            {
                exitRequested = true;
                break;
            }

            glfwMakeContextCurrent(window);

            glClearColor(1.0f, .1f, .1f, .1f);
            glClear(GL_COLOR_BUFFER_BIT);
            glfwSwapBuffers(window);

        }
        glfwPollEvents();
    }

    glfwTerminate();
    return true;

#elif defined NEXGEN_OG

	return true;

#elif defined NEXGEN_360

	return true;

#else

	return false;

#endif
}