#if defined NEXGEN_OG

#include "XboxOGDeviceHelper.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"

#include <Gensys/DebugUtility.h>

#include <xtl.h>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
	bool m_initialized = false;
	std::string m_clipboardValue = "";
	HANDLE m_controllerHandles[4] = {0};
}

void XboxOGDeviceHelper::Close(void) 
{
	std::vector<uint32_t> windowHandles = WindowManager::GetWindowHandles();
	for (uint32_t i = 0; i < windowHandles.size(); i++) 
	{
		WindowManager::WindowClose(windowHandles.at(i));
	}

	for (uint32_t i = 0; i < 4; i++) 
	{
		if (m_controllerHandles[i] != NULL) 
		{
			XInputClose(m_controllerHandles[i]);
		}
	}
}

void XboxOGDeviceHelper::PollEvents(void)
{
	ProcessController();
}

bool XboxOGDeviceHelper::GetAvailableMonitorCount(uint32_t& monitorCount)
{
	monitorCount = 1;
	return true;
}

bool XboxOGDeviceHelper::GetMonitorVideoMode(uint32_t monitorIndex, WindowManager::MonitorVideoMode& monitorVideoMode)
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

bool XboxOGDeviceHelper::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<WindowManager::MonitorVideoMode>& monitorVideoModes)
{
	uint32_t videoModesCount = IDirect3D8::GetAdapterModeCount(D3DADAPTER_DEFAULT);
	for (uint32_t i = 0; i < videoModesCount; i++ )
	{
		D3DDISPLAYMODE displayMode;
		IDirect3D8::EnumAdapterModes(D3DADAPTER_DEFAULT, i, &displayMode);
		WindowManager::MonitorVideoMode monitorVideoMode;
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
			WindowManager::MonitorVideoMode* currentMonitorVideoMode = &monitorVideoModes.at(i);
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

bool XboxOGDeviceHelper::WindowCreateWithVideoMode(WindowManager::MonitorVideoMode monitorVideoMode, uint32_t& windowHandle)
{
	if (WindowManager::GetWindowCount() > 0)
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

	IDirect3DDevice8* d3dDevice;
	HRESULT hr = IDirect3D8::CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dPresentParameters, &d3dDevice);
	if (FAILED(hr))
	{
		return false;
	}

    WindowManager::WindowContainer windowContainer;
	windowContainer.width = selectedDisplayMode.Width;
    windowContainer.height = selectedDisplayMode.Height;
    windowContainer.window = d3dDevice;
    windowHandle = WindowManager::AddWindowContainer(windowContainer);
	return true;
}

bool XboxOGDeviceHelper::WindowCreateWithSize(uint32_t width, uint32_t height, uint32_t& windowHandle)
{
	if (WindowManager::GetWindowCount() > 0)
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

	IDirect3DDevice8* d3dDevice;
	HRESULT hr = IDirect3D8::CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dPresentParameters, &d3dDevice);
	if (FAILED(hr))
	{
		return false;
	}

    WindowManager::WindowContainer windowContainer;
	windowContainer.width = selectedDisplayMode.Width;
    windowContainer.height = selectedDisplayMode.Height;
    windowContainer.window = d3dDevice;
    windowHandle = WindowManager::AddWindowContainer(windowContainer);
	return true;
}

bool XboxOGDeviceHelper::GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height)
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

bool XboxOGDeviceHelper::SetCursorMode(uint32_t windowHandle, uint32_t mode)
{
	return true;
}

bool XboxOGDeviceHelper::WindowRender(uint32_t& windowHandle, bool& exitRequested)
{
	HRESULT hr;

	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
	if (windowContainer == NULL)
	{
		return false;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)windowContainer->window;

	D3DCOLOR color = D3DCOLOR_RGBA(255, 0, 0, 255);

	hr = d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, color, 0, 0);
	if (FAILED(hr))
	{
		return false;
	}

	hr = d3dDevice->Present(NULL, NULL, NULL, NULL);
	if (FAILED(hr))
	{
		return false;
	}

	return true;
}

bool XboxOGDeviceHelper::WindowClose(uint32_t windowHandle)
{
	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
	if (windowContainer == NULL) 
	{
		return false;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)windowContainer->window;
	d3dDevice->Release();
	WindowManager::DeleteWindowContainer(windowHandle);
	return true;
}

bool XboxOGDeviceHelper::GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed)
{
	pressed = 0;
	return true;
}

bool XboxOGDeviceHelper::GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed)
{
	pressed = 0;
    return true;
}

bool XboxOGDeviceHelper::GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos)
{
	xPos = 0;
	yPos = 0;
    return true;
}

bool XboxOGDeviceHelper::SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos)
{
    return true;
}

bool XboxOGDeviceHelper::GetClipboardString(std::string& value)
{
	value = m_clipboardValue;
	return true;
}

bool XboxOGDeviceHelper::SetClipboardString(std::string value)
{
	m_clipboardValue = value;
	return true;
}

bool XboxOGDeviceHelper::JoystickIsPresent(uint32_t joystickID, uint32_t& present)
{
	if (Init() == false)
    {
        return false;
    }

	if (joystickID >= 4)
	{
		present = 0;
		return true;
	}

	present = m_controllerHandles[joystickID] == NULL ? 0 : 1;
	return true;
}

bool XboxOGDeviceHelper::JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad)
{
    if (Init() == false)
    {
        return false;
    }

	gamepad = 1;
	return true;
}

bool XboxOGDeviceHelper::GetJoystickButtonStates(uint32_t joystickID, JoystickManager::JoystickButtonStates& joystickButtonStates)
{
    if (Init() == false)
    {
        return false;
    }

	if (joystickID >= 4 || m_controllerHandles[joystickID] == NULL)
	{
		memset(&joystickButtonStates, 0, sizeof(joystickButtonStates));
		return true;
	}

	XINPUT_GAMEPAD gamePad;
	XINPUT_STATE inputStates;
	XInputGetState(m_controllerHandles[joystickID], &inputStates);
	memcpy(&gamePad, &inputStates.Gamepad, sizeof(XINPUT_GAMEPAD));

	joystickButtonStates.buttonA = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_A] > 30) ? 1 : 0;
	joystickButtonStates.buttonB = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_B] > 30) ? 1 : 0;
	joystickButtonStates.buttonX = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_X] > 30) ? 1 : 0;
	joystickButtonStates.buttonY = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_Y] > 30) ? 1 : 0;
	joystickButtonStates.buttonLeftBumperOrWhite = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_WHITE] > 30) ? 1 : 0;
	joystickButtonStates.buttonRightBumperOrBlack = (gamePad.bAnalogButtons[XINPUT_GAMEPAD_BLACK] > 30) ? 1 : 0;
	joystickButtonStates.buttonBack = (gamePad.wButtons & XINPUT_GAMEPAD_BACK) == XINPUT_GAMEPAD_BACK ? 1 : 0;
	joystickButtonStates.buttonStart = (gamePad.wButtons & XINPUT_GAMEPAD_START) == XINPUT_GAMEPAD_START ? 1 : 0;
	joystickButtonStates.buttonGuide = 0;
	joystickButtonStates.buttonLeftThumb = (gamePad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) == XINPUT_GAMEPAD_LEFT_THUMB ? 1 : 0;
	joystickButtonStates.buttonRightThumb = (gamePad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) == XINPUT_GAMEPAD_RIGHT_THUMB ? 1 : 0;
	joystickButtonStates.buttonDpadUp = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_UP) == XINPUT_GAMEPAD_DPAD_UP ? 1 : 0;
	joystickButtonStates.buttonDpadRight = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) == XINPUT_GAMEPAD_DPAD_RIGHT ? 1 : 0;
	joystickButtonStates.buttonDpadDown = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) == XINPUT_GAMEPAD_DPAD_DOWN ? 1 : 0;
	joystickButtonStates.buttonDpadLeft = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) == XINPUT_GAMEPAD_DPAD_LEFT ? 1 : 0;
	return true;
}

bool XboxOGDeviceHelper::GetJoystickAxisStates(uint32_t joystickID, JoystickManager::JoystickAxisStates& joystickAxisStates)
{
    if (Init() == false)
    {
        return false;
    }

	if (joystickID >= 4 || m_controllerHandles[joystickID] == NULL)
	{
		joystickAxisStates.axisLeftX = 0;
		joystickAxisStates.axisLeftY = 0;
		joystickAxisStates.axisRightX = 0;
		joystickAxisStates.axisRightY = 0;
		joystickAxisStates.axisLeftTrigger = -1;
		joystickAxisStates.axisRightTrigger = -1;
		return true;
	}

	XINPUT_GAMEPAD gamePad;
	XINPUT_STATE inputStates;
	XInputGetState(m_controllerHandles[joystickID], &inputStates);
	memcpy(&gamePad, &inputStates.Gamepad, sizeof(XINPUT_GAMEPAD));

	joystickAxisStates.axisLeftX = gamePad.sThumbLX > 0 ? gamePad.sThumbLX / (float)32768 : gamePad.sThumbLX / (float)32767;
	joystickAxisStates.axisLeftY = gamePad.sThumbLY > 0 ? gamePad.sThumbLY / (float)32768 : gamePad.sThumbLY / (float)32767;
	joystickAxisStates.axisRightX = gamePad.sThumbRX > 0 ? gamePad.sThumbRX / (float)32768 : gamePad.sThumbRX / (float)32767;
	joystickAxisStates.axisRightY = gamePad.sThumbRY > 0 ? gamePad.sThumbRY / (float)32768 : gamePad.sThumbRY / (float)32767;
	joystickAxisStates.axisLeftTrigger = ((gamePad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] / 255.0f) * 2) - 1;
	joystickAxisStates.axisRightTrigger = ((gamePad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] / 255.0f) * 2) - 1;
	return true;
}

bool XboxOGDeviceHelper::GetJoystickHatCount(uint32_t joystickID, uint32_t& count)
{
    if (Init() == false)
    {
        return false;
    }

	if (joystickID >= 4 || m_controllerHandles[joystickID] == NULL)
	{
		count = 0;
		return true;
	}

	count = 1;
	return true;
}

bool XboxOGDeviceHelper::GetJoystickHatDirection(uint32_t joystickID, uint32_t hatIndex, uint32_t& direction)
{
    if (Init() == false)
    {
        return false;
    }

	if (joystickID >= 4 || hatIndex >= 1 || m_controllerHandles[joystickID] == NULL)
	{
		direction = 0;
		return true;
	}

	XINPUT_GAMEPAD gamePad;
	XINPUT_STATE inputStates;
	XInputGetState(m_controllerHandles[joystickID], &inputStates);
	memcpy(&gamePad, &inputStates.Gamepad, sizeof(XINPUT_GAMEPAD));
	
	direction = (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_UP) == XINPUT_GAMEPAD_DPAD_UP ? 1 : 0;
	direction |= (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) == XINPUT_GAMEPAD_DPAD_RIGHT ? 2 : 0;
	direction |= (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) == XINPUT_GAMEPAD_DPAD_DOWN ? 4 : 0;
	direction |= (gamePad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) == XINPUT_GAMEPAD_DPAD_LEFT ? 8 : 0;
	return true;
}

// Privates

bool XboxOGDeviceHelper::Init(void) 
{
    if (m_initialized == false)
    {
        m_initialized = true;
		XInitDevices(0, 0);
    }
    return true;
}

void XboxOGDeviceHelper::ProcessController()
{
    DWORD insertions;
	DWORD removals;
    XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals);
	for (uint32_t i = 0; i < 4; i++)
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

#endif