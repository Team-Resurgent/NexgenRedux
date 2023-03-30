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

		static WindowManager* GetInstance();
		static void Close();

		IWindowHelper* GetWindowHelper(void);
		bool GetAvailableMonitorCount(uint32_t& monitorCount);
		bool GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode);
		bool GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes);
		bool WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title);
		bool WindowCreateWithSize(uint32_t width, uint32_t height, std::string title);
		void* GetWindowPtr();
		bool GetWindowSize(uint32_t& width, uint32_t& height);
		bool SetCursorMode(uint32_t mode);
        bool WindowClose();
		bool WindowPreRender(bool& exitRequested);
		bool WindowPostRender();
		void PollEvents(void);
		bool RenderLoop();
		bool GetKeyPressed(uint32_t key, uint32_t& pressed);
		bool GetMouseButtonPressed(uint32_t button, uint32_t& pressed);
		bool GetMouseCursorPosition(double& xPos, double& yPos);
		bool SetMouseCursorPosition(double xPos, double yPos);
		bool GetClipboardString(std::string& value);
		bool SetClipboardString(std::string value);
	
	private:

		WindowManager();
		~WindowManager();

	private:

		IWindowHelper* m_windowHelper;
	};
}
