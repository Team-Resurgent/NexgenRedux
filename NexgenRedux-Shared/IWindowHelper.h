#pragma once

#include "MeshUtility.h"
#include "WindowManager.h"
#include "RenderStateManager.h"

#include <Gensys/Int.h>

#include <map>
#include <string>

namespace NexgenRedux
{
	class IWindowHelper
	{		
	public:

		virtual ~IWindowHelper() {};

		virtual std::vector<uint32_t> GetWindowHandles(void) = 0;
		virtual bool GetAvailableMonitorCount(uint32_t& monitorCount) = 0;
		virtual bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode) = 0;
		virtual bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes) = 0;
		virtual bool WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle) = 0;
		virtual bool WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle) = 0;
		virtual bool GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height) = 0;
		virtual bool WindowPreRender(uint32_t& windowHandle, bool& exitRequested) = 0;
		virtual bool WindowPostRender(uint32_t& windowHandle) = 0;
		virtual void PollEvents(void) = 0;
		virtual bool WindowClose(uint32_t windowHandle) = 0;
		virtual bool GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed) = 0;
		virtual bool GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed) = 0;
		virtual bool GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos) = 0;
		virtual bool SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos) = 0;
		virtual bool GetClipboardString(std::string& value) = 0;
		virtual bool SetClipboardString(std::string value) = 0;

		virtual bool SetCursorMode(uint32_t windowHandle, uint32_t mode) = 0;
		virtual bool JoystickIsPresent(uint32_t joystickID, uint32_t& present) = 0;
		virtual bool JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad) = 0;
		virtual bool GetJoystickButtonStates(uint32_t joystickID, JoystickButtonStates& joystickButtonStates) = 0;
		virtual bool GetJoystickAxisStates(uint32_t joystickID, JoystickAxisStates& joystickAxisStates) = 0;
		virtual bool GetJoystickHatCount(uint32_t joystickID, uint32_t& count) = 0;
		virtual bool GetJoystickHatDirection(uint32_t joystickID, uint32_t hatIndex, uint32_t& direction) = 0;
	};
}
