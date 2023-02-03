#pragma once

#include <string>

namespace NexgenRedux
{
	typedef struct ConfigData 
	{
		std::wstring Launch;
	} ConfigData;

	class ConfigLoader
	{		
	public:
		bool LoadConfig(ConfigData *configData);
	private:
		void ParseConfigLine(ConfigData* configData, std::string line);
	};
}
