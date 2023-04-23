#include "WindowManager.h"
#include "AngelScriptRunner.h"
#include "XboxOGWindowHelper.h"
#include "OpenGLWindowHelper.h"
#include "RenderStateManager.h"
#include "EntityEngine/NodeManager.h"
#include "TextureManager.h"
#include "AudioManager.h"
#include "MeshUtility.h"
#include "IWindowHelper.h"

#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>
#include <Gensys/TimeUtility.h>

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
#include <GLAD/glad.h>
#include <GLFW/glfw3.h>
#endif

#include <map>
#include <vector>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
	WindowManager* m_instance = NULL;
}

WindowManager::WindowManager()
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	m_windowHelper = new OpenGLWindowHelper();
#else
	m_windowHelper = new XboxOGWindowHelper();
#endif
}

WindowManager::~WindowManager()
{
	delete m_windowHelper;
}

WindowManager* WindowManager::GetInstance()
{
	if (m_instance == NULL) 
	{
    	m_instance = new WindowManager();
    }
    return m_instance;
}

void WindowManager::Close()
{
	delete m_instance;
	m_instance = NULL;
}

IWindowHelper* WindowManager::GetWindowHelper(void)
{
	return m_windowHelper;
}

bool WindowManager::GetAvailableMonitorCount(uint32_t& monitorCount)
{
	return m_windowHelper->GetAvailableMonitorCount(monitorCount);
}

bool WindowManager::GetMonitorVideoMode(uint32_t monitorIndex, MonitorVideoMode& monitorVideoMode)
{
	return m_windowHelper->GetMonitorVideoMode(monitorIndex, monitorVideoMode);
}

bool WindowManager::GetMonitorVideoModes(uint32_t monitorIndex, std::vector<MonitorVideoMode>& monitorVideoModes)
{
	return m_windowHelper->GetMonitorVideoModes(monitorIndex, monitorVideoModes);
}

bool WindowManager::WindowCreateWithVideoMode(MonitorVideoMode monitorVideoMode, std::string title)
{
	return m_windowHelper->WindowCreateWithVideoMode(monitorVideoMode, title);
}

bool WindowManager::WindowCreateWithSize(uint32_t width, uint32_t height, std::string title)
{
	return m_windowHelper->WindowCreateWithSize(width, height, title);
}

void* WindowManager::GetWindowPtr()
{
	return m_windowHelper->GetWindowPtr();
}

bool WindowManager::GetWindowSize(uint32_t& width, uint32_t& height)
{
	return m_windowHelper->GetWindowSize(width, height);
}

bool WindowManager::SetCursorMode(CursorMode mode)
{
	return m_windowHelper->SetCursorMode(mode);
}

bool WindowManager::WindowClose()
{
	return m_windowHelper->WindowClose();
}

bool WindowManager::WindowPreRender(bool& exitRequested)
{
	return m_windowHelper->WindowPreRender(exitRequested);
}

bool WindowManager::WindowPostRender()
{ 
	return m_windowHelper->WindowPostRender();
}

void WindowManager::PollEvents(void)
{
	m_windowHelper->PollEvents();
}

bool WindowManager::RenderLoop()
{
	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

	float f = 0;

	if (m_windowHelper->GetWindowPtr() != NULL) 
	{
		renderStateManager->Init();

		TextureManager::Init();

		uint64_t now = TimeUtility::GetMillisecondsNow();
		uint64_t previousNow = now;

		bool exitRequested = false;
		while (exitRequested == false)
		{
			double dt = TimeUtility::GetDurationSeconds(previousNow, now);
			if (WindowPreRender(exitRequested) == false)
			{
				DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowPreRender failed.");
				return false;
			}

			AudioManager::GetInstance()->Update();
			SceneManager::Update((float)dt);

			if (AngelScriptRunner::ExecuteUpdate(dt) == false)
			{
				DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "ExecuteUpdate failed.");
				return false;
			}

			SceneManager::Render();
			NodeManager::PurgeNodes();

			if (WindowPostRender() == false)
			{
				DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "WindowPostRender failed.");
				return false;
			}
			
			PollEvents();
			previousNow = now;
			now = TimeUtility::GetMillisecondsNow();
		}

		TextureManager::Close();
	}
    return true;
}

bool WindowManager::GetKeyPressed(uint32_t key, KeyButtonState& state)
{
	return m_windowHelper->GetKeyPressed(key, state);
}

bool WindowManager::GetMouseButtonPressed(uint32_t button, MouseButtonState& state)
{
	return m_windowHelper->GetMouseButtonPressed(button, state);
}

bool WindowManager::GetMouseCursorPosition(double& xPos, double& yPos)
{
	return m_windowHelper->GetMouseCursorPosition(xPos, yPos);
}

bool WindowManager::SetMouseCursorPosition(double xPos, double yPos)
{
	return m_windowHelper->SetMouseCursorPosition(xPos, yPos);
}

bool WindowManager::GetClipboardString(std::string& value)
{
	return m_windowHelper->GetClipboardString(value);
}

bool WindowManager::SetClipboardString(std::string value)
{
	return m_windowHelper->SetClipboardString(value);
}