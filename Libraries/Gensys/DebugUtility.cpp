#include "DebugUtility.h"
#include "StringUtility.h"
#include "FileSystem.h"
#include "SocketUtility.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <stdarg.h>
#include <cstring>

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
	SocketUtility* m_SocketUtility = NULL;
	uint16_t m_port = 17745;
}

#define MIN_LOGLEVEL LOGLEVEL_INFO

bool DebugUtility::Init(std::wstring const logFile, uint16_t port)
{
	m_LogFile = logFile;
	m_port = port;

	m_SocketUtility = new SocketUtility();
	if (m_SocketUtility->Create(IPPROTO_UDP) == false)
	{
		Close();
		return false;
	}
    if (m_SocketUtility->EnableBroadcast() == false)
	{
		Close();
		return false;
	}
	if (m_SocketUtility->Bind(0) == false)
	{
		Close();
		return false;
	}
	return true;
}

void DebugUtility::Close()
{
	if (m_SocketUtility != NULL)
	{
		delete m_SocketUtility;
		m_SocketUtility = NULL;
	}
}

void DebugUtility::DeleteLogFile()
{
    FileSystem::FileDelete(m_LogFile);
}

void DebugUtility::LogMessage(DebugUtility::LogLevel const logLevel, const std::string message)
{
	if (logLevel < DebugUtility::MIN_LOGLEVEL)
	{
		return;
	}

	std::string level;
	if (logLevel == DebugUtility::LOGLEVEL_INFO) {
		level = "INFO";
	} else if (logLevel == DebugUtility::LOGLEVEL_WARNING) {
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

	if (m_SocketUtility != NULL)
	{
		sockaddr_in remoteAddr;
		memset(&remoteAddr, 0, sizeof(remoteAddr));
		remoteAddr.sin_family = AF_INET;
		remoteAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
		remoteAddr.sin_port = htons(m_port);
		m_SocketUtility->SendUDP(message.c_str(), (uint32_t)message.length(), remoteAddr);
	}
}

void DebugUtility::LogMessageIf(const bool condition, const LogLevel logLevel, const std::string message)
{
	if (condition == false)
	{
		return;
	}
	LogMessage(logLevel, message);
}
