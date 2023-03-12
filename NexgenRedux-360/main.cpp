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

void __cdecl main()
{
	//Gensys::Test::RunTests();

	BootLoader::Run();

    if (AngelScriptRunner::Init() == false)
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

