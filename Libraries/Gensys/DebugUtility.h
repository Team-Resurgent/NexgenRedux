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

		static bool Init(std::wstring const logFile, uint16_t port);
		static void Close();
		static void DeleteLogFile();

		static void LogMessage(const LogLevel logLevel, std::string const message);
		static void LogMessageIf(const bool condition, const LogLevel logLevel, const std::string message);
	};
}