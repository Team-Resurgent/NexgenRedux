#if defined NEXGEN_OG

#pragma once

#include "WindowManager.h"
#include "JoystickManager.h"

#include <Gensys/Int.h>

#include <vector>
#include <xtl.h>

namespace NexgenRedux
{
	class XboxOGDeviceHelper
	{		
	public:
		static bool GetAvailableMonitorCount(uint32_t& monitorCount);
		static bool GetMonitorVideoMode(uint32_t monitorIndex, WindowManager::MonitorVideoMode& monitorVideoMode);
		static bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<WindowManager::MonitorVideoMode>& monitorVideoModes);
		static bool WindowCreateWithVideoMode(WindowManager::MonitorVideoMode monitorVideoMode, uint32_t& windowHandle);
		static bool WindowCreateWithSize(uint32_t width, uint32_t height, uint32_t& windowHandle);
		static bool SetCursorMode(uint32_t windowHandle, uint32_t mode);
		static bool WindowRender(uint32_t& windowHandle, bool& exitRequested);
		static bool WindowClose(uint32_t windowHandle);
		static bool GetClipboardString(std::string& value);
		static bool SetClipboardString(std::string value);

		static bool JoystickIsPresent(uint32_t joystickID, uint32_t& state);
		static bool JoystickIsGamepad(uint32_t joystickID, uint32_t& state);
		static bool GetJoystickButtonStates(uint32_t joystickID, JoystickManager::JoystickButtonStates& joystickButtonStates);
		static bool GetJoystickAxisStates(uint32_t joystickID, JoystickManager::JoystickAxisStates& joystickAxisStates);
	};
}

#endif
