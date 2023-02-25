#include "Drive.h"
#include "StringUtility.h"
#include <string>
#include "XboxInternals.h"

using namespace Gensys;

Drive::Drive(std::wstring mountPoint, std::wstring systemPath)
{
	m_mountPoint = mountPoint;
	m_systemPath = systemPath;
}

bool Drive::Mount()
{
	if (IsMounted()) {
		Unmount();
	}

#if defined NEXGEN_OG

	char mountPoint[260];
	sprintf(mountPoint, "\\??\\%s:", StringUtility::ToString(m_mountPoint).c_str());
	char systemPath[260];
	sprintf(systemPath,"%s", StringUtility::ToString(m_systemPath).c_str());
	STRING sMountPoint = { (USHORT)strlen(mountPoint), (USHORT)strlen(mountPoint) + 1, mountPoint };
	STRING sSystemPath = { (USHORT)strlen(systemPath), (USHORT)strlen(systemPath) + 1, systemPath };
	int result = IoCreateSymbolicLink(&sMountPoint, &sSystemPath);
	return result == 0;

#elif defined NEXGEN_360

	if (_wcsicmp(m_mountPoint.c_str(), L"Game") == 0)
	{
		return true;
	}

	char mountPoint[260];
	sprintf(mountPoint, "\\??\\%s:", StringUtility::ToString(m_mountPoint).c_str());
	char systemPath[260];
	sprintf(systemPath,"%s", StringUtility::ToString(m_systemPath).c_str());
	STRING sMountPoint = { (USHORT)strlen(mountPoint), (USHORT)strlen(mountPoint) + 1, mountPoint };
	STRING sSystemPath = { (USHORT)strlen(systemPath), (USHORT)strlen(systemPath) + 1, systemPath };
	int result = ObCreateSymbolicLink(&sMountPoint, &sSystemPath);
	return result == 0;

#else

	return true;

#endif
}

bool Drive::Unmount()
{
#if defined NEXGEN_OG

	char MountPoint[16];
	sprintf(MountPoint, "\\??\\%s:", StringUtility::ToString(m_mountPoint).c_str());
	STRING sMountPoint = {(USHORT)strlen(MountPoint), (USHORT)strlen(MountPoint) + 1, MountPoint};
	int result = IoDeleteSymbolicLink(&sMountPoint);
	return result == 0;

#elif defined NEXGEN_360

	if (_wcsicmp(m_mountPoint.c_str(), L"Game") == 0)
	{
		return true;
	}
	
	char MountPoint[16];
	sprintf(MountPoint, "\\??\\%s:", StringUtility::ToString(m_mountPoint).c_str());
	STRING sMountPoint = {(USHORT)strlen(MountPoint), (USHORT)strlen(MountPoint) + 1, MountPoint };
	int result = ObDeleteSymbolicLink(&sMountPoint);
	return result == 0;

#else

	return true;

#endif
}

bool Drive::IsMounted()
{	
	return GetVolumeSerialNumber() != 0 || GetTotalNumberOfBytes() != 0;
}

std::wstring Drive::GetMountPoint()
{
	return m_mountPoint;
}

std::wstring Drive::GetSystemPath()
{
	return m_systemPath;
}

uint64_t Drive::GetTotalNumberOfBytes()
{	
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_WIN

	std::wstring rootPath = m_mountPoint + L":\\";
	ULARGE_INTEGER totalNumberOfBytes;
	if (GetDiskFreeSpaceExA(StringUtility::ToString(rootPath).c_str(), NULL, &totalNumberOfBytes, NULL) == 0) 
	{
		return 0;
	}
	return (uint64_t)totalNumberOfBytes.QuadPart;

#else

	return 0;

#endif
}

uint64_t Drive::GetTotalFreeNumberOfBytes()
{	
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_WIN

	std::wstring rootPath = m_mountPoint + L":\\";
	ULARGE_INTEGER totalNumberOfFreeBytes;
	if (GetDiskFreeSpaceExA(StringUtility::ToString(rootPath).c_str(), NULL, NULL, &totalNumberOfFreeBytes) == 0) 
	{
		return 0;
	}
	return (uint64_t)totalNumberOfFreeBytes.QuadPart;

#else

	return 0;

#endif
}

uint32_t Drive::GetVolumeSerialNumber()
{	
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_WIN

	std::wstring rootPath = m_mountPoint + L":\\";
	DWORD volumeSerialNumber;
	if (GetVolumeInformationA(StringUtility::ToString(rootPath).c_str(), NULL, 0, &volumeSerialNumber, NULL, NULL, NULL, 0) == 0) 
	{
		return 0;
	}
	return (uint32_t)volumeSerialNumber;

#else

	return 0;

#endif
}
