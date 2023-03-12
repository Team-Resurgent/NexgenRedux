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

    if (WindowManager::RenderLoop() == false)
    {
        return;
    }

	RenderStateManager::Close();
    WindowManager::Close();
    AngelScriptRunner::Close();
}