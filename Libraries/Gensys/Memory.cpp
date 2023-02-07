#include "Memory.h"

#if defined UWP_ANGLE || defined NEXGEN_WIN
#include <WinSock2.h>
#include <Windows.h>
#elif defined NEXGEN_360 || defined NEXGEN_OG
#include <xtl.h>
#include <winsockx.h>
#endif	

using namespace Gensys;

unsigned long long Memory::GetTotalVirtualMemory()
{
	unsigned long long result;
#if defined NEXGEN_360 || defined NEXGEN_OG
	MEMORYSTATUS status;
	memset(&status, 0, sizeof(status));
	GlobalMemoryStatus(&status);
	result = (unsigned long long)status.dwTotalVirtual;
#elif defined UWP_ANGLE || defined NEXGEN_WIN
	MEMORYSTATUSEX status;
	memset(&status, 0, sizeof(status));
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	result = (unsigned long long)status.ullTotalVirtual;
#endif
	return result;
}

unsigned long long Memory::GetFreeVirtualMemory()
{
	unsigned long long result;
#if defined NEXGEN_360 || defined NEXGEN_OG
	MEMORYSTATUS status;
	memset(&status, 0, sizeof(status));
	GlobalMemoryStatus(&status);
	result = (unsigned long long)status.dwAvailVirtual;
#elif defined UWP_ANGLE || defined NEXGEN_WIN
	MEMORYSTATUSEX status;
	memset(&status, 0, sizeof(status));
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	result = (unsigned long long)status.ullAvailVirtual;
#endif
	return result;
}

unsigned long long Memory::GetTotalPhysicalMemory()
{
	unsigned long long result;
#if defined NEXGEN_360 || defined NEXGEN_OG
	MEMORYSTATUS status;
	memset(&status, 0, sizeof(status));
	GlobalMemoryStatus(&status);
	result = (unsigned long long)status.dwTotalPhys;
#elif defined UWP_ANGLE || defined NEXGEN_WIN
	MEMORYSTATUSEX status;
	memset(&status, 0, sizeof(status));
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	result = (unsigned long long)status.ullTotalPhys;
#endif
	return result;
}

unsigned long long Memory::GetFreePhysicalMemory()
{
	unsigned long long result;
#if defined NEXGEN_360 || defined NEXGEN_OG
	MEMORYSTATUS status;
	memset(&status, 0, sizeof(status));
	GlobalMemoryStatus(&status);
	result = (unsigned long long)status.dwAvailPhys;
#elif defined UWP_ANGLE || defined NEXGEN_WIN
	MEMORYSTATUSEX status;
	memset(&status, 0, sizeof(status));
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	result = (unsigned long long)status.ullAvailPhys;
#endif
	return result;
}