#include "ConfigLoader.h"
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

void __cdecl main(int, char **)
{
	//Gensys::Test::RunTests();

	ConfigLoader::ConfigData configData;
	memset(&configData, 0, sizeof(configData));
	if (ConfigLoader::LoadConfig(configData) == false) 
	{
		return;
	}

    if (AngelScriptRunner::Init(configData.LaunchFolder) == false)
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

    WindowManager::Close();
    AngelScriptRunner::Close();
}