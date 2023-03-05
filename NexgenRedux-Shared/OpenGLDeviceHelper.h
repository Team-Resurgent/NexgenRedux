#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#pragma once

#include "WindowManager.h"
#include "JoystickManager.h"

#include <Gensys/Int.h>
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#include <vector>

namespace NexgenRedux
{
	class OpenGLDeviceHelper
	{		
	public:
		static bool GetAvailableMonitorCount(uint32_t& monitorCount);
		static bool GetMonitorVideoMode(uint32_t monitorIndex, WindowManager::MonitorVideoMode& monitorVideoMode);
		static bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<WindowManager::MonitorVideoMode>& monitorVideoModes);
		static bool WindowCreateWithVideoMode(WindowManager::MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle);
		static bool WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle);
		static bool WindowRender(uint32_t& windowHandle, bool& exitRequested);
		static bool WindowClose(uint32_t windowHandle);
		static bool GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed);
		static bool GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed);
		static bool GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos);
		static bool SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos);
		static bool GetClipboardString(std::string& value);
		static bool SetClipboardString(std::string value);

		static bool SetCursorMode(uint32_t windowHandle, uint32_t mode);
		static bool JoystickIsPresent(uint32_t joystickID, uint32_t& present);
		static bool JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad);
		static bool GetJoystickButtonStates(uint32_t joystickID, JoystickManager::JoystickButtonStates& joystickButtonStates);
		static bool GetJoystickAxisStates(uint32_t joystickID, JoystickManager::JoystickAxisStates& joystickAxisStates);

	private:
		static bool Init(void);
		static void SetCallbacks(GLFWwindow* window);
		static void WindowIconify(GLFWwindow* window, int iconified);
		static void WindowMaximize(GLFWwindow* window, int maximized);
		static void WindowSize(GLFWwindow* window, int width, int height);
		static void WindowFocus(GLFWwindow* window, int focused);
		static void WindowKeyboardKey(GLFWwindow* window, int key, int scancode, int action, int modifier);
		static void WindowKeyboardCharacter(GLFWwindow* window, unsigned int codepoint);
		static void WindowMouseCursorPosition(GLFWwindow* window, double xPos, double yPos);
		static void WindowMouseCursorEnter(GLFWwindow* window, int entered);
		static void WindowMouseButton(GLFWwindow* window, int button, int action, int modifier);
		static void WindowMouseScroll(GLFWwindow* window, double xOffset, double yOffset);
		static void WindowDrop(GLFWwindow* window, int count, const char** paths);
		static void JoystickConnect(int joystickID, int event);
	};
}

#endif
