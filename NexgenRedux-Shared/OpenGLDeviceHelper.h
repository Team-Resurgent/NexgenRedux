#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#pragma once

#include "WindowManager.h"

#include <Gensys/Int.h>
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#include <vector>

namespace NexgenRedux
{
	class OpenGLDeviceHelper
	{		
	public:
		static bool GetAvailableMonitorCount(uint32_t& monitorCount);
		static bool GetMonitorVideoMode(uint32_t monitorIndex, WindowManager::MonitorVideoMode& monitorVideoMode);
		static bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<WindowManager::MonitorVideoMode>& monitorVideoModes);
		static bool WindowCreate(WindowManager::MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle);
		static bool WindowCreate(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle);
		static bool WindowRender(uint32_t& windowHandle, bool& exitRequested);
		static bool WindowClose(uint32_t windowHandle);
	};
}

#endif
