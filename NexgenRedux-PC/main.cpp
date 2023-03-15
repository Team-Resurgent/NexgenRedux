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

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/Test.h>

#include "ConfigLoader.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "RenderStateManager.h"

#include "ECSManager.h"

using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

//static float lerp(float a, float b, float t) { return a + (b - a) * t; }

int main(int argc, const char* argv[])
{
	//Gensys::Test::RunTests();

    RenderStateManager *renderStateManager = new RenderStateManager();
    WindowManager *windowManager = new WindowManager(renderStateManager);
    AngelScriptRunner *angelScriptRunner = new AngelScriptRunner(windowManager);

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

    if (windowManager->RenderLoop(angelScriptRunner) == false)
    {
        return 0;
    }

    delete renderStateManager;
    delete windowManager;
    delete angelScriptRunner;

    return 0;
}