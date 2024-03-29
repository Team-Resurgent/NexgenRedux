#include "ConfigLoader.h"
#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <string>
#include <vector>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
    ConfigLoader::ConfigData m_config = {};

    void ParseConfigLine(ConfigLoader::ConfigData& configData, std::string line)
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

        if (param1.compare(L"MEDIAOVERRIDE") == 0) {
            configData.MediaOverride = std::wstring(param2);
        } else if (param1.compare(L"LAUNCHFOLDER") == 0) {
            configData.LaunchFolder = std::wstring(param2);
        } else if (param1.compare(L"DEBUG") == 0) {
            configData.Debug = StringUtility::UpperCase(param2) == L"TRUE";
        }
    }
}

bool ConfigLoader::LoadConfig()
{
    m_config.MediaOverride = L"";
    m_config.LaunchFolder = L"Default";

	std::wstring mediaPath;
	if (FileSystem::GetMediaDirectory(mediaPath) == false)
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

bool ConfigLoader::MapPath(const std::wstring& path, std::wstring& mappedPath)
{
	std::wstring mediaDirectory;
    if (m_config.MediaOverride.length() > 0)
    {
        mediaDirectory = m_config.MediaOverride;
    }
    else if (FileSystem::GetMediaDirectory(mediaDirectory) == false)
    {
        return false;
    }

	if (StringUtility::StartsWith(path, L"skin:", true))
	{
		std::wstring skinPath = FileSystem::CombinePath(mediaDirectory, m_config.LaunchFolder);
		mappedPath = FileSystem::CombinePath(skinPath, path.substr(5));			
		return true;
	}
	if (StringUtility::StartsWith(path, L"script:", true))
	{
		std::wstring scriptPath = FileSystem::CombinePath(mediaDirectory, m_config.LaunchFolder);
		scriptPath = FileSystem::CombinePath(scriptPath, L"Scripts");
		mappedPath = FileSystem::CombinePath(scriptPath, path.substr(7));
		return true;
	}
	if (StringUtility::StartsWith(path, L"asset:", true))
	{
		std::wstring assetPath = FileSystem::CombinePath(mediaDirectory, m_config.LaunchFolder);		
		assetPath = FileSystem::CombinePath(assetPath, L"Asset");
		mappedPath = FileSystem::CombinePath(assetPath, path.substr(6));
		return true;
	}
	if (StringUtility::StartsWith(path, L"audio:", true))
	{
		std::wstring audioPath = FileSystem::CombinePath(mediaDirectory, m_config.LaunchFolder);		
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

bool ConfigLoader::Debug()
{
    return m_config.Debug;
}