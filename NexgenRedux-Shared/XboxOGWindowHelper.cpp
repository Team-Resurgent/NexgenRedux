#if defined NEXGEN_OG

#include "XboxOGWindowHelper.h"
#include "XboxOGRenderingHelper.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "IWindowHelper.h"
#include "MeshUtility.h"
#include "MathUtility.h"
#include "Constants.h"

#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>
#include <Gensys/TimeUtility.h>

#include <map>
#include <xtl.h>

using namespace Gensys;
using namespace NexgenRedux;

XboxOGWindowHelper::XboxOGWindowHelper()
{
	m_initialized = false;
	m_d3dDevice = NULL;
	m_clipboardValue = "";
	m_mouseX = 0;
	m_mouseY = 0;
	m_keyboardModifier = 0;

	for (uint32_t i = 0; i < XGetPortCount(); i++)
	{
		m_controllerHandles[i] = 0;
	}

	for (uint32_t i = 0; i < KEY_LAST; i++)
	{
		m_keyboardKeys[i] = false;
	}

	for (uint32_t i = 0; i < 5; i++)
	{
		m_mouseButtons[i] = false;
		m_previousMouseButtons[i] = false;
	}

	for (uint32_t i = 0; i < XGetPortCount() * 2; i++)
	{
		m_keyboardHandles[i] = 0;
		m_mouseHandles[i] = 0;

		XINPUT_STATE state;
		memset(&state, 0, sizeof(state));
		m_mouseStates[i] = state;
		m_lastMousePacket[i] = 0;

		uint32_t x = i >= 4 ? i + 12 : i;
		m_keyboardMaskTable[i] = 1 << x;
		m_mouseMaskTable[i] = 1 << x;
	}
}

XboxOGWindowHelper::~XboxOGWindowHelper()
{
    WindowClose();

	for (uint32_t i = 0; i < XGetPortCount(); i++) 
	{
		if (m_controllerHandles[i] != NULL) 
		{
			XInputClose(m_controllerHandles[i]);
		}
	}
}

bool XboxOGWindowHelper::GetAvailableMonitorCount(uint32_t& monitorCount)
{
	monitorCount = 1;
	return true;
}

bool XboxOGWindowHelper::GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode)
{
	uint32_t videoModesCount = IDirect3D8::GetAdapterModeCount(D3DADAPTER_DEFAULT);
	for (uint32_t i = 0; i < videoModesCount; i++ )
	{
		D3DDISPLAYMODE displayMode;
		IDirect3D8::EnumAdapterModes(D3DADAPTER_DEFAULT, i, &displayMode);

		if ((displayMode.Flags & D3DPRESENTFLAG_10X11PIXELASPECTRATIO) == D3DPRESENTFLAG_10X11PIXELASPECTRATIO)
		{
			continue;
		}

		if (displayMode.Format == D3DFMT_LIN_R5G6B5) 
		{
			monitorVideoMode.redBits = 5;
			monitorVideoMode.greenBits = 6;
			monitorVideoMode.blueBits = 5;
		}
		else if (displayMode.Format == D3DFMT_LIN_X1R5G5B5) 
		{
			monitorVideoMode.redBits = 5;
			monitorVideoMode.greenBits = 5;
			monitorVideoMode.blueBits = 5;
		}
		else if (displayMode.Format == D3DFMT_LIN_X8R8G8B8) 
		{
			monitorVideoMode.redBits = 8;
			monitorVideoMode.greenBits = 8;
			monitorVideoMode.blueBits = 8;
		}
		else
		{
			continue;
		}

		uint32_t heightMultiple = (displayMode.Flags & D3DPRESENTFLAG_FIELD) == D3DPRESENTFLAG_FIELD ? 2 : 1;

		monitorVideoMode.monitorIndex = monitorIndex;
		monitorVideoMode.width = displayMode.Width;
		monitorVideoMode.height = displayMode.Height * heightMultiple;
		monitorVideoMode.refreshRate = displayMode.RefreshRate;
		return true;
	}

	return false;
}

bool XboxOGWindowHelper::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes)
{
	uint32_t videoModesCount = IDirect3D8::GetAdapterModeCount(D3DADAPTER_DEFAULT);
	for (uint32_t i = 0; i < videoModesCount; i++ )
	{
		D3DDISPLAYMODE displayMode;
		IDirect3D8::EnumAdapterModes(D3DADAPTER_DEFAULT, i, &displayMode);
		MonitorVideoMode monitorVideoMode;
		if ((displayMode.Flags & D3DPRESENTFLAG_10X11PIXELASPECTRATIO) == D3DPRESENTFLAG_10X11PIXELASPECTRATIO)
		{
			continue;
		}
		if (displayMode.Format == D3DFMT_LIN_R5G6B5) 
		{
			monitorVideoMode.redBits = 5;
			monitorVideoMode.greenBits = 6;
			monitorVideoMode.blueBits = 5;
		}
		else if (displayMode.Format == D3DFMT_LIN_X1R5G5B5) 
		{
			monitorVideoMode.redBits = 5;
			monitorVideoMode.greenBits = 5;
			monitorVideoMode.blueBits = 5;
		}
		else if (displayMode.Format == D3DFMT_LIN_X8R8G8B8) 
		{
			monitorVideoMode.redBits = 8;
			monitorVideoMode.greenBits = 8;
			monitorVideoMode.blueBits = 8;
		}
		else
		{
			continue;
		}

		uint32_t heightMultiple = (displayMode.Flags & D3DPRESENTFLAG_FIELD) == D3DPRESENTFLAG_FIELD ? 2 : 1;

		monitorVideoMode.monitorIndex = monitorIndex;
        monitorVideoMode.width = displayMode.Width;
        monitorVideoMode.height = displayMode.Height * heightMultiple;
		monitorVideoMode.refreshRate = displayMode.RefreshRate;

		bool found = false;
		for (uint32_t i = 0; i < (uint32_t)monitorVideoModes.size(); i++)
		{
			MonitorVideoMode* currentMonitorVideoMode = &monitorVideoModes.at(i);
			if (currentMonitorVideoMode->redBits == monitorVideoMode.redBits &&
				currentMonitorVideoMode->greenBits == monitorVideoMode.greenBits &&
				currentMonitorVideoMode->blueBits == monitorVideoMode.blueBits &&
				currentMonitorVideoMode->width == monitorVideoMode.width &&
				currentMonitorVideoMode->height == monitorVideoMode.height &&
				currentMonitorVideoMode->refreshRate == monitorVideoMode.refreshRate) 
			{
				found = true;
				break;
			}
		}

		if (found == true)
		{
			continue;
		}

        monitorVideoModes.push_back(monitorVideoMode);
	}
	return true;
}

bool XboxOGWindowHelper::WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title)
{
	if (m_d3dDevice != NULL)
	{
		return false;
	}

	int32_t selectedPriority = -1;
	uint32_t selectedVideoMode = 0;

	uint32_t videoModesCount = IDirect3D8::GetAdapterModeCount(D3DADAPTER_DEFAULT);
	for (uint32_t i = 0; i < videoModesCount; i++ )
	{
		D3DDISPLAYMODE displayMode;
		IDirect3D8::EnumAdapterModes(D3DADAPTER_DEFAULT, i, &displayMode);

		if ((displayMode.Flags & D3DPRESENTFLAG_10X11PIXELASPECTRATIO) == D3DPRESENTFLAG_10X11PIXELASPECTRATIO)
		{
			continue;
		}

		int32_t priority = 0;
		if ((displayMode.Flags & D3DPRESENTFLAG_PROGRESSIVE) > 0 &&
			(displayMode.Flags & D3DPRESENTFLAG_WIDESCREEN) == 0)
		{
			priority = 1;
		}
		else if ((displayMode.Flags & D3DPRESENTFLAG_PROGRESSIVE) > 0 &&
			(displayMode.Flags & D3DPRESENTFLAG_WIDESCREEN) > 0)
		{
			priority = 2;
		}

		uint32_t heightMultiple = (displayMode.Flags & D3DPRESENTFLAG_FIELD) == D3DPRESENTFLAG_FIELD ? 2 : 1;
		uint32_t totalBits = monitorVideoMode.redBits + monitorVideoMode.greenBits + monitorVideoMode.blueBits;

		if (displayMode.Format == D3DFMT_LIN_R5G6B5 && totalBits == 16 &&
			displayMode.Width == monitorVideoMode.width &&
			displayMode.Height * heightMultiple == monitorVideoMode.height &&
			displayMode.RefreshRate == monitorVideoMode.refreshRate &&
			priority > selectedPriority) 
		{
			selectedPriority = priority;
			selectedVideoMode = i;
		}
		else if (displayMode.Format == D3DFMT_LIN_X1R5G5B5 && totalBits == 15 &&
			displayMode.Width == monitorVideoMode.width &&
			displayMode.Height * heightMultiple == monitorVideoMode.height &&
			displayMode.RefreshRate == monitorVideoMode.refreshRate &&
			priority > selectedPriority) 
		{
			selectedPriority = priority;
			selectedVideoMode = i;
		}
		else if (displayMode.Format == D3DFMT_LIN_X8R8G8B8 && totalBits == 24 &&
			displayMode.Width == monitorVideoMode.width &&
			displayMode.Height * heightMultiple == monitorVideoMode.height &&
			displayMode.RefreshRate == monitorVideoMode.refreshRate &&
			priority > selectedPriority) 
		{
			selectedPriority = priority;
			selectedVideoMode = i;
		}
	}

	if (selectedPriority < 0)
	{
		return false;
	}

	D3DDISPLAYMODE selectedDisplayMode;
	IDirect3D8::EnumAdapterModes(D3DADAPTER_DEFAULT, selectedVideoMode, &selectedDisplayMode);

	D3DPRESENT_PARAMETERS d3dPresentParameters;
	memset(&d3dPresentParameters, 0, sizeof(d3dPresentParameters));
	d3dPresentParameters.BackBufferWidth = selectedDisplayMode.Width;
	d3dPresentParameters.BackBufferHeight = selectedDisplayMode.Height;
	d3dPresentParameters.Flags = selectedDisplayMode.Flags;
	d3dPresentParameters.BackBufferCount = 1;	
	d3dPresentParameters.BackBufferFormat = selectedDisplayMode.Format;
	d3dPresentParameters.EnableAutoDepthStencil = TRUE;
	d3dPresentParameters.AutoDepthStencilFormat = D3DFMT_D24S8;	
	d3dPresentParameters.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (FAILED(IDirect3D8::CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dPresentParameters, &m_d3dDevice)))
	{
		return false;
	}

	m_width = selectedDisplayMode.Width;
    m_height = selectedDisplayMode.Height;
	return true;
}

void* XboxOGWindowHelper::GetWindowPtr()
{
    return m_d3dDevice;
}

bool XboxOGWindowHelper::WindowCreateWithSize(uint32_t width, uint32_t height, std::string title)
{
	if (m_d3dDevice != NULL)
	{
		return false;
	}

	int32_t selectedPriority = -1;
	uint32_t selectedVideoMode = 0;

	uint32_t videoModesCount = IDirect3D8::GetAdapterModeCount(D3DADAPTER_DEFAULT);
	for (uint32_t i = 0; i < videoModesCount; i++ )
	{
		D3DDISPLAYMODE displayMode;
		IDirect3D8::EnumAdapterModes(D3DADAPTER_DEFAULT, i, &displayMode);

		if ((displayMode.Flags & D3DPRESENTFLAG_10X11PIXELASPECTRATIO) == D3DPRESENTFLAG_10X11PIXELASPECTRATIO)
		{
			continue;
		}

		int32_t priority = 0;
		if ((displayMode.Flags & D3DPRESENTFLAG_PROGRESSIVE) > 0 &&
			(displayMode.Flags & D3DPRESENTFLAG_WIDESCREEN) == 0)
		{
			priority = 1;
		}
		else if ((displayMode.Flags & D3DPRESENTFLAG_PROGRESSIVE) > 0 &&
			(displayMode.Flags & D3DPRESENTFLAG_WIDESCREEN) > 0)
		{
			priority = 2;
		}

		uint32_t heightMultiple = (displayMode.Flags & D3DPRESENTFLAG_FIELD) == D3DPRESENTFLAG_FIELD ? 2 : 1;

		if (displayMode.Format == D3DFMT_LIN_X8R8G8B8 &&
			displayMode.Width == width &&
			displayMode.Height * heightMultiple == height &&
			priority > selectedPriority)
		{
			selectedPriority = priority;
			selectedVideoMode = i;
		}
	}

	if (selectedPriority < 0)
	{
		return false;
	}

	D3DDISPLAYMODE selectedDisplayMode;
	IDirect3D8::EnumAdapterModes(D3DADAPTER_DEFAULT, selectedVideoMode, &selectedDisplayMode);

	D3DPRESENT_PARAMETERS d3dPresentParameters;
	memset(&d3dPresentParameters, 0, sizeof(d3dPresentParameters));
	d3dPresentParameters.BackBufferWidth = selectedDisplayMode.Width;
	d3dPresentParameters.BackBufferHeight = selectedDisplayMode.Height;
	d3dPresentParameters.Flags = selectedDisplayMode.Flags;
	d3dPresentParameters.BackBufferCount = 1;	
	d3dPresentParameters.BackBufferFormat = selectedDisplayMode.Format;
	d3dPresentParameters.EnableAutoDepthStencil = TRUE;
	d3dPresentParameters.AutoDepthStencilFormat = D3DFMT_D24S8;	
	d3dPresentParameters.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (FAILED(IDirect3D8::CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dPresentParameters, &m_d3dDevice)))
	{
		return false;
	}

	m_width = selectedDisplayMode.Width;
    m_height = selectedDisplayMode.Height;
	return true;
}

bool XboxOGWindowHelper::GetWindowSize(uint32_t& width, uint32_t& height)
{
	width = m_width;
	height = m_height;
	return true;
}

bool XboxOGWindowHelper::SetCursorMode(CursorMode mode)
{
	return true;
}

bool XboxOGWindowHelper::WindowPreRender(bool& exitRequested)
{
	return true;
}

bool XboxOGWindowHelper::WindowPostRender()
{
	if (FAILED(m_d3dDevice->Present(NULL, NULL, NULL, NULL)))
	{
		return false;
	}
	return true;
}

void XboxOGWindowHelper::PollEvents(void)
{
	ProcessController();
	ProcessKeyboard();
	ProcessMouse();
}

bool XboxOGWindowHelper::WindowClose()
{
	if (m_d3dDevice != NULL)
	{
		m_d3dDevice->Release();
		m_d3dDevice = NULL;
	}
	return true;
}

bool XboxOGWindowHelper::GetKeyPressed(uint32_t key, KeyButtonState& state)
{
	state = KeyButtonStateNone;

	if (key >= KEY_LAST)
	{
		return true;
	}

	state = m_keyboardKeys[key] == true ? KeyButtonStatePressed : KeyButtonStateNone;
	return true;
}

bool XboxOGWindowHelper::GetMouseButtonPressed(uint32_t button, MouseButtonState& state)
{
	state = MouseButtonStateNone;

	if (button >= 5)
	{
		return true;
	}

	state = m_mouseButtons[button] == true ? MouseButtonStatePressed : MouseButtonStateNone;
    return true;
}

bool XboxOGWindowHelper::GetMouseCursorPosition(double& xPos, double& yPos)
{
	xPos = m_mouseX;
	yPos = m_mouseY;
    return true;
}

bool XboxOGWindowHelper::SetMouseCursorPosition(double xPos, double yPos)
{
	m_mouseX = (int32_t)xPos;
	m_mouseY = (int32_t)yPos;
    return true;
}

bool XboxOGWindowHelper::GetClipboardString(std::string& value)
{
	value = m_clipboardValue;
	return true;
}

bool XboxOGWindowHelper::SetClipboardString(std::string value)
{
	m_clipboardValue = value;
	return true;
}

bool XboxOGWindowHelper::JoystickIsPresent(uint32_t joystickID, uint32_t& present)
{
	if (Init() == false)
    {
        return false;
    }

	if (joystickID >= XGetPortCount())
	{
		present = 0;
		return true;
	}

	present = m_controllerHandles[joystickID] == NULL ? 0 : 1;
	return true;
}

bool XboxOGWindowHelper::JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad)
{
    if (Init() == false)
    {
        return false;
    }

	gamepad = 1;
	return true;
}

bool XboxOGWindowHelper::GetJoystickButtonStates(uint32_t joystickID, JoystickButtonStates& joystickButtonStates)
{
    if (Init() == false)
    {
        return false;
    }

	memset(&joystickButtonStates, 0, sizeof(joystickButtonStates));

	if (joystickID >= XGetPortCount() || m_controllerHandles[joystickID] == NULL)
	{
		return true;
	}

	XINPUT_STATE inputStates;
	if (XInputGetState(m_controllerHandles[joystickID], &inputStates) != 0) 
	{
		return true;
	}

	XINPUT_GAMEPAD gamePad;
	memcpy(&gamePad, &inputStates.Gamepad, sizeof(XINPUT_GAMEPAD));

	joystickButtonStates.buttonA = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_A] > 30) ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonB = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_B] > 30) ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonX = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_X] > 30) ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonY = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_Y] > 30) ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonLeftBumperOrWhite = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_WHITE] > 30) ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonRightBumperOrBlack = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_BLACK] > 30) ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonBack = (gamePad.wButtons & XINPUT_GAMEPAD_BACK) == XINPUT_GAMEPAD_BACK ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonStart = (gamePad.wButtons & XINPUT_GAMEPAD_START) == XINPUT_GAMEPAD_START ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonGuide = JoystickButtonStateNone;
	joystickButtonStates.buttonLeftThumb = (gamePad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) == XINPUT_GAMEPAD_LEFT_THUMB ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonRightThumb = (gamePad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) == XINPUT_GAMEPAD_RIGHT_THUMB ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonDpadUp = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_UP) == XINPUT_GAMEPAD_DPAD_UP ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonDpadRight = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) == XINPUT_GAMEPAD_DPAD_RIGHT ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonDpadDown = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) == XINPUT_GAMEPAD_DPAD_DOWN ? JoystickButtonStatePressed : JoystickButtonStateNone;
	joystickButtonStates.buttonDpadLeft = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) == XINPUT_GAMEPAD_DPAD_LEFT ? JoystickButtonStatePressed : JoystickButtonStateNone;
	return true;
}

bool XboxOGWindowHelper::GetJoystickAxisStates(uint32_t joystickID, JoystickAxisStates& joystickAxisStates)
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

	if (joystickID >= XGetPortCount() || m_controllerHandles[joystickID] == NULL)
	{
		return true;
	}

	XINPUT_STATE inputStates;
	if (XInputGetState(m_controllerHandles[joystickID], &inputStates) != 0) 
	{
		return true;
	}

	XINPUT_GAMEPAD gamePad;
	memcpy(&gamePad, &inputStates.Gamepad, sizeof(XINPUT_GAMEPAD));

	joystickAxisStates.axisLeftX = gamePad.sThumbLX > 0 ? gamePad.sThumbLX / (float)32768 : gamePad.sThumbLX / (float)32767;
	joystickAxisStates.axisLeftY = gamePad.sThumbLY > 0 ? gamePad.sThumbLY / -(float)32768 : gamePad.sThumbLY / -(float)32767;
	joystickAxisStates.axisRightX = gamePad.sThumbRX > 0 ? gamePad.sThumbRX / (float)32768 : gamePad.sThumbRX / (float)32767;
	joystickAxisStates.axisRightY = gamePad.sThumbRY > 0 ? gamePad.sThumbRY / -(float)32768 : gamePad.sThumbRY / -(float)32767;
	joystickAxisStates.axisLeftTrigger = ((gamePad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] / 255.0f) * 2) - 1;
	joystickAxisStates.axisRightTrigger = ((gamePad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] / 255.0f) * 2) - 1;
	return true;
}

bool XboxOGWindowHelper::GetJoystickHatCount(uint32_t joystickID, uint32_t& count)
{
    if (Init() == false)
    {
        return false;
    }

	if (joystickID >= XGetPortCount() || m_controllerHandles[joystickID] == NULL)
	{
		count = 0;
		return true;
	}

	count = 1;
	return true;
}

bool XboxOGWindowHelper::GetJoystickHatDirection(uint32_t joystickID, uint32_t hatIndex, uint32_t& direction)
{
    if (Init() == false)
    {
        return false;
    }

	direction = 0;

	if (joystickID >= XGetPortCount() || hatIndex >= 1 || m_controllerHandles[joystickID] == NULL)
	{
		return true;
	}

	XINPUT_STATE inputStates;
	if (XInputGetState(m_controllerHandles[joystickID], &inputStates) != 0) 
	{
		return true;
	}

	XINPUT_GAMEPAD gamePad;
	memcpy(&gamePad, &inputStates.Gamepad, sizeof(XINPUT_GAMEPAD));
	
	direction = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_UP) == XINPUT_GAMEPAD_DPAD_UP ? 1 : 0;
	direction |= (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) == XINPUT_GAMEPAD_DPAD_RIGHT ? 2 : 0;
	direction |= (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) == XINPUT_GAMEPAD_DPAD_DOWN ? 4 : 0;
	direction |= (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) == XINPUT_GAMEPAD_DPAD_LEFT ? 8 : 0;
	return true;
}

IDirect3DDevice8* XboxOGWindowHelper::GetD3DDevice()
{
	return m_d3dDevice;
}

// Privates

bool XboxOGWindowHelper::Init(void) 
{
    if (m_initialized == false)
    {
        m_initialized = true;
		XInitDevices(0, 0);
		InitKeyboard();
		InitMouse();
    }
    return true;
}

bool XboxOGWindowHelper::InitKeyboard(void)
{
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x30, KeyInfo(0x00B, KEY_0)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x31, KeyInfo(0x002, KEY_1)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x32, KeyInfo(0x003, KEY_2)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x33, KeyInfo(0x004, KEY_3)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x34, KeyInfo(0x005, KEY_4)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x35, KeyInfo(0x006, KEY_5)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x36, KeyInfo(0x007, KEY_6)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x37, KeyInfo(0x008, KEY_7)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x38, KeyInfo(0x009, KEY_8)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x39, KeyInfo(0x00A, KEY_9)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x41, KeyInfo(0x01E, KEY_A)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x42, KeyInfo(0x030, KEY_B)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x43, KeyInfo(0x02E, KEY_C)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x44, KeyInfo(0x020, KEY_D)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x45, KeyInfo(0x012, KEY_E)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x46, KeyInfo(0x021, KEY_F)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x47, KeyInfo(0x022, KEY_G)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x48, KeyInfo(0x023, KEY_H))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x49, KeyInfo(0x017, KEY_I)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x4A, KeyInfo(0x024, KEY_J))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x4B, KeyInfo(0x025, KEY_K)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x4C, KeyInfo(0x026, KEY_L))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x4D, KeyInfo(0x032, KEY_M)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x4E, KeyInfo(0x031, KEY_N))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x4F, KeyInfo(0x018, KEY_O)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x50, KeyInfo(0x019, KEY_P)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x51, KeyInfo(0x010, KEY_Q)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x52, KeyInfo(0x013, KEY_R)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x53, KeyInfo(0x01F, KEY_S))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x54, KeyInfo(0x014, KEY_T))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x55, KeyInfo(0x016, KEY_U))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x56, KeyInfo(0x02F, KEY_V))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x57, KeyInfo(0x011, KEY_W))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x58, KeyInfo(0x02D, KEY_X))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x59, KeyInfo(0x015, KEY_Y)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(0x5A, KeyInfo(0x02C, KEY_Z)));

 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_7, KeyInfo(0x028, KEY_APOSTROPHE)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_5, KeyInfo(0x02B, KEY_BACKSLASH)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_COMMA, KeyInfo(0x033, KEY_COMMA))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_PLUS, KeyInfo(0x00D, KEY_EQUAL))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_3, KeyInfo(0x029, KEY_GRAVE_ACCENT))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_4, KeyInfo(0x01A, KEY_LEFT_BRACKET))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_MINUS, KeyInfo(0x00C, KEY_MINUS))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_PERIOD, KeyInfo(0x034, KEY_PERIOD))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_6, KeyInfo(0x01B, KEY_RIGHT_BRACKET)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_1, KeyInfo(0x027, KEY_SEMICOLON))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_2, KeyInfo(0x035, KEY_SLASH)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_102, KeyInfo(0x056, KEY_WORLD_2)));

 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_BACK, KeyInfo(0x00E, KEY_BACKSPACE)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_DELETE, KeyInfo(0x153, KEY_DELETE)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_END, KeyInfo(0x14F, KEY_END)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_RETURN, KeyInfo(0x01C, KEY_ENTER)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_ESCAPE, KeyInfo(0x001, KEY_ESCAPE)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_HOME, KeyInfo(0x147, KEY_HOME))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_INSERT, KeyInfo(0x152, KEY_INSERT))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_MENU, KeyInfo(0x15D, KEY_MENU)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NEXT, KeyInfo(0x151, KEY_PAGE_DOWN))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_PRIOR, KeyInfo(0x149, KEY_PAGE_UP)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_PAUSE, KeyInfo(0x045, KEY_PAUSE))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_SPACE, KeyInfo(0x039, KEY_SPACE))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_TAB, KeyInfo(0x00F, KEY_TAB)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_CAPITAL, KeyInfo(0x03A, KEY_CAPS_LOCK)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMLOCK, KeyInfo(0x145, KEY_NUM_LOCK)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_SCROLL, KeyInfo(0x046, KEY_SCROLL_LOCK)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F1, KeyInfo(0x03B, KEY_F1))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F2, KeyInfo(0x03C, KEY_F2)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F3, KeyInfo(0x03D, KEY_F3)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F4, KeyInfo(0x03E, KEY_F4))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F5, KeyInfo(0x03F, KEY_F5))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F6, KeyInfo(0x040, KEY_F6))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F7, KeyInfo(0x041, KEY_F7))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F8, KeyInfo(0x042, KEY_F8))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F9, KeyInfo(0x043, KEY_F9)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F10, KeyInfo(0x044, KEY_F10))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F11, KeyInfo(0x057, KEY_F11))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F12, KeyInfo(0x058, KEY_F12))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F13, KeyInfo(0x064, KEY_F13))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F14, KeyInfo(0x065, KEY_F14))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F15, KeyInfo(0x066, KEY_F15)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F16, KeyInfo(0x067, KEY_F16))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F17, KeyInfo(0x068, KEY_F17))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F18, KeyInfo(0x069, KEY_F18)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F19, KeyInfo(0x06A, KEY_F19)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F20, KeyInfo(0x06B, KEY_F20))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F21, KeyInfo(0x06C, KEY_F21))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F22, KeyInfo(0x06D, KEY_F22))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F23, KeyInfo(0x06E, KEY_F23))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_F24, KeyInfo(0x076, KEY_F24))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_LMENU, KeyInfo(0x038, KEY_LEFT_ALT)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_LCONTROL, KeyInfo(0x01D, KEY_LEFT_CONTROL)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_LSHIFT, KeyInfo(0x02A, KEY_LEFT_SHIFT))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_LWIN, KeyInfo(0x15B, KEY_LEFT_SUPER))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_SNAPSHOT, KeyInfo(0x137, KEY_PRINT_SCREEN))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_RMENU, KeyInfo(0x138, KEY_RIGHT_ALT)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_RCONTROL, KeyInfo(0x11D, KEY_RIGHT_CONTROL))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_RSHIFT, KeyInfo(0x036, KEY_RIGHT_SHIFT)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_RWIN, KeyInfo(0x15C, KEY_RIGHT_SUPER))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_DOWN, KeyInfo(0x150, KEY_DOWN))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_LEFT, KeyInfo(0x14B, KEY_LEFT))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_RIGHT, KeyInfo(0x14D, KEY_RIGHT)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_UP, KeyInfo(0x148, KEY_UP)));

 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD0, KeyInfo(0x052, KEY_KP_0))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD1, KeyInfo(0x04F, KEY_KP_1))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD2, KeyInfo(0x050, KEY_KP_2))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD3, KeyInfo(0x051, KEY_KP_3))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD4, KeyInfo(0x04B, KEY_KP_4))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD5, KeyInfo(0x04C, KEY_KP_5))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD6, KeyInfo(0x04D, KEY_KP_6)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD7, KeyInfo(0x047, KEY_KP_7)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD8, KeyInfo(0x048, KEY_KP_8))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_NUMPAD9, KeyInfo(0x049, KEY_KP_9)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_ADD, KeyInfo(0x04E, KEY_KP_ADD)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_DECIMAL, KeyInfo(0x053, KEY_KP_DECIMAL)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_DIVIDE, KeyInfo(0x135, KEY_KP_DIVIDE)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_SEPARATOR, KeyInfo(0x11C, KEY_KP_ENTER))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_OEM_NEC_EQUAL, KeyInfo(0x059, KEY_KP_EQUAL)));
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_MULTIPLY, KeyInfo(0x037, KEY_KP_MULTIPLY))); 
 	m_virtualKeyToScancodeMap.insert(std::pair<uint32_t, KeyInfo>(VK_SUBTRACT, KeyInfo(0x04A, KEY_KP_SUBTRACT)));

	XINPUT_DEBUG_KEYQUEUE_PARAMETERS keyboardSettings;
	keyboardSettings.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYUP;
	keyboardSettings.dwQueueSize = 25;
	keyboardSettings.dwRepeatDelay = 500;
	keyboardSettings.dwRepeatInterval = 50;

	if (XInputDebugInitKeyboardQueue(&keyboardSettings) != ERROR_SUCCESS)
	{
		return false;
	}

	uint32_t keyboardPort = XGetDevices(XDEVICE_TYPE_DEBUG_KEYBOARD);

	for (uint32_t i = 0; i < XGetPortCount() * 2; i++)
	{
		if ((m_keyboardHandles[i] == NULL) && (keyboardPort & m_keyboardMaskTable[i]))
		{
			XINPUT_POLLING_PARAMETERS pollValues;
			pollValues.fAutoPoll = TRUE;
			pollValues.fInterruptOut = TRUE;
			pollValues.bInputInterval = 32;
			pollValues.bOutputInterval = 32;
			pollValues.ReservedMBZ1 = 0;
			pollValues.ReservedMBZ2 = 0;

			if (i < XGetPortCount())
			{
				m_keyboardHandles[i] = XInputOpen( XDEVICE_TYPE_DEBUG_KEYBOARD, i, XDEVICE_NO_SLOT, &pollValues );
			}
			else
			{
				m_keyboardHandles[i] = XInputOpen( XDEVICE_TYPE_DEBUG_KEYBOARD, i - XGetPortCount(), XDEVICE_BOTTOM_SLOT, &pollValues );
			}
		}
	}
	return true;
}

bool XboxOGWindowHelper::InitMouse(void)
{
	uint32_t mousePort = XGetDevices(XDEVICE_TYPE_DEBUG_MOUSE);
	for (uint32_t i = 0; i < XGetPortCount() * 2; i++)
	{
		if ((m_mouseHandles[i] == NULL ) && (mousePort & m_mouseMaskTable[i]))
		{
			if (i < XGetPortCount())
			{
				m_mouseHandles[i] = XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, i, XDEVICE_NO_SLOT, NULL);
			}
			else
			{
				m_mouseHandles[i] = XInputOpen( XDEVICE_TYPE_DEBUG_MOUSE, i - XGetPortCount(), XDEVICE_BOTTOM_SLOT, NULL);
			}
		}
	}
	return true;
}

void XboxOGWindowHelper::ProcessController()
{
    DWORD insertions;
	DWORD removals;
    XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals);
	for (uint32_t i = 0; i < XGetPortCount(); i++)
	{
		if ((insertions & 1) == 1)
		{
			m_controllerHandles[i] = XInputOpen(XDEVICE_TYPE_GAMEPAD, i, XDEVICE_NO_SLOT, NULL);

			if (AngelScriptRunner::ExecuteJoystickConnectCallback(i, 1) == false)
			{
				DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteJoystickConnectCallback failed.");
			}
		}
		if ((removals & 1) == 1)
		{
			XInputClose(m_controllerHandles[i]);
			m_controllerHandles[i] = NULL;

			if (AngelScriptRunner::ExecuteJoystickConnectCallback(i, 0) == false)
			{
				DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteJoystickConnectCallback failed.");
			}
		}
		insertions = insertions >> 1;
		removals = removals >> 1;
	}
}

void XboxOGWindowHelper::ProcessKeyboard()
{
	uint32_t insertions;
	uint32_t removals;
	XGetDeviceChanges(XDEVICE_TYPE_DEBUG_KEYBOARD, &insertions, &removals);

	XINPUT_DEBUG_KEYSTROKE currentKeyStroke;
	memset(&currentKeyStroke, 0, sizeof(currentKeyStroke));

	for ( uint32_t i = 0; i < XGetPortCount()* 2; i++ )
	{
		if ((removals & m_keyboardMaskTable[i]) > 0)
		{
			XInputClose( m_keyboardHandles[i] );
			m_keyboardHandles[i] = NULL;
		}

		if ((insertions & m_keyboardMaskTable[i]) > 0)
		{
			XINPUT_POLLING_PARAMETERS pollValues;
			pollValues.fAutoPoll = TRUE;
			pollValues.fInterruptOut = TRUE;
			pollValues.bInputInterval = 32;
			pollValues.bOutputInterval = 32;
			pollValues.ReservedMBZ1 = 0;
			pollValues.ReservedMBZ2 = 0;

			if (i < XGetPortCount())
			{
				m_keyboardHandles[i] = XInputOpen( XDEVICE_TYPE_DEBUG_KEYBOARD, i, XDEVICE_NO_SLOT, &pollValues );
			}
			else
			{
				m_keyboardHandles[i] = XInputOpen( XDEVICE_TYPE_DEBUG_KEYBOARD, i - XGetPortCount(), XDEVICE_BOTTOM_SLOT, &pollValues );
			}
		}

		if (m_keyboardHandles[i] && XInputDebugGetKeystroke(&currentKeyStroke) == ERROR_SUCCESS)
		{
			break;
		}
	}

	uint32_t keyState = 0;
	if (currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_KEYUP)
	{
		keyState = 1;
	}
	else if (currentKeyStroke.VirtualKey != 0 || currentKeyStroke.Ascii != 0) 
	{
		keyState = 2;
	}

	if (keyState > 0)
	{
		uint32_t modifier = 0;
		modifier |= ((currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_SHIFT) > 0) ? 1 : 0;
		modifier |= ((currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_CTRL) > 0) ? 2 : 0;
		modifier |= ((currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_ALT) > 0) ? 4 : 0;
		modifier |= (currentKeyStroke.VirtualKey == VK_LWIN || currentKeyStroke.VirtualKey == VK_RWIN) ? 8 : 0;
		modifier |= ((currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_CAPSLOCK) > 0) ? 16 : 0;
		modifier |= ((currentKeyStroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_NUMLOCK) > 0) ? 32 : 0;
		m_keyboardModifier = modifier;

		if (currentKeyStroke.Ascii != 0)
		{
			if (AngelScriptRunner::ExecuteWindowKeyboardCharacterCallback(currentKeyStroke.Ascii) == false)
			{
				DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowKeyboardCharacterCallback failed.");
			}
		}

		for (std::map<uint32_t, KeyInfo>::iterator it = m_virtualKeyToScancodeMap.begin(); it != m_virtualKeyToScancodeMap.end(); ++it)
		{
			if (it->first == currentKeyStroke.VirtualKey)
			{
				m_keyboardKeys[it->second.keyID] = (keyState - 1) == 1;
				if (AngelScriptRunner::ExecuteWindowKeyboardKeyCallback(it->second.keyID, it->second.scancode, keyState - 1, modifier) == false)
				{
					DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowKeyboardKeyCallback failed.");
				}
				break;
			} 
		}
	}
}

void XboxOGWindowHelper::ProcessMouse()
{
	uint32_t windowWidth;
	uint32_t windowHeight;
	if (GetWindowSize(windowWidth, windowHeight) == false)
	{
		return;
	}

	uint32_t insertions;
	uint32_t removals;
	if (XGetDeviceChanges(XDEVICE_TYPE_DEBUG_MOUSE, &insertions, &removals))
	{
		for (DWORD i = 0; i < XGetPortCount() * 2; i++)
		{
			if ((removals & m_mouseMaskTable[i]) > 0 && (m_mouseHandles[i] != NULL))
			{
				XInputClose(m_mouseHandles[i]);
				m_mouseHandles[i] = NULL;
			}
		}

		for (uint32_t i = 0; i < XGetPortCount()* 2; i++)
		{
			if ((m_mouseHandles[i] == NULL) && (insertions & m_mouseMaskTable[i]) > 0)
			{
				if (i < XGetPortCount())
				{
					m_mouseHandles[i] = XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, i, XDEVICE_NO_SLOT, NULL);
				}
				else
				{
					m_mouseHandles[i] = XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, i - XGetPortCount(), XDEVICE_BOTTOM_SLOT, NULL);
				}
			}
		}
	}

	uint32_t mouseMoved = 0;
	for (uint32_t i = 0; i < XGetPortCount()*2; i++ )
	{
		if (m_mouseHandles[i]) 
		{
			XInputGetState(m_mouseHandles[i], &m_mouseStates[i]);
		}
		if (m_lastMousePacket[i] != m_mouseStates[i].dwPacketNumber)
		{
			mouseMoved |= m_mouseMaskTable[i];
			m_lastMousePacket[i] = m_mouseStates[i].dwPacketNumber;
		}
	}

	if (mouseMoved > 0)
	{
		for (uint32_t i = 0; i < XGetPortCount() * 2; i++)
		{
			if ((mouseMoved & m_mouseMaskTable[i]) > 0)
			{
				m_mouseX += m_mouseStates[i].DebugMouse.cMickeysX;
				m_mouseY += m_mouseStates[i].DebugMouse.cMickeysY;
				m_mouseX = min(max(m_mouseX, 0), (int32_t)windowWidth);
				m_mouseY = min(max(m_mouseY, 0), (int32_t)windowHeight);

				if (m_mouseStates[i].DebugMouse.cMickeysX > 0 || m_mouseStates[i].DebugMouse.cMickeysY > 0) 
				{
					if (AngelScriptRunner::ExecuteWindowMouseCursorPositionCallback(m_mouseX, m_mouseY) == false)
					{
						DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseCursorPositionCallback failed.");
					}
				}

				m_mouseButtons[0] = (m_mouseStates[i].DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_LEFT_BUTTON) != 0;
				m_mouseButtons[1] = (m_mouseStates[i].DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_RIGHT_BUTTON) != 0;
				m_mouseButtons[2] = (m_mouseStates[i].DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_MIDDLE_BUTTON) != 0;
				m_mouseButtons[3] = (m_mouseStates[i].DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_XBUTTON1) != 0;
				m_mouseButtons[4] = (m_mouseStates[i].DebugMouse.bButtons & XINPUT_DEBUG_MOUSE_XBUTTON2) != 0;

				for (uint32_t j = 0; j < 5; j++) 
				{
					if (m_previousMouseButtons[j] != m_mouseButtons[j]) 
					{
						if (AngelScriptRunner::ExecuteWindowMouseButtonCallback(j, m_mouseButtons[j] ? 1 : 0, m_keyboardModifier) == false)
						{
							DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseCursorPositionCallback failed.");
						}
					}
					m_previousMouseButtons[j] = m_mouseButtons[j];
				}

				if (m_mouseStates[i].DebugMouse.cWheel != 0) 
				{
					if (AngelScriptRunner::ExecuteWindowMouseScrollCallback(0, m_mouseStates[i].DebugMouse.cWheel) == false)
					{
						DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteWindowMouseScrollCallback failed.");
					}
				}

				break;
			}
		}
	}
}

#endif