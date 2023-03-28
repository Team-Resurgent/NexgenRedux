#include "GLAD/glad.h"
#include "GLFW/glfw3.h"

#include "Icon.h"
#include <array>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

#include <RenderDoc/renderdoc_app.h>>

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/Test.h>

#include "ConfigLoader.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "RenderStateManager.h"

#include "ECSManager.h"

#include "Windows.h"

using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

//static float lerp(float a, float b, float t) { return a + (b - a) * t; }

int main(int argc, const char* argv[])
{
//     RENDERDOC_API_1_1_2 *rdoc_api = NULL;
//     HMODULE mod = GetModuleHandleA("C:\\Program Files\\RenderDoc\\renderdoc.dll");
//     if(mod)
//     {
//         pRENDERDOC_GetAPI RENDERDOC_GetAPI =
//             (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
//         int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
//     }
//     if(rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);

	//Gensys::Test::RunTests();

    WindowManager *windowManager = WindowManager::GetInstance();
    RenderStateManager *renderStateManager = RenderStateManager::GetInstance();

    //ECSManager::ECSManagerExample();

	if (ConfigLoader::LoadConfig() == false) 
	{
		return 0;
	}

    if (AngelScriptRunner::Init(ConfigLoader::GetLaunchFolder()) == false)
    {
        return 0;
    }

    if (AngelScriptRunner::ExecuteInit() == false)
    {
        return 0;
    }

    if (windowManager->RenderLoop() == false)
    {
        return 0;
    }

    NodeManager::Close();

    RenderStateManager::Close();
    WindowManager::Close();
    AngelScriptRunner::Close();

    return 0;
}