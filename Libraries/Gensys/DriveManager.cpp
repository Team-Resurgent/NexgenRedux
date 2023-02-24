#include "DriveManager.h"
#include "FileSystem.h"
#include "Drive.h"
#include "StringUtility.h"

using namespace Gensys;

namespace
{
	bool m_initialized;
	std::vector<Drive> m_drives;
}

void DriveManager::Init()
{
	if (m_initialized == true) 
	{
		return;
	}
	m_initialized = true;

#if defined NEXGEN_OG

	m_drives.push_back(Drive(L"C", L"\\Device\\Harddisk0\\Partition2"));
	m_drives.push_back(Drive(L"D", L"\\Device\\Cdrom0"));
	m_drives.push_back(Drive(L"E", L"\\Device\\Harddisk0\\Partition1"));
	m_drives.push_back(Drive(L"F", L"\\Device\\Harddisk0\\Partition6"));
	m_drives.push_back(Drive(L"G", L"\\Device\\Harddisk0\\Partition7"));
	m_drives.push_back(Drive(L"X", L"\\Device\\Harddisk0\\Partition3"));
	m_drives.push_back(Drive(L"Y", L"\\Device\\Harddisk0\\Partition4"));
	m_drives.push_back(Drive(L"Z", L"\\Device\\Harddisk0\\Partition5"));

	m_drives.push_back(Drive(L"R", L"\\Device\\Harddisk0\\Partition8"));
	m_drives.push_back(Drive(L"S", L"\\Device\\Harddisk0\\Partition9"));
	m_drives.push_back(Drive(L"V", L"\\Device\\Harddisk0\\Partition10"));
	m_drives.push_back(Drive(L"W", L"\\Device\\Harddisk0\\Partition11"));
	m_drives.push_back(Drive(L"A", L"\\Device\\Harddisk0\\Partition12"));
	m_drives.push_back(Drive(L"B", L"\\Device\\Harddisk0\\Partition13"));

	m_drives.push_back(Drive(L"H", L"\\Device\\MU0"));
	m_drives.push_back(Drive(L"I", L"\\Device\\MU1"));
	m_drives.push_back(Drive(L"J", L"\\Device\\MU2"));
	m_drives.push_back(Drive(L"K", L"\\Device\\MU3"));
	m_drives.push_back(Drive(L"L", L"\\Device\\MU4"));
	m_drives.push_back(Drive(L"M", L"\\Device\\MU5"));
	m_drives.push_back(Drive(L"N", L"\\Device\\MU6"));
	m_drives.push_back(Drive(L"O", L"\\Device\\MU7"));

	for (size_t i = 0; i < m_drives.size(); i++)
    {
		m_drives.at(i).Mount();
    }

#elif defined NEXGEN_360

	m_drives.push_back(Drive(L"Game", L"\\Device\\Game"));
	m_drives.push_back(Drive(L"Flash", L"\\Device\\Flash"));
	m_drives.push_back(Drive(L"Memunit0", L"\\Device\\Mu0"));
	m_drives.push_back(Drive(L"Memunit1", L"\\Device\\Mu1"));
	m_drives.push_back(Drive(L"OnBoardMU", L"\\Device\\BuiltInMuSfc"));
	m_drives.push_back(Drive(L"Dvd", L"\\Device\\Cdrom0"));
	m_drives.push_back(Drive(L"Hdd1", L"\\Device\\Harddisk0\\Partition1"));
	m_drives.push_back(Drive(L"Hdd0", L"\\Device\\Harddisk0\\Partition0"));
	m_drives.push_back(Drive(L"HddX", L"\\Device\\Harddisk0\\SystemPartition"));
	m_drives.push_back(Drive(L"SysExt", L"\\Device\\Harddisk0\\SystemExtPartition"));
	m_drives.push_back(Drive(L"Usb0", L"\\Device\\Mass0"));
	m_drives.push_back(Drive(L"Usb1", L"\\Device\\Mass1"));
	m_drives.push_back(Drive(L"Usb2", L"\\Device\\Mass2"));
	m_drives.push_back(Drive(L"HdDvdPlayer", L"\\Device\\HdDvdPlayer"));
	m_drives.push_back(Drive(L"HdDvdStorage", L"\\Device\\HdDvdStorage"));
	m_drives.push_back(Drive(L"Transfercable", L"\\Device\\Transfercable"));
	m_drives.push_back(Drive(L"TransfercableXbox1", L"\\Device\\Transfercable\\Compatibility\\Xbox1"));
	m_drives.push_back(Drive(L"USBMU0", L"\\Device\\Mass0PartitionFile\\Storage"));
	m_drives.push_back(Drive(L"USBMU1", L"\\Device\\Mass1PartitionFile\\Storage"));
	m_drives.push_back(Drive(L"USBMU2", L"\\Device\\Mass2PartitionFile\\Storage"));
	m_drives.push_back(Drive(L"USBMUCache0", L"\\Device\\Mass0PartitionFile\\StorageSystem"));
	m_drives.push_back(Drive(L"USBMUCache1", L"\\Device\\Mass1PartitionFile\\StorageSystem"));
	m_drives.push_back(Drive(L"USBMUCache2", L"\\Device\\Mass2PartitionFile\\StorageSystem"));

	for (size_t i = 0; i < m_drives.size(); i++)
    {
		m_drives.at(i)->Mount();
    }
//#else
//
//	std::vector<std::wstring> driveLetters;
//	GetMountedDrives(driveLetters);
//    for (int i = 0; i < driveLetters.size(); i++)
//	{
//        std::wstring driveLetter = driveLetters[i];
//		m_drives.push_back(new Drive(driveLetter, driveLetter));
//	}
	
#endif
}

void DriveManager::Refresh()
{
	Init();

	for (size_t i = 0; i < m_drives.size(); i++)
    {
		Drive* currentDrive = &m_drives.at(i);
		if (!currentDrive->IsMounted()) {
			currentDrive->Mount();
		}
    }
}

std::vector<std::wstring> DriveManager::GetAllMountPoints()
{
	Init();

	std::vector<std::wstring> mountPoints;
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		mountPoints.push_back(currentDrive->GetMountPoint());		
    }
	return mountPoints;
}

std::vector<std::wstring> DriveManager::GetAllSystemPaths()
{
	Init();

	std::vector<std::wstring> systemPaths;
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		systemPaths.push_back(currentDrive->GetSystemPath());		
    }
	return systemPaths;
}

std::wstring DriveManager::GetSystemPath(std::wstring mountPoint)
{
	Init();

	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return currentDrive->GetSystemPath();
		}
	}
	return L"";
}

std::wstring DriveManager::GetMountPoint(std::wstring systemPath)
{
	Init();

	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		if (currentDrive->GetSystemPath() == systemPath) {
			return currentDrive->GetMountPoint();
		}
	}
	return L"";
}

bool DriveManager::IsMounted(std::wstring mountPoint)
{
	Init();

#if defined UWP_ANGLE || defined NEXGEN_WIN
	return true;
#else	
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return currentDrive->IsMounted();
		}
	}
	return false;
#endif
}

long DriveManager::GetTotalNumberOfBytes(std::wstring mountPoint)
{
	Init();

	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return (long)currentDrive->GetTotalNumberOfBytes();
		}
	}
	return 0;
}

long DriveManager::GetFreeNumberOfBytes(std::wstring mountPoint)
{
	Init();

	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return (long)currentDrive->GetFreeNumberOfBytes();
		}
	}
	return 0;
}

long DriveManager::GetVolumeSerialNumber(std::wstring mountPoint)
{
	Init();

	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return currentDrive->GetVolumeSerialNumber();
		}
	}
	return 0;
}

bool DriveManager::GetMountedDrives(std::vector<std::wstring>& drives)
{
	Init();

	drives.clear();

#if defined NEXGEN_OG || defined NEXGEN_360

	return false;
	
#elif defined NEXGEN_WIN

	char buffer[2];
	buffer[1] = 0;
	
	DWORD driveFlags = GetLogicalDrives();
	for (char i = 0; i < 26; i++)
	{
		const char letterA = 65;
		char driveLetter = letterA + i;
		DWORD testFlag = 1 << i;
		if ((driveFlags & testFlag) > 0)
		{
			buffer[0] = driveLetter;
			drives.push_back(StringUtility::ToWideString(std::string(buffer)));
		}
	}
	return true;

#elif defined NEXGEN_MAC

	std::vector<FileInfoDetail> fileInfoDetails;
	if (!FileGetFileInfoDetails(L"/Volumes", fileInfoDetails))
	{
		return false;
	}
	for (uint32_t i = 0; i < fileInfoDetails.size(); i++)
	{
		if (fileInfoDetails.at(i).name == L"." || fileInfoDetails.at(i).name == L"..") 
		{
			continue;
		}
		drives.push_back(CombinePath(L"/Volumes", fileInfoDetails.at(i).name));
	}
	return true;

#elif defined NEXGEN_LINUX

  	FILE *file = setmntent("/etc/mtab", "r");
    if (file == NULL) 
	{
        return false;
    }

	const std::string filters[] = { "/dev/sd", "/dev/hd", "/dev/fd", "/dev/sr", "/dev/scd", "/dev/mmcblk"};

	struct mntent *entry = getmntent(file);
    while (entry != NULL) 
	{
		std::string name = std::string(entry->mnt_fsname);
		for (uint32_t i = 0; i < 6; i++) 
		{
			if (name.find(filters[i]) == 0)
			{
				drives.push_back(StringUtility::ToWideString(entry->mnt_dir));
				break;
			}
		}
		entry = getmntent(file);
    }
    endmntent(file);
    return false;
#else
    return false;
#endif
}
