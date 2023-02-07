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
	RefreshDetails();
	return m_volumeSerialNumber != 0 || m_totalNumberOfBytes > 0;
}

std::wstring Drive::GetMountPoint()
{
	return m_mountPoint;
}

std::wstring Drive::GetSystemPath()
{
	return m_systemPath;
}

void Drive::RefreshDetails()
{
#ifdef NEXGEN_OG
	std::wstring rootPath = m_mountPoint + L":\\";

	ULARGE_INTEGER freeBytesAvailable;
	ULARGE_INTEGER totalNumberOfBytes;
	ULARGE_INTEGER totalNumberOfFreeBytes;
	if (GetDiskFreeSpaceExA(StringUtility::ToString(rootPath).c_str(), &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
		m_totalNumberOfBytes = (long)totalNumberOfBytes.QuadPart;
		m_freeNumberOfBytes = (long)totalNumberOfFreeBytes.QuadPart;
	} else {
		m_totalNumberOfBytes = 0;
		m_freeNumberOfBytes = 0;
	}

	DWORD volumeSerialNumber;
	DWORD maximumComponentLength;
	DWORD fileSystemFlags;
	if (GetVolumeInformationA(StringUtility::ToString(rootPath).c_str(), NULL, 0, &volumeSerialNumber, &maximumComponentLength, &fileSystemFlags, NULL, 0) == 1) {
		m_volumeSerialNumber = volumeSerialNumber;
	} else {
		m_volumeSerialNumber = 0;
	}
#endif
}

long Drive::GetTotalNumberOfBytes()
{	
	RefreshDetails();
	return m_totalNumberOfBytes; 
}

long Drive::GetFreeNumberOfBytes()
{	
	RefreshDetails();
	return m_freeNumberOfBytes; 
}

long Drive::GetVolumeSerialNumber()
{	
	RefreshDetails();
	return m_volumeSerialNumber; 
}
	
