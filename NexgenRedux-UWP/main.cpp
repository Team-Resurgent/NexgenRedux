#include "BootLoader.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/Test.h>

#include <Windows.h>
#include <winrt/base.h>

using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

bool UwpInit()
{
	std::wstring mediaDirectory;
	if (FileSystem::GetMediaDirectory(mediaDirectory) == false)
	{
		return false;
	}

	std::wstring appDirectory;
	if (FileSystem::GetAppDirectory(appDirectory) == false)
	{
		return false;
	}

	DebugUtility::SetLogFile(FileSystem::CombinePath(mediaDirectory, L"logFile.txt"));
	DebugUtility::DeleteLogFile();

	bool exists = false;
	if (!FileSystem::DirectoryExists(mediaDirectory, exists))
	{
		return false;
	}

	if (exists)
	{
		if (!FileSystem::DirectoryDelete(mediaDirectory, true))
		{
			return false;
		}
	}

	std::wstring sourcePath = FileSystem::CombinePath(FileSystem::CombinePath(appDirectory, L"Assets"), L"Media");
	if (FileSystem::DirectoryCopy(sourcePath, mediaDirectory) == false)
	{
		return false;
	}
}

//https://github.com/Justin-Credible/xbox-uwp-sdl2-starter/blob/master/src/xbox-uwp-sdl2-starter/Program.cs

int main(Platform::Array<Platform::String^>^ args)
{
	if (UwpInit() == false)
	{
		return 0;
	}

	Gensys::Test::RunTests();

//
//    //BootLoader::Run();
//
//    //if (AngelScriptRunner::Init() == false)
//    //{
//    //    return;
//    //}
//    //if (AngelScriptRunner::ExecuteInit() == false)
//    //{
//    //    return;
//    //}
//
//    //if (WindowManager::RenderLoop() == false)
//    //{
//    //    return;
//    //}
//
//    //WindowManager::Close();
//    //AngelScriptRunner::Close();
    return 0;
}

//
//int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,  _In_ int nCmdShow)
//{
//    //Gensys::Test::RunTests();
//
//    //BootLoader::Run();
//
//    //if (AngelScriptRunner::Init() == false)
//    //{
//    //    return;
//    //}
//    //if (AngelScriptRunner::ExecuteInit() == false)
//    //{
//    //    return;
//    //}
//
//    //if (WindowManager::RenderLoop() == false)
//    //{
//    //    return;
//    //}
//
//    //WindowManager::Close();
//    //AngelScriptRunner::Close();
//    return 0;
//}

