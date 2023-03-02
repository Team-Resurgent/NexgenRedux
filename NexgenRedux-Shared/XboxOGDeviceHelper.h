#if defined NEXGEN_OG

#pragma once

#include "WindowManager.h"

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
		static bool WindowCreate(WindowManager::MonitorVideoMode monitorVideoMode, IDirect3DDevice8** d3dDevice);
		static bool WindowCreate(int width, int height, IDirect3DDevice8** d3dDevice);
		static void WindowDispose(void* window);
	};
}

#endif
