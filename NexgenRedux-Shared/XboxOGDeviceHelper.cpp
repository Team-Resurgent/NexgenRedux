#if defined NEXGEN_OG

#include "XboxOGDeviceHelper.h"
#include "WindowManager.h"

#include <xtl.h>

using namespace NexgenRedux;

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
    windowContainer.window = d3dDevice;
    windowHandle = WindowManager::AddWindowContainer(windowContainer);
	return true;
}

bool SetCursorMode(uint32_t windowHandle, uint32_t mode)
{
	return true;
}

bool XboxOGDeviceHelper::WindowRender(uint32_t& windowHandle, bool& exitRequested)
{
	HRESULT hr;

	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
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
	if (windowContainer == NULL) {
		return false;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)windowContainer->window;
	d3dDevice->Release();
	WindowManager::DeleteWindowContainer(windowHandle);
	return true;
}

bool XboxOGDeviceHelper::JoystickIsPresent(uint32_t joystickID, uint32_t& state)
{
	return false;
}

bool XboxOGDeviceHelper::JoystickIsGamepad(uint32_t joystickID, uint32_t& state)
{
	return false;
}

#endif