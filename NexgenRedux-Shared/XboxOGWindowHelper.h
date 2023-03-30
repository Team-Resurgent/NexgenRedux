#if defined NEXGEN_OG

#pragma once

#include "WindowManager.h"
#include "IRenderingHelper.h"
#include "IWindowHelper.h"
#include "GlobalTypes.h"
#include "Constants.h"

#include <Gensys/Int.h>

#include <vector>
#include <xtl.h>

namespace NexgenRedux
{
	class XboxOGWindowHelper : public IWindowHelper
	{		
	public:

		uint32_t m_width;
		uint32_t m_height;
		IDirect3DDevice8* m_d3dDevice;

		bool m_initialized;

		std::string m_clipboardValue;

		typedef struct KeyInfo
		{
			uint32_t scancode;
			uint32_t keyID;
			KeyInfo(uint32_t scancode, uint32_t keyID) : scancode(scancode), keyID(keyID) {}
		} KeyInfo;

		std::map<uint32_t, KeyInfo> m_virtualKeyToScancodeMap;
		uint32_t m_keyboardModifier;
		bool m_keyboardKeys[KEY_LAST];

		int32_t m_mouseX;
		int32_t m_mouseY;
		bool m_mouseButtons[5];
		bool m_previousMouseButtons[5];
		XINPUT_STATE m_mouseStates[XGetPortCount() * 2];
		uint32_t m_lastMousePacket[XGetPortCount() * 2];

		HANDLE m_controllerHandles[XGetPortCount()];
		HANDLE m_keyboardHandles[XGetPortCount() * 2];
		HANDLE m_mouseHandles[XGetPortCount() * 2];

		uint32_t m_keyboardMaskTable[XGetPortCount() * 2];
		uint32_t m_mouseMaskTable[XGetPortCount() * 2];

	public:

		XboxOGWindowHelper();
		~XboxOGWindowHelper();

		bool GetAvailableMonitorCount(uint32_t& monitorCount);
		bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode);
		bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes);
		bool WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title);
		void* GetWindowPtr();
		bool WindowCreateWithSize(uint32_t width, uint32_t height, std::string title);
		bool GetWindowSize(uint32_t& width, uint32_t& height);
		bool WindowPreRender(bool& exitRequested);
		bool WindowPostRender();
		void PollEvents(void);
		bool WindowClose();
		bool GetKeyPressed(uint32_t key, uint32_t& pressed);
		bool GetMouseButtonPressed(uint32_t button, uint32_t& pressed);
		bool GetMouseCursorPosition(double& xPos, double& yPos);
		bool SetMouseCursorPosition(double xPos, double yPos);
		bool GetClipboardString(std::string& value);
		bool SetClipboardString(std::string value);

		bool SetCursorMode(uint32_t mode);
		bool JoystickIsPresent(uint32_t joystickID, uint32_t& present);
		bool JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad);
		bool GetJoystickButtonStates(uint32_t joystickID, JoystickButtonStates& joystickButtonStates);
		bool GetJoystickAxisStates(uint32_t joystickID, JoystickAxisStates& joystickAxisStates);
		bool GetJoystickHatCount(uint32_t joystickID, uint32_t& count);
		bool GetJoystickHatDirection(uint32_t joystickID, uint32_t hatIndex, uint32_t& direction);

		IDirect3DDevice8* GetD3DDevice();

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
