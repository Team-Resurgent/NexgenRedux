#include "Memory.h"

#if defined NEXGEN_WIN || defined NEXGEN_UWP
#include <WinSock2.h>
#include <Windows.h>
#elif defined NEXGEN_360 || defined NEXGEN_OG
#include <xtl.h>
#include <winsockx.h>
#endif	

using namespace Gensys;

unsigned long long Memory::GetTotalVirtualMemory()
{
#if defined NEXGEN_360 || defined NEXGEN_OG
	unsigned long long result;
	MEMORYSTATUS status;
	memset(&status, 0, sizeof(status));
	GlobalMemoryStatus(&status);
	result = (unsigned long long)status.dwTotalVirtual;
	return result;
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
	unsigned long long result;
	MEMORYSTATUSEX status;
	memset(&status, 0, sizeof(status));
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	result = (unsigned long long)status.ullTotalVirtual;
	return result;
#else
	return 0;
#endif
}

unsigned long long Memory::GetFreeVirtualMemory()
{
#if defined NEXGEN_360 || defined NEXGEN_OG
	unsigned long long result;
	MEMORYSTATUS status;
	memset(&status, 0, sizeof(status));
	GlobalMemoryStatus(&status);
	result = (unsigned long long)status.dwAvailVirtual;
	return result;
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
	unsigned long long result;
	MEMORYSTATUSEX status;
	memset(&status, 0, sizeof(status));
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	result = (unsigned long long)status.ullAvailVirtual;
	return result;
#else
	return 0;
#endif
}

unsigned long long Memory::GetTotalPhysicalMemory()
{
#if defined NEXGEN_360 || defined NEXGEN_OG
	unsigned long long result;
	MEMORYSTATUS status;
	memset(&status, 0, sizeof(status));
	GlobalMemoryStatus(&status);
	result = (unsigned long long)status.dwTotalPhys;
	return result;
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
	unsigned long long result;
	MEMORYSTATUSEX status;
	memset(&status, 0, sizeof(status));
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	result = (unsigned long long)status.ullTotalPhys;
	return result;
#else
	return 0;
#endif
}

unsigned long long Memory::GetFreePhysicalMemory()
{
#if defined NEXGEN_360 || defined NEXGEN_OG
	unsigned long long result;
	MEMORYSTATUS status;
	memset(&status, 0, sizeof(status));
	GlobalMemoryStatus(&status);
	result = (unsigned long long)status.dwAvailPhys;
	return result;
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
	unsigned long long result;
	MEMORYSTATUSEX status;
	memset(&status, 0, sizeof(status));
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	result = (unsigned long long)status.ullAvailPhys;
	return result;
#else
	return 0;
#endif
}