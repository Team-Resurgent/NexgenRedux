#include "DebugUtility.h"
#include "StringUtility.h"
#include "FileSystem.h"
#include <fstream>
#include <vector>
#include <iostream>

#if defined UWP_ANGLE || defined WIN_ANGLE
#include <WinSock2.h>
#include <Windows.h>
#elif defined XBOX_360 || defined XBOX_OG
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

void DebugUtility::LogMessage(LogLevel const logLevel, std::wstring const format, ...)
{
	va_list va;
	va_start(va, format);	
	wchar_t buffer[1024];
	vswprintf(buffer, format.c_str(), va);
	va_end(va);

	std::wstring message = std::wstring(buffer);
		
	if (logLevel < MIN_LOGLEVEL)
	{
		return;
	}

	std::wstring level;
	if (logLevel == LOGLEVEL_INFO) {
		level = L"INFO";
	} else if (logLevel == LOGLEVEL_WARNING) {
		level = L"WARNING";
	} else {
		level = L"ERROR";
	}

	if (m_LogFile.length() > 0)
	{	
		std::fstream file;
		file.open(StringUtility::ToString(m_LogFile).c_str(), std::ios::out | std::fstream::app);
		file << StringUtility::ToString(level);
		file << ": ";
		file << StringUtility::ToString(message);
		file << "\n";
		file.close();
	}

	OutputDebugStringW(level.c_str());
	OutputDebugStringW(L": ");
	OutputDebugStringW(message.c_str());
	OutputDebugStringW(L"\n");

#ifdef WIN_ANGLE

	HANDLE stdOutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdOutHandle <= 0) {
		return;
	}

	DWORD dwCharsWritten;
	WriteConsole(stdOutHandle, level.c_str(), (DWORD)level.length(), &dwCharsWritten, NULL);
	WriteConsole(stdOutHandle, L": ", 2, &dwCharsWritten, NULL);
	WriteConsole(stdOutHandle, message.c_str(), (DWORD)message.length(), &dwCharsWritten, NULL);
	WriteConsole(stdOutHandle, L"\n", 1, &dwCharsWritten, NULL);

#endif
}

