#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#include "OpenGLDeviceHelper.h"
#include "WindowManager.h"
#include "Icon.h"

#include <GLAD/glad.h>
#include <GLFW/glfw3.h>

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
}

bool OpenGLDeviceHelper::GetAvailableMonitorCount(uint32_t& monitorCount)
{
	if (Init() == false)
    {
        return false;
    }

    int count;
    glfwGetMonitors(&count);
    monitorCount = count;
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

bool OpenGLDeviceHelper::WindowCreate(WindowManager::MonitorVideoMode monitorVideoMode, std::string title, GLFWwindow** window)
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
    
    GLFWwindow* createdWindow = glfwCreateWindow(monitorVideoMode.width, monitorVideoMode.height, title.c_str(), monitors[monitorVideoMode.monitorIndex], nullptr);
    if (createdWindow == nullptr)
    {
        glfwTerminate();
        return false;
    }

	glfwMakeContextCurrent(createdWindow);
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
    glfwSetWindowIcon(createdWindow, 1, iconImages);
	free(buffer); 

	*window = createdWindow;

	return true;
}

bool OpenGLDeviceHelper::WindowCreate(int width, int height, std::string title, GLFWwindow** window)
{
	if (Init() == false)
    {
        return false;
    }

	glfwWindowHint(GLFW_SAMPLES, 0);
    
    GLFWwindow* createdWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (createdWindow == nullptr)
    {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(createdWindow);
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
    glfwSetWindowIcon(createdWindow, 1, iconImages); 
	free(buffer); 

	*window = createdWindow;

	return true;
}

void OpenGLDeviceHelper::WindowDispose(void* window)
{
	glfwDestroyWindow((GLFWwindow*)window);
}

#endif