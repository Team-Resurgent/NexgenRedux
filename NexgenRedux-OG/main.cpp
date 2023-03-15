#include "ConfigLoader.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "RenderStateManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/Test.h>

#include <xtl.h>


using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

void __cdecl main(int, char **)
{
	//Gensys::Test::RunTests();

    RenderStateManager *renderStateManager = new RenderStateManager();
    WindowManager *windowManager = new WindowManager(renderStateManager);
    AngelScriptRunner *angelScriptRunner = new AngelScriptRunner(windowManager);

    //ECSManager::ECSManagerExample();

	if (ConfigLoader::LoadConfig() == false) 
	{
		return;
	}

    if (AngelScriptRunner::Init(ConfigLoader::GetLaunchFolder()) == false)
    {
        return;
    }

    if (AngelScriptRunner::ExecuteInit() == false)
    {
        return;
    }

    if (windowManager->RenderLoop(angelScriptRunner) == false)
    {
        return;
    }

    delete renderStateManager;
    delete windowManager;
    delete angelScriptRunner;
}