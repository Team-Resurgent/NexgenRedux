#pragma once

#include <string>

namespace NexgenRedux
{
	class ConfigLoader
	{		
	public:

		typedef struct ConfigData 
		{
			std::wstring LaunchFolder;
		} ConfigData;

		static bool LoadConfig();
		static std::wstring GetLaunchFolder();
		static bool MapPath(const std::wstring& path, std::wstring& mappedPath);
	};
}
