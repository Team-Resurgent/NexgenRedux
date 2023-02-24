#include "ConfigLoader.h"
#include <Gensys/Int.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <string>
#include <vector>

using namespace Gensys;
using namespace NexgenRedux;

bool ConfigLoader::LoadConfig(ConfigData *configData)
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
                ParseConfigLine(configData, line);
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
        ParseConfigLine(configData, line);
    }

	return true;
}


void ConfigLoader::ParseConfigLine(ConfigData* configData, std::string line)
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

    if (param1.compare(L"LAUNCH") == 0) {
		configData->Launch = param2;
    }
}

