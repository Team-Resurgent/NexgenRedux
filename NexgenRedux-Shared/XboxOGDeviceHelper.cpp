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

bool XboxOGDeviceHelper::WindowCreate(WindowManager::MonitorVideoMode monitorVideoMode, IDirect3DDevice8** d3dDevice)
{
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

	HRESULT hr = IDirect3D8::CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dPresentParameters, d3dDevice);
	return SUCCEEDED(hr);
}

bool XboxOGDeviceHelper::WindowCreate(int width, int height, IDirect3DDevice8** d3dDevice)
{
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

	HRESULT hr = IDirect3D8::CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dPresentParameters, d3dDevice);
	return SUCCEEDED(hr);
}

void XboxOGDeviceHelper::WindowDispose(void* window)
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)window;
	d3dDevice->Release();
}

#endif