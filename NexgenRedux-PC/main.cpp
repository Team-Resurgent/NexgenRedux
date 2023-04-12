#include <EntityEngine/FontManager.h>
#include <EntityEngine/NodeManager.h>
#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/Test.h>
#include <Gensys/NetworkManager.h>
#include <Gensys/SocketUtility.h>

#include "ConfigLoader.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "RenderStateManager.h"

#define MINIAUDIO_IMPLEMENTATION
   // #include "miniaudio.h"
#include <MinAudio/miniaudio.h>

using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

//static float lerp(float a, float b, float t) { return a + (b - a) * t; }

int main(int argc, const char* argv[])
{
    if (ConfigLoader::LoadConfig() == false) 
	{
		return 0;
	}

    NetworkManager::Init();

    std::wstring appPath;
	if (FileSystem::GetAppDirectory(appPath) == false)
    {
        return 0;
    }
    DebugUtility::Init(FileSystem::CombinePath(appPath, L"log.txt"), 17745);
	DebugUtility::DeleteLogFile();

    ma_result result;
    ma_engine engine;


    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize audio engine.");
        return -1;
    }


    std::wstring mediaPath;
	if (FileSystem::GetMediaDirectory(mediaPath) == false)
    {
        return 0;
    }
    result = ma_engine_play_sound(&engine, StringUtility::ToString(FileSystem::CombinePath(mediaPath, L"comic.mp3")).c_str(), NULL);
    //ma_engine_uninit(&engine);

    WindowManager *windowManager = WindowManager::GetInstance();
    RenderStateManager *renderStateManager = RenderStateManager::GetInstance();
    
    if (AngelScriptRunner::Init() == false)
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
    FontManager::Close();
    
    RenderStateManager::Close();
    WindowManager::Close();
    AngelScriptRunner::Close();
    DebugUtility::Close();
    NetworkManager::Close();

    return 0;
}