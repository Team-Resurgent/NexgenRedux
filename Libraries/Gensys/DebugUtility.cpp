#include "DebugUtility.h"
#include "StringUtility.h"
#include "FileSystem.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <stdarg.h>

#if defined UWP_ANGLE || defined NEXGEN_WIN
#include <WinSock2.h>
#include <Windows.h>
#elif defined NEXGEN_360 || defined NEXGEN_OG
#include <xtl.h>
#include <winsockx.h>
#endif

using namespace Gensys;

namespace {

	std::wstring m_LogFile = L"logFile.log";

}

#define MIN_LOGLEVEL LOGLEVEL_INFO

void DebugUtility::SetLogFile(std::wstring const logFile)
{
	m_LogFile = logFile;
}

void DebugUtility::DeleteLogFile()
{
    FileSystem::FileDelete(m_LogFile);
}

void DebugUtility::LogMessage(LogLevel const logLevel, std::string const format, ...)
{
	va_list args;
	va_start(args, format);	
	char buffer[1024];
	vsprintf(buffer, format.c_str(), args);	
	va_end(args);

	std::string message = std::string(buffer);
		
	if (logLevel < MIN_LOGLEVEL)
	{
		return;
	}

	std::string level;
	if (logLevel == LOGLEVEL_INFO) {
		level = "INFO";
	} else if (logLevel == LOGLEVEL_WARNING) {
		level = "WARNING";
	} else {
		level = "ERROR";
	}

	if (m_LogFile.length() > 0)
	{	
		std::fstream file;
		file.open(StringUtility::ToString(m_LogFile).c_str(), std::ios::out | std::fstream::app);
		file << level;
		file << ": ";
		file << message;
		file << "\n";
		file.close();
	}

#if defined NEXGEN_OG || NEXGEN_360 || NEXGEN_WIN
	OutputDebugStringA(level.c_str());
	OutputDebugStringA(": ");
	OutputDebugStringA(message.c_str());
	OutputDebugStringA("\n");
#endif 

#if defined NEXGEN_WIN || NEXGEN_MAC || NEXGEN_LINUX
	printf("%s", level.c_str());
	printf(": ");
	printf("%s", message.c_str());
	printf("\n");
#endif

// #ifdef NEXGEN_WIN

// 	HANDLE stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
// 	if (stdOutHandle <= 0) {
// 		return;
// 	}

// 	DWORD dwCharsWritten;
// 	WriteConsole(stdOutHandle, level.c_str(), (DWORD)level.length(), &dwCharsWritten, NULL);
// 	WriteConsole(stdOutHandle, L": ", 2, &dwCharsWritten, NULL);
// 	WriteConsole(stdOutHandle, message.c_str(), (DWORD)message.length(), &dwCharsWritten, NULL);
// 	WriteConsole(stdOutHandle, L"\n", 1, &dwCharsWritten, NULL);

// #endif
}

