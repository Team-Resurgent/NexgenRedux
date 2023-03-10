#include "ConfigLoader.h"
#include <Gensys/Int.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <string>
#include <vector>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{

    typedef struct ConfigData 
    {
        std::wstring LaunchFolder;
    } ConfigData;

    ConfigData m_config = {};

    void ParseConfigLine(ConfigData& configData, std::string line)
    {
        std::string params[2];
        params[0] = "";
        params[1] = "";

        uint8_t paramIndex = 0;
        for (uint32_t i = 0; i < line.length(); i++) {
            char currentChar = line[i];
            if (paramIndex > 1) {
                return;
            }
            else if (currentChar == '=') {
                paramIndex++;
                continue;
            }
            params[paramIndex] += currentChar;
        }
    
        if (paramIndex != 1) {
            return;
        }

        std::wstring param1 = StringUtility::ToWideString(params[0]);
        param1 = StringUtility::Trim(param1, ' ');
        param1 = StringUtility::UpperCase(param1);

        std::wstring param2 = StringUtility::ToWideString(params[1]);
        param2 = StringUtility::Trim(param2, ' ');

        if (param1.compare(L"LAUNCHFOLDER") == 0) {
            configData.LaunchFolder = std::wstring(param2);
        }
    }
}

bool ConfigLoader::LoadConfig()
{
	std::wstring mediaPath;
	if (!FileSystem::GetMediaDirectory(mediaPath))
	{
		return false;
	}

	const std::wstring configPath = FileSystem::CombinePath(mediaPath, L"config.ini");

	std::string buffer;
	if (!FileSystem::FileReadAllAsString(configPath, &buffer))
	{
		return false;
	}

    std::string line = "";
	bool skip = false;

    for (uint32_t i = 0; i < buffer.length(); i++) {
        char currentChar = buffer[i];
        if (currentChar == '\t') {
            continue;
        }
		if (currentChar == ';') {
			skip = true;
            continue;
        }
        if (currentChar == '\r' || currentChar == '\n') {
			skip = false;
            if (line.length() > 0) {
                ParseConfigLine(m_config, line);
                line = "";
            }
            continue;
        }
		else if (skip == false) 
		{
			line += currentChar;
		}
    }

    if (line.length() > 0) {
        ParseConfigLine(m_config, line);
    }

	return true;
}

std::wstring ConfigLoader::GetLaunchFolder()
{
    return m_config.LaunchFolder;
}

bool ConfigLoader::MapPath(const std::wstring& path, std::wstring& mappedPath)
{
	std::wstring mediaDirectory;
    if (FileSystem::GetMediaDirectory(mediaDirectory) == false)
    {
        return false;
    }

	if (StringUtility::StartsWith(path, L"skin:", true))
	{
		std::wstring skinPath = FileSystem::CombinePath(mediaDirectory, GetLaunchFolder());
		mappedPath = FileSystem::CombinePath(skinPath, path.substr(5));			
		return true;
	}
	if (StringUtility::StartsWith(path, L"script:", true))
	{
		std::wstring scriptPath = FileSystem::CombinePath(mediaDirectory, GetLaunchFolder());
		scriptPath = FileSystem::CombinePath(scriptPath, L"Scripts");
		mappedPath = FileSystem::CombinePath(scriptPath, path.substr(8));
		return true;
	}
	if (StringUtility::StartsWith(path, L"asset:", true))
	{
		std::wstring assetPath = FileSystem::CombinePath(mediaDirectory, GetLaunchFolder());		
		assetPath = FileSystem::CombinePath(assetPath, L"Asset");
		mappedPath = FileSystem::CombinePath(assetPath, path.substr(7));
		return true;
	}
	if (StringUtility::StartsWith(path, L"audio:", true))
	{
		std::wstring audioPath = FileSystem::CombinePath(mediaDirectory, GetLaunchFolder());		
		audioPath = FileSystem::CombinePath(audioPath, L"Audio");
		mappedPath = FileSystem::CombinePath(audioPath, path.substr(6));
		return true;
	}
	if (StringUtility::StartsWith(path, L"global:", true))
	{
		std::wstring globalPath = FileSystem::CombinePath(mediaDirectory, L"Global");
		mappedPath = FileSystem::CombinePath(globalPath, path.substr(7));
		return true;
	}
    mappedPath = path;
	return true;
}