#pragma once

#include "RenderStateManager.h"
#include "IWindowHelper.h"
#include "AngelScriptRunner.h"
#include "GlobalTypes.h"

#include <Gensys/Int.h>

#include <string>
#include <vector>

namespace NexgenRedux
{
	class RenderStateManager;
	class IWindowHelper;
	class AngelScriptRunner;
	class WindowManager
	{		
	public:

		WindowManager();
		~WindowManager();

		IWindowHelper* GetWindowHelper(void);
		std::vector<uint32_t> GetWindowHandles(void);
		bool GetAvailableMonitorCount(uint32_t& monitorCount);
		bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode);
		bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes);
		bool WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title, uint32_t& windowHandle);
		bool WindowCreateWithSize(uint32_t width, uint32_t height, std::string title, uint32_t& windowHandle);
		bool GetWindowSize(uint32_t windowHandle, uint32_t& width, uint32_t& height);
		bool SetCursorMode(uint32_t windowHandle, uint32_t mode);
        bool WindowClose(uint32_t windowHandle);
		bool WindowPreRender(uint32_t& windowHandle, bool& exitRequested);
		bool WindowPostRender(uint32_t& windowHandle);
		void PollEvents(void);
		bool RenderLoop(AngelScriptRunner* angelScriptRunner, RenderStateManager* renderStateManager);
		bool GetKeyPressed(uint32_t windowHandle, uint32_t key, uint32_t& pressed);
		bool GetMouseButtonPressed(uint32_t windowHandle, uint32_t button, uint32_t& pressed);
		bool GetMouseCursorPosition(uint32_t windowHandle, double& xPos, double& yPos);
		bool SetMouseCursorPosition(uint32_t windowHandle, double xPos, double yPos);
		bool GetClipboardString(std::string& value);
		bool SetClipboardString(std::string value);

	private:

		IWindowHelper* m_windowHelper;
	};
}
