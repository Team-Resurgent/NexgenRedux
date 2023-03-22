#pragma once

#include <string>

namespace Gensys
{
	class DebugUtility
	{		
	public:

		typedef enum LogLevel
		{   
			LOGLEVEL_INFO = 0, 
			LOGLEVEL_WARNING = 1,
			LOGLEVEL_ERROR = 2
		} LogLevel;

		static void DeleteLogFile();
		static void SetLogFile(std::wstring const logFile);

		static void LogMessage(const LogLevel logLevel, std::string const message);
		static void LogMessageIf(const bool condition, const LogLevel logLevel, const std::string message);
	};
}