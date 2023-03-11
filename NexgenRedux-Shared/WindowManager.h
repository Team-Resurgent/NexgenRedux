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

    	static void Close(void);
		static bool GetAvailableMonitorCount(uint32_t& monitorCount);
		static bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode);
		static bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes);
		static bool WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle);
		static bool WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle);
		static bool GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height);
		static bool SetCursorMode(uint32_t windowHandle, uint32_t mode);
        static bool WindowClose(uint32_t windowHandle);
		static bool RenderLoop(void);
		static bool GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed);
		static bool GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed);
		static bool GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos);
		static bool SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos);
		static bool GetClipboardString(std::string& value);
		static bool SetClipboardString(std::string value);
	};
}
