#pragma once

#include <string>

namespace NexgenRedux
{
	class ConfigLoader
	{		
	public:

		typedef struct ConfigData 
		{
			std::wstring Launch;
		} ConfigData;

		static bool LoadConfig(ConfigData *configData);
	private:
		static void ParseConfigLine(ConfigData* configData, std::string line);
	};
}
