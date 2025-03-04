#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#pragma once

#include "WindowManager.h"
#include "IWindowHelper.h"

#include <Gensys/Int.h>
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#include <vector>

namespace NexgenRedux
{
	class OpenGLWindowHelper : public IWindowHelper
	{		
	private:

		bool m_initialized;

		GLFWwindow* m_window;

	public:

		OpenGLWindowHelper();
		~OpenGLWindowHelper();

		bool GetAvailableMonitorCount(uint32_t& monitorCount) override;
		bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode) override;
		bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes) override;
		bool WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode) override;
		bool WindowCreateWithSize(uint32_t width, uint32_t height) override;
		bool WindowSetTitle(std::string title) override;
		void* GetWindowPtr() override;
		bool GetWindowSize(uint32_t& width, uint32_t& height) override;
		bool WindowPreRender(bool& exitRequested) override;
		bool WindowPostRender() override;
		void PollEvents(void) override;
		bool WindowClose() override;
		bool GetKeyPressed(uint32_t key, KeyButtonState& state) override;
		bool GetMouseButtonPressed(uint32_t button, MouseButtonState& state) override;
		bool GetMouseCursorPosition(double& xPos, double& yPos) override;
		bool SetMouseCursorPosition(double xPos, double yPos) override;
		bool GetClipboardString(std::string& value) override;
		bool SetClipboardString(std::string value) override;

		bool SetCursorMode(CursorMode mode) override;
		bool JoystickIsPresent(uint32_t joystickID, uint32_t& present) override;
		bool JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad) override;
		bool GetJoystickButtonStates(uint32_t joystickID, JoystickButtonStates& joystickButtonStates) override;
		bool GetJoystickAxisStates(uint32_t joystickID, JoystickAxisStates& joystickAxisStates) override;
		bool GetJoystickHatCount(uint32_t joystickID, uint32_t& count) override;
		bool GetJoystickHatDirection(uint32_t joystickID, uint32_t hatIndex, uint32_t& direction) override;

	private:

		bool Init(void);
		void SetDarkTitleBar(GLFWwindow* window);
		void SetCallbacks(GLFWwindow* window);
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
