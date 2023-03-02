#pragma once

#include <Gensys/Int.h>

#include <string>
#include <vector>

namespace NexgenRedux
{
	class WindowManager
	{		
	public:

		typedef struct MonitorVideoMode
		{
			uint32_t monitorIndex;
			uint32_t width;
			uint32_t height; 
			uint32_t redBits;
			uint32_t greenBits;
			uint32_t blueBits;
			uint32_t refreshRate;
		} MonitorVideoMode;

    	static void Dispose(void);
		static bool GetAvailableMonitorCount(uint32_t& monitorCount);
		static bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode);
		static bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes);
		static bool WindowCreate(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle);
		static bool WindowCreate(int width, int height, std::string title, uint32_t& windowHandle);
        static bool RenderLoop();
        static bool WindowClose(uint32_t windowHandle);
	};
}
