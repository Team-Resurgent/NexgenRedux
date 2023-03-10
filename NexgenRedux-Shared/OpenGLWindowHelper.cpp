#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#include "OpenGLWindowHelper.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "Icon.h"
#include "RenderingManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/TimeUtility.h>
#include <Gensys/Int.h>
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>

#include <cstring>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
	bool m_initialized = false;
}

void OpenGLWindowHelper::Close(void) 
{
	std::vector<uint32_t> windowHandles = WindowManager::GetWindowHandles();
	for (uint32_t i = 0; i < windowHandles.size(); i++) 
	{
		WindowManager::WindowClose(windowHandles.at(i));
	}
}

bool OpenGLWindowHelper::GetAvailableMonitorCount(uint32_t& monitorCount)
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

bool OpenGLWindowHelper::GetMonitorVideoMode(uint32_t monitorIndex, WindowManager::MonitorVideoMode& monitorVideoMode)
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

bool OpenGLWindowHelper::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<WindowManager::MonitorVideoMode>& monitorVideoModes)
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

bool OpenGLWindowHelper::WindowCreateWithVideoMode(WindowManager::MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle)
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
    windowContainer.width = monitorVideoMode.width;
    windowContainer.height = monitorVideoMode.height;
    windowContainer.window = window;
    windowHandle = WindowManager::AddWindowContainer(windowContainer);

	return RenderingManager::Init(windowHandle);
}

bool OpenGLWindowHelper::WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle)
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
    windowContainer.width = width;
    windowContainer.height = height;
    windowContainer.window = window;
    windowHandle = WindowManager::AddWindowContainer(windowContainer);

	return RenderingManager::Init(windowHandle);
}

bool OpenGLWindowHelper::GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height)
{
	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
	if (windowContainer == NULL)
	{
		return false;
	}

	width = windowContainer->width;
	height = windowContainer->height;
	return true;
}

bool OpenGLWindowHelper::SetCursorMode(uint32_t windowHandle, uint32_t mode)
{
    if (Init() == false)
    {
        return false;
    }

    WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
    if (windowContainer == NULL)
	{
		return false;
	}

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

bool OpenGLWindowHelper::WindowRender(uint32_t& windowHandle, bool& exitRequested)
{
 	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
    if (windowContainer == NULL)
	{
		return false;
	}

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

bool OpenGLWindowHelper::WindowClose(uint32_t windowHandle)
{
	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
	if (windowContainer == NULL) 
    {
		return false;
	}

	glfwDestroyWindow((GLFWwindow*)windowContainer->window);
	WindowManager::DeleteWindowContainer(windowHandle);
	return true;
}

bool OpenGLWindowHelper::RenderLoop(void)
{
	if (WindowManager::GetWindowCount() > 0) {
		bool exitRequested = false;
		while (exitRequested == false)
		{
			uint64_t now = TimeUtility::GetMillisecondsNow();
			uint64_t previousNow = now;

			std::vector<uint32_t> windowHandles = WindowManager::GetWindowHandles();

			for (uint32_t i = 0; i < windowHandles.size(); i++)
			{
				uint32_t windowHandle = windowHandles.at(i);
				double dt = TimeUtility::GetDurationSeconds(previousNow, now);
				if (AngelScriptRunner::ExecuteRender(windowHandle, dt) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteRender failed.");
					return false;
				}
				if (OpenGLWindowHelper::WindowRender(windowHandle, exitRequested) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowRender failed.");
					return false;
				}
			}
			glfwPollEvents();
			previousNow = now;
		}
	}

    glfwTerminate();
    return true;
}

bool OpenGLWindowHelper::GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed)
{
    if (Init() == false)
    {
        return false;
    }

    WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
    if (windowContainer == NULL)
	{
		return false;
	}

	GLFWwindow* window = (GLFWwindow*)windowContainer->window;

    pressed = (uint32_t)glfwGetKey(window, key);

    return true;
}

bool OpenGLWindowHelper::GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed)
{
    if (Init() == false)
    {
        return false;
    }

    WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
    if (windowContainer == NULL)
	{
		return false;
	}

	GLFWwindow* window = (GLFWwindow*)windowContainer->window;

    pressed = (uint32_t)glfwGetMouseButton(window, button);

    return true;

}

bool OpenGLWindowHelper::GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos)
{
    if (Init() == false)
    {
        return false;
    }

    WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
    if (windowContainer == NULL)
	{
		return false;
	}

	GLFWwindow* window = (GLFWwindow*)windowContainer->window;

    glfwGetCursorPos(window, &xPos, &yPos);
    return true;
}

bool OpenGLWindowHelper::SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos)
{
    if (Init() == false)
    {
        return false;
    }

    WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
    if (windowContainer == NULL)
	{
		return false;
	}

	GLFWwindow* window = (GLFWwindow*)windowContainer->window;

    glfwSetCursorPos(window, xPos, yPos);
    return true;
}

bool OpenGLWindowHelper::GetClipboardString(std::string& value)
{
    if (Init() == false)
    {
        return false;
    }

    const char* clipboard = glfwGetClipboardString(NULL);
	value = clipboard == NULL ? "" : std::string(clipboard);
	return true;
}

bool OpenGLWindowHelper::SetClipboardString(std::string value)
{
    if (Init() == false)
    {
        return false;
    }

    glfwSetClipboardString(NULL, value.c_str());
	return true;
}

bool OpenGLWindowHelper::JoystickIsPresent(uint32_t joystickID, uint32_t& present)
{
    if (Init() == false)
    {
        return false;
    }

    present = glfwJoystickPresent(joystickID) == true ? 1 : 0;
    return true;
}

bool OpenGLWindowHelper::JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad)
{
    if (Init() == false)
    {
        return false;
    }

	gamepad = glfwJoystickIsGamepad(joystickID) == true ? 1 : 0;
    return true;
}

bool OpenGLWindowHelper::GetJoystickButtonStates(uint32_t joystickID, JoystickManager::JoystickButtonStates& joystickButtonStates)
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
        joystickButtonStates.buttonLeftBumperOrWhite = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER];
        joystickButtonStates.buttonRightBumperOrBlack = gamepadState.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER];
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

bool OpenGLWindowHelper::GetJoystickAxisStates(uint32_t joystickID, JoystickManager::JoystickAxisStates& joystickAxisStates)
{
    if (Init() == false)
    {
        return false;
    }

	joystickAxisStates.axisLeftX = 0;
	joystickAxisStates.axisLeftY = 0;
	joystickAxisStates.axisRightX = 0;
	joystickAxisStates.axisRightY = 0;
	joystickAxisStates.axisLeftTrigger = -1;
	joystickAxisStates.axisRightTrigger = -1;

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

bool OpenGLWindowHelper::GetJoystickHatCount(uint32_t joystickID, uint32_t& count)
{
    if (Init() == false)
    {
        return false;
    }

    int hatCount;
    glfwGetJoystickHats(joystickID, &hatCount);
    count = hatCount;
    return true;
}

bool OpenGLWindowHelper::GetJoystickHatDirection(uint32_t joystickID, uint32_t hatIndex, uint32_t& direction)
{
    if (Init() == false)
    {
        return false;
    }

    int hatCount;
    const unsigned char *hatStates = glfwGetJoystickHats(joystickID, &hatCount);

    if (hatStates == NULL || hatIndex >= hatCount)
	{
		direction = 0;
		return true;
	}

    direction = (uint32_t)hatStates[hatIndex];
    return true;
}

// Privates

bool OpenGLWindowHelper::Init(void) 
{
    if (m_initialized == false)
    {
        m_initialized = true;

        // glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_OPENGLES);
        // const char* description;
        // int code = glfwGetError(&description);

        glfwInitHint(GLFW_JOYSTICK_HAT_BUTTONS, GLFW_TRUE);

        if (glfwInit() == false)
        {
            return false;
        }
        glfwSetJoystickCallback(OpenGLWindowHelper::JoystickConnect);
    }
    return true;
}

void OpenGLWindowHelper::SetCallbacks(GLFWwindow* window)
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

void OpenGLWindowHelper::WindowIconify(GLFWwindow* window, int iconified)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowIconifyCallback(windowHandle, (uint32_t)iconified) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowIconifyCallback failed.");
    }
}

void OpenGLWindowHelper::WindowMaximize(GLFWwindow* window, int maximized)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMaximizeCallback(windowHandle, (uint32_t)maximized) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMaximizeCallback failed.");
    }
}

void OpenGLWindowHelper::WindowSize(GLFWwindow* window, int width, int height)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowSizeCallback(windowHandle, (uint32_t)width, (uint32_t)height) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowSizeCallback failed.");
    }
}

void OpenGLWindowHelper::WindowFocus(GLFWwindow* window, int focused)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowFocusCallback(windowHandle, (uint32_t)focused) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowFocusCallback failed.");
    }
}

void OpenGLWindowHelper::WindowKeyboardKey(GLFWwindow* window, int key, int scancode, int action, int modifier)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowKeyboardKeyCallback(windowHandle, (uint32_t)key, (uint32_t)scancode, (uint32_t)action, (uint32_t)modifier) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowKeyboardKeyCallback failed.");
    }
}

void OpenGLWindowHelper::WindowKeyboardCharacter(GLFWwindow* window, unsigned int codepoint)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowKeyboardCharacterCallback(windowHandle, (uint32_t)codepoint) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowKeyboardCharacterCallback failed.");
    }
}

void OpenGLWindowHelper::WindowMouseCursorPosition(GLFWwindow* window, double xPos, double yPos)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMouseCursorPositionCallback(windowHandle, xPos, yPos) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseCursorPositionCallback failed.");
    }
}

void OpenGLWindowHelper::WindowMouseCursorEnter(GLFWwindow* window, int entered)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMouseCursorEnterCallback(windowHandle, (uint32_t)entered) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseCursorEnterCallback failed.");
    }
}

void OpenGLWindowHelper::WindowMouseButton(GLFWwindow* window, int button, int action, int modifier)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMouseButtonCallback(windowHandle, (uint32_t)button, (uint32_t)action, (uint32_t)modifier) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseButtonCallback failed.");
    }
}

void OpenGLWindowHelper::WindowMouseScroll(GLFWwindow* window, double xOffset, double yOffset)
{
    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowMouseScrollCallback(windowHandle, xOffset, yOffset) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMousScrollCallback failed.");
    }
}

void OpenGLWindowHelper::WindowDrop(GLFWwindow* window, int count, const char** paths)
{
    std::vector<std::string> pathArray;
    for (int i = 0; i < count; i++) 
    {
        std::string path = std::string(paths[i]);
        pathArray.push_back(path);
    }

    uint32_t windowHandle;
    if (WindowManager::GetWindowHandle(window, windowHandle) == false) 
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "GetWindowHandle failed.");
    }
    else if (AngelScriptRunner::ExecuteWindowDropCallback(windowHandle, pathArray) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowDropCallback failed.");
    }
}

void OpenGLWindowHelper::JoystickConnect(int joystickID, int event)
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