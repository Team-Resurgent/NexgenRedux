#if defined NEXGEN_OG

#pragma once

#include "WindowManager.h"
#include "IRenderingHelper.h"
#include "IWindowHelper.h"
#include "GlobalTypes.h"

#include <Gensys/Int.h>

#include <vector>
#include <xtl.h>

namespace NexgenRedux
{
	class XboxOGWindowHelper : public IWindowHelper
	{		
	public:

		typedef struct WindowContainer
		{
			uint32_t width;
			uint32_t height;
		} WindowContainer;

		XboxOGWindowHelper();
		~XboxOGWindowHelper();

		void Close(void);

		std::vector<uint32_t> GetWindowHandles(void);
		bool GetAvailableMonitorCount(uint32_t& monitorCount);
		bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode);
		bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes);
		bool WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle);
		bool WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle);
		bool GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height);
		bool WindowPreRender(uint32_t& windowHandle, bool& exitRequested);
		bool WindowPostRender(uint32_t& windowHandle);
		void PollEvents(void);
		bool WindowClose(uint32_t windowHandle);
		bool GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed);
		bool GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed);
		bool GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos);
		bool SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos);
		bool GetClipboardString(std::string& value);
		bool SetClipboardString(std::string value);

		bool SetCursorMode(uint32_t windowHandle, uint32_t mode);
		bool JoystickIsPresent(uint32_t joystickID, uint32_t& present);
		bool JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad);
		bool GetJoystickButtonStates(uint32_t joystickID, JoystickButtonStates& joystickButtonStates);
		bool GetJoystickAxisStates(uint32_t joystickID, JoystickAxisStates& joystickAxisStates);
		bool GetJoystickHatCount(uint32_t joystickID, uint32_t& count);
		bool GetJoystickHatDirection(uint32_t joystickID, uint32_t hatIndex, uint32_t& direction);

		static void* GetD3DDevice();

	private:
		bool Init(void);
		bool InitKeyboard(void);
		bool InitMouse(void);
		void ProcessController();
		void ProcessKeyboard();
		void ProcessMouse();
	};
}

#endif
