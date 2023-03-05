#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#include "OpenGLDeviceHelper.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "Icon.h"

#include <Gensys/DebugUtility.h>
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>

#include <cstring>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
	bool m_initialized = false;
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

bool OpenGLDeviceHelper::SetCursorMode(uint32_t windowHandle, uint32_t mode)
{
    if (Init() == false)
    {
        return false;
    }

    WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
	GLFWwindow* window = (GLFWwindow*)windowContainer->window;

    if (mode == 0)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        return true;
    }
    if (mode == 1)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        return true;
    }
    else if (mode == 2)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        return true;
    }
    else if (mode == 2)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_CAPTURED);
        return true;
    }

    return false;
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

bool OpenGLDeviceHelper::GetClipboardString(std::string& value)
{
    if (Init() == false)
    {
        return false;
    }

    const char* clipboard = glfwGetClipboardString(NULL);
	value = clipboard == NULL ? "" : std::string(clipboard);
	return true;
}

bool OpenGLDeviceHelper::SetClipboardString(std::string value)
{
    if (Init() == false)
    {
        return false;
    }

    glfwSetClipboardString(NULL, value.c_str());
	return true;
}

bool OpenGLDeviceHelper::JoystickIsPresent(uint32_t joystickID, uint32_t& state)
{
    if (Init() == false)
    {
        return false;
    }

    state = glfwJoystickPresent(joystickID) == true ? 1 : 0;
    return true;
}

bool OpenGLDeviceHelper::JoystickIsGamepad(uint32_t joystickID, uint32_t& state)
{
    if (Init() == false)
    {
        return false;
    }

	state = glfwJoystickIsGamepad(joystickID) == true ? 1 : 0;
    return true;
}

bool OpenGLDeviceHelper::GetJoystickButtonStates(uint32_t joystickID, JoystickManager::JoystickButtonStates& joystickButtonStates)
{
    if (Init() == false)
    {
        return false;
    }

    memset(&joystickButtonStates, 0, sizeof(joystickButtonStates));

    GLFWgamepadstate gamepadState;
    if (glfwGetGamepadState(joystickID, &gamepadState))
    {
        joystickButtonStates.buttonA = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A];
        joystickButtonStates.buttonB = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_B];
        joystickButtonStates.buttonX = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X];
        joystickButtonStates.buttonY = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_Y];
        joystickButtonStates.buttonLeftBumper = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER];
        joystickButtonStates.buttonRightBumper = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER];
        joystickButtonStates.buttonBack = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_BACK];
        joystickButtonStates.buttonStart = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_START];
        joystickButtonStates.buttonGuide = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_GUIDE];
        joystickButtonStates.buttonLeftThumb = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB];
        joystickButtonStates.buttonRightThumb = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB];
        joystickButtonStates.buttonDpadUp = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP];
        joystickButtonStates.buttonDpadRight = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT];
        joystickButtonStates.buttonDpadDown = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN];
        joystickButtonStates.buttonDpadLeft = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT];
    }
	return true;
}

bool OpenGLDeviceHelper::GetJoystickAxisStates(uint32_t joystickID, JoystickManager::JoystickAxisStates& joystickAxisStates)
{
    if (Init() == false)
    {
        return false;
    }

    memset(&joystickAxisStates, 0, sizeof(joystickAxisStates));

    GLFWgamepadstate gamepadState;
    if (glfwGetGamepadState(joystickID, &gamepadState))
    {
        joystickAxisStates.axisLeftX = gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
        joystickAxisStates.axisLeftY = gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        joystickAxisStates.axisRightX = gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
        joystickAxisStates.axisRightY = gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
        joystickAxisStates.axisLeftTrigger = gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER];
        joystickAxisStates.axisRightTrigger = gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER];
    }
	return true;
}

// Privates

bool OpenGLDeviceHelper::Init(void) 
{
    if (m_initialized == false)
    {
        m_initialized = true;
        glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_VULKAN);
        if (glfwInit() == false)
        {
            return false;
        }
        glfwSetJoystickCallback(OpenGLDeviceHelper::JoystickConnect);
    }
    return true;
}

void OpenGLDeviceHelper::SetCallbacks(GLFWwindow* window)
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
}

void OpenGLDeviceHelper::WindowIconify(GLFWwindow* window, int iconified)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowIconifyCallback(windowHandle, (uint32_t)iconified) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowIconifyCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowMaximize(GLFWwindow* window, int maximized)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMaximizeCallback(windowHandle, (uint32_t)maximized) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMaximizeCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowSize(GLFWwindow* window, int width, int height)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowSizeCallback(windowHandle, (uint32_t)width, (uint32_t)height) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowSizeCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowFocus(GLFWwindow* window, int focused)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowFocusCallback(windowHandle, (uint32_t)focused) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowFocusCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowKeyboardKey(GLFWwindow* window, int key, int scancode, int action, int modifier)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowKeyboardKeyCallback(windowHandle, (uint32_t)key, (uint32_t)scancode, (uint32_t)action, (uint32_t)modifier) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowKeyboardKeyCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowKeyboardCharacter(GLFWwindow* window, unsigned int codepoint)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowKeyboardCharacterCallback(windowHandle, (uint32_t)codepoint) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowKeyboardCharacterCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowMouseCursorPosition(GLFWwindow* window, double xPos, double yPos)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMouseCursorPositionCallback(windowHandle, xPos, yPos) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseCursorPositionCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowMouseCursorEnter(GLFWwindow* window, int entered)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMouseCursorEnterCallback(windowHandle, (uint32_t)entered) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseCursorEnterCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowMouseButton(GLFWwindow* window, int button, int action, int modifier)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMouseButtonCallback(windowHandle, (uint32_t)button, (uint32_t)action, (uint32_t)modifier) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseButtonCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowMouseScroll(GLFWwindow* window, double xOffset, double yOffset)
{
    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMouseScrollCallback(windowHandle, xOffset, yOffset) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMousScrollCallback failed.");
    }
}

void OpenGLDeviceHelper::WindowDrop(GLFWwindow* window, int count, const char** paths)
{
    std::vector<std::string> pathArray;
    for (int i = 0; i < count; i++) 
    {
        std::string path = std::string(paths[i]);
        pathArray.push_back(path);
    }

    WindowManager::WindowContainer windowContainer;
    windowContainer.window = window;

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(windowContainer, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowDropCallback(windowHandle, pathArray) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowDropCallback failed.");
    }
}

void OpenGLDeviceHelper::JoystickConnect(int joystickID, int event)
{
    if (event == GLFW_DISCONNECTED)
    {
        if (AngelScriptRunner::ExecuteJoystickConnectCallback((uint32_t)joystickID, 0) == false)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteJoystickConnectCallback failed.");
        }
    }
    else if (event == GLFW_CONNECTED)
    {
        if (AngelScriptRunner::ExecuteJoystickConnectCallback((uint32_t)joystickID, 1) == false)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteJoystickConnectCallback failed.");
        }
    }
}

#endif