#include "BootLoader.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/Test.h>

#include <xtl.h>


using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

void PrintString(std::string &str);

void __cdecl main(int, char **)
{
	//Gensys::Test::RunTests();

	//BootLoader::Run();

    //AngelScriptRunner::Init();
    //AngelScriptRunner::ExecuteCalc();
    //AngelScriptRunner::Close();

    uint32_t monitorCount;
    if (WindowManager::GetAvailableMonitorCount(monitorCount) == false)
    {
        return;
    }

    std::vector<WindowManager::MonitorVideoMode> videoModes;
    if (WindowManager::GetMonitorVideoModes(0, videoModes) == false)
    {
        return;
    }

    WindowManager::MonitorVideoMode videoMode;
    if (WindowManager::GetMonitorVideoMode(0, videoMode) == false)
    {
        return;
    }

    uint32_t windowHandle1;
    if (WindowManager::WindowCreate(640, 480, "EqUiNoX was here...", windowHandle1) == false)
    {
        return;
    }

    //uint32_t windowHandle2;
    //if (WindowManager::WindowCreate(videoMode, "EqUiNoX was here again...", windowHandle2) == false)
    //{
    //     return;
    //}

    if (WindowManager::RenderLoop() == false)
    {
        return;
    }

    if (WindowManager::WindowClose(windowHandle1) == false)
    {
        return;
    }

	return;
}