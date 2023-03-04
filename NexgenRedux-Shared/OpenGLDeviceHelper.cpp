#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#include "OpenGLDeviceHelper.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "Icon.h"

#include <Gensys/DebugUtility.h>
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
	bool m_initialized = false;

	bool Init(void) 
	{
		if (m_initialized == false)
		{
			m_initialized = true;
			glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
			if (glfwInit() == false)
			{
				return false;
			}
		}
		return true;
	}

    void WindowIconify(GLFWwindow* window, int iconified)
    {
        //printf("window iconify: %i\n", iconified);
    }

    void WindowMaximize(GLFWwindow* window, int maximized)
    {
        //printf("window maximize: %i\n", maximized);
    }

    void WindowSize(GLFWwindow* window, int width, int height)
    {
        //printf("window size: %i,%i\n", width, height);
    }

    void WindowFocus(GLFWwindow* window, int focused)
    {
        //printf("window focus: %i\n", focused);
    }

    void WindowKeyboardKey(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        //printf("keyboard state: %i,%i,%i,%i\n", key, scancode, action, mods);
    }

    void WindowKeyboardCharacter(GLFWwindow* window, unsigned int codepoint)
    {
        //printf("keyboard key: %i\n", codepoint);
    }

    void WindowMouseCursorPosition(GLFWwindow* window, double xpos, double ypos)
    {
        //printf("mouse moved: %f,%f\n", xpos, ypos);
    }

    void WindowMouseCursorEnter(GLFWwindow* window, int entered)
    {
    //     if (entered)
    //     {
    //         printf("mouse entered\n");
    //     }
    //     else
    //     {
    //         printf("mouse left\n");
    //     }
    }

    void WindowMouseButton(GLFWwindow* window, int button, int action, int modifier)
    {
        WindowManager::WindowContainer windowContainer;
        windowContainer.window = window;

        uint32_t windowHandle;
        if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
        }
        else if (AngelScriptRunner::ExecuteWindowMouseButtonCallback(0, (uint32_t)button, (uint32_t)action, (uint32_t)modifier) == false)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseButtonCallback failed.");
        }
    }

    void WindowMouseScroll(GLFWwindow* window, double xoffset, double yoffset)
    {
        //printf("mouse scroll %f,%f\n", xoffset, yoffset);
    }

    void WindowDrop(GLFWwindow* window, int count, const char** paths)
    {
        // for (int i = 0; i < count; i++) {
        //     printf("drop %i,%s\n", count, paths[i]);
        // }
    }

    void WindowJoystick(int jid, int event)
    {
        // if (event == GLFW_CONNECTED)
        // {
        //     printf("joystick connected %i,%i\n", jid, event);
        // }
        // else if (event == GLFW_DISCONNECTED)
        // {
        //     printf("joystick disconnected %i,%i\n", jid, event);
        // }
    }

    void SetCallbacks(GLFWwindow* window)
    {
        glfwSetWindowIconifyCallback(window, WindowIconify);
        glfwSetWindowMaximizeCallback(window, WindowMaximize);
        glfwSetWindowSizeCallback(window, WindowSize);
        glfwSetWindowFocusCallback(window, WindowFocus);
        glfwSetKeyCallback(window, WindowKeyboardKey);
        glfwSetCharCallback(window, WindowKeyboardCharacter);
        glfwSetCursorPosCallback(window, WindowMouseCursorPosition);
        glfwSetCursorEnterCallback(window, WindowMouseCursorEnter);
        glfwSetMouseButtonCallback(window, WindowMouseButton);
        glfwSetScrollCallback(window, WindowMouseScroll);
        glfwSetDropCallback(window, WindowDrop);
        glfwSetJoystickCallback(WindowJoystick);
    }
}

bool OpenGLDeviceHelper::GetAvailableMonitorCount(uint32_t& monitorCount)
{
	if (Init() == false)
    {
        return false;
    }

    int count;
    glfwGetMonitors(&count);
    monitorCount = (uint32_t)count;
	return true;
}

bool OpenGLDeviceHelper::GetMonitorVideoMode(uint32_t monitorIndex, WindowManager::MonitorVideoMode& monitorVideoMode)
{
	if (Init() == false)
    {
        return false;
    }

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
}

bool OpenGLDeviceHelper::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<WindowManager::MonitorVideoMode>& monitorVideoModes)
{
	if (Init() == false)
    {
        return false;
    }

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
        WindowManager::MonitorVideoMode monitorVideoMode;
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
}

bool OpenGLDeviceHelper::WindowCreateWithVideoMode(WindowManager::MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle)
{
	if (Init() == false)
    {
        return false;
    }

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

    SetCallbacks(window);

    //glfwSetJoystickCallback(joystick_callback);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	glfwMakeContextCurrent(window);
    if (gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress) == false)
    {
        return false;
    }

    GLFWimage iconImages[1]; 

	unsigned char* buffer = (unsigned char*)malloc(Icon::DataSize());
	Icon::Populate(buffer);
    iconImages[0].pixels = buffer;
    iconImages[0].width = Icon::Width();
    iconImages[0].height = Icon::Height();
    glfwSetWindowIcon(window, 1, iconImages);
	free(buffer); 

    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;
    windowHandle = WindowManager::AddWindowContainer(windowContainer);
	return true;
}

bool OpenGLDeviceHelper::WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle)
{
	if (Init() == false)
    {
        return false;
    }

	glfwWindowHint(GLFW_SAMPLES, 0);
    
    GLFWwindow* window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return false;
    }

    SetCallbacks(window);

    //glfwSetJoystickCallback(joystick_callback);
    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    glfwMakeContextCurrent(window);
    if (gladLoadGLES2Loader((GLADloadproc)glfwGetProcAddress) == false)
    {
        return false;
    }

    GLFWimage iconImages[1]; 
	unsigned char* buffer = (unsigned char*)malloc(Icon::DataSize());
	Icon::Populate(buffer);
    iconImages[0].pixels = buffer;
    iconImages[0].width = Icon::Width();
    iconImages[0].height = Icon::Height();
    glfwSetWindowIcon(window, 1, iconImages); 
	free(buffer); 

    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;
    windowHandle = WindowManager::AddWindowContainer(windowContainer);
	return true;
}

bool OpenGLDeviceHelper::WindowRender(uint32_t& windowHandle, bool& exitRequested)
{
 	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
	GLFWwindow* window = (GLFWwindow*)windowContainer->window;

	int closeFlag = glfwWindowShouldClose(window);
	if (closeFlag != 0)
	{
		exitRequested = true;
		return true;
	}

	glfwMakeContextCurrent(window);

	glClearColor(1.0f, .1f, .1f, .1f);
	glClear(GL_COLOR_BUFFER_BIT);

	glfwSwapBuffers(window);
	return true;
}

bool OpenGLDeviceHelper::WindowClose(uint32_t windowHandle)
{
	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
	if (windowContainer == NULL) {
		return false;
	}

	glfwDestroyWindow((GLFWwindow*)windowContainer->window);
	WindowManager::DeleteWindowContainer(windowHandle);
	return true;
}

#endif