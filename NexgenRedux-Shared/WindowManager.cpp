#include "WindowManager.h"
#include "Icon.h"

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
#include "GLAD/glad.h"
#include "GLFW/glfw3.h"
#endif

#include <Gensys/Int.h>

#include <map>
#include <vector>

using namespace NexgenRedux;

namespace 
{
    typedef struct WindowContainer
	{
		void* window;
	} WindowContainer;

    bool m_initialized = false;
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

	void DeleteWindowContainer(uint32_t windowHandle)
	{
		m_windowContainerMap.erase(windowHandle);
	}
}

bool Init(void) 
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    if (m_initialized == false)
    {
        m_initialized = true;
        glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
        if (glfwInit() == false)
        {
            return false;
        }
    }

#elif defined NEXGEN_OG || defined NEXGEN_360

	return true;

#else

	return false;

#endif
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
    if (Init() == false)
    {
        return false;
    }

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    int count;
    glfwGetMonitors(&count);
    monitorCount = count;
	return true;

#elif defined NEXGEN_OG || defined NEXGEN_360

	monitorCount = 1;
	return true;

#else

	return false;

#endif
}

bool WindowManager::GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode)
{
    if (Init() == false)
    {
        return false;
    }

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorIndex >= monitorCount)
    {
        return false;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitors[monitorIndex]);
    monitorVideoMode.monitorIndex = monitorIndex;
    monitorVideoMode.width = mode->width;
    monitorVideoMode.height = mode->height;
    monitorVideoMode.redBits = mode->redBits;
    monitorVideoMode.greenBits = mode->greenBits;
    monitorVideoMode.blueBits = mode->blueBits;
    monitorVideoMode.refreshRate = mode->refreshRate;
    return true;

#elif defined NEXGEN_OG || defined NEXGEN_360

	return true;

#else

	return false;

#endif
}

bool WindowManager::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes)
{
    if (Init() == false)
    {
        return false;
    }

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorIndex >= monitorCount)
    {
        return false;
    }

    monitorVideoModes.clear();

    int videoModesCount;
    const GLFWvidmode* modes = glfwGetVideoModes(monitors[monitorIndex], &videoModesCount);
    for (uint32_t i = 0; i < videoModesCount; i++)
    {
        MonitorVideoMode monitorVideoMode;
        monitorVideoMode.monitorIndex = monitorIndex;
        monitorVideoMode.width = modes[i].width;
        monitorVideoMode.height = modes[i].height;
        monitorVideoMode.redBits = modes[i].redBits;
        monitorVideoMode.greenBits = modes[i].greenBits;
        monitorVideoMode.blueBits = modes[i].blueBits;
        monitorVideoMode.refreshRate = modes[i].refreshRate;
        monitorVideoModes.push_back(monitorVideoMode);
    }

    return true;

#elif defined NEXGEN_OG || defined NEXGEN_360

	return true;

#else

	return false;

#endif
}

bool WindowManager::WindowCreate(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle)
{
    if (Init() == false)
    {
        return false;
    }
    
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    if (monitorVideoMode.monitorIndex >= monitorCount)
    {
        return false;
    }

    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RED_BITS, monitorVideoMode.redBits);
    glfwWindowHint(GLFW_GREEN_BITS, monitorVideoMode.greenBits);
    glfwWindowHint(GLFW_BLUE_BITS, monitorVideoMode.blueBits);
    glfwWindowHint(GLFW_REFRESH_RATE, monitorVideoMode.refreshRate);
    
    GLFWwindow* window = glfwCreateWindow(monitorVideoMode.width, monitorVideoMode.height, title.c_str(), monitors[monitorVideoMode.monitorIndex], nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return false;
    }

    GLFWimage iconImages[1]; 
    iconImages[0].pixels = iconData;
    iconImages[0].width = iconWidth;
    iconImages[0].height = iconHeight;
    glfwSetWindowIcon(window, 1, iconImages); 

    WindowContainer windowContainer;
    windowContainer.window = window;
    windowHandle = AddWindowContainer(windowContainer);

    return true;

#elif defined NEXGEN_OG || defined NEXGEN_360

	return true;

#else

	return false;

#endif
}

bool WindowManager::WindowCreate(int width, int height, std::string title, uint32_t& windowHandle)
{
    if (Init() == false)
    {
        return false;
    }

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

    glfwWindowHint(GLFW_SAMPLES, 0);
    
    GLFWwindow* window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    if (gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress) == false)
    {
        return false;
    }

    GLFWimage iconImages[1]; 
    iconImages[0].pixels = iconData;
    iconImages[0].width = iconWidth;
    iconImages[0].height = iconHeight;
    glfwSetWindowIcon(window, 1, iconImages); 

    WindowContainer windowContainer;
    windowContainer.window = window;
    windowHandle = AddWindowContainer(windowContainer);

    return true;

#elif defined NEXGEN_OG || defined NEXGEN_360

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

    glfwDestroyWindow((GLFWwindow*)windowContainer->window);
    DeleteWindowContainer(windowHandle);
    return true;


#elif defined NEXGEN_OG || defined NEXGEN_360

	DeleteWindowContainer(windowHandle);
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

#elif defined NEXGEN_OG || defined NEXGEN_360

	return true;

#else

	return false;

#endif
}