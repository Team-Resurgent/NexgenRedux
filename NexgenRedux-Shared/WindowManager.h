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

		typedef struct WindowContainer
		{
			void* window;
		} WindowContainer;

    	static void Close(void);
		static bool GetAvailableMonitorCount(uint32_t& monitorCount);
		static bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode);
		static bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes);
		static bool WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle);
		static bool WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle);
		static bool SetCursorMode(uint32_t windowHandle, uint32_t mode);
        static bool WindowClose(uint32_t windowHandle);
		static bool RenderLoop(void);
		static bool GetClipboardString(std::string& value);
		static bool SetClipboardString(std::string value);

		friend class OpenGLDeviceHelper;
		friend class XboxOGDeviceHelper;

	private:

		static uint32_t GetWindowCount(void);
		static bool GetWindowHandle(WindowContainer windowContainer, uint32_t& windowHandle);
		static std::vector<uint32_t> GetWindowHandles(void);
		static uint32_t AddWindowContainer(WindowContainer windowContainer);
		static WindowContainer* GetWindowContainer(uint32_t windowHandle);
		static void DeleteWindowContainer(uint32_t windowHandle);
	};
}
