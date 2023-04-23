#include "ConfigLoader.h"
#include "AngelScriptRunner.h"
#include "AudioManager.h"
#include "WindowManager.h"
#include "EntityEngine/FontManager.h"
#include "RenderStateManager.h"

#include <Gensys/NetworkManager.h>
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
	if (ConfigLoader::LoadConfig() == false) 
	{
		return;
	}

	AudioManager::GetInstance()->Init();
    NetworkManager::Init();

    std::wstring appPath;
	if (FileSystem::GetAppDirectory(appPath) == false)
    {
        return;
    }
    DebugUtility::Init(FileSystem::CombinePath(appPath, L"log.txt"), 17745);
	DebugUtility::DeleteLogFile();

    WindowManager *windowManager = WindowManager::GetInstance();
    RenderStateManager *renderStateManager = RenderStateManager::GetInstance();
    
    if (AngelScriptRunner::Init() == false)
    {
        return;
    }

    if (AngelScriptRunner::ExecuteInit() == false)
    {
        return;
    }

    if (windowManager->RenderLoop() == false)
    {
        return;
    }

    NodeManager::Close();
    FontManager::Close();
    
    RenderStateManager::Close();
    WindowManager::Close();
    AngelScriptRunner::Close();
    DebugUtility::Close();
    NetworkManager::Close();
	AudioManager::GetInstance()->Close();
}