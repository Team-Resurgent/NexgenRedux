#pragma once

#include <string>

namespace NexgenRedux
{
	class ConfigLoader
	{		
	public:

		typedef struct ConfigData 
		{
			std::wstring MediaOverride;
			std::wstring LaunchFolder;
			bool Debug;
		} ConfigData;

		static bool LoadConfig();
		static bool MapPath(const std::wstring& path, std::wstring& mappedPath);
		static bool Debug();
	};
}
