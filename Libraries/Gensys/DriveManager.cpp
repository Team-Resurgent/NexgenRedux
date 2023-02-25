#include "DriveManager.h"
#include "FileSystem.h"
#include "Drive.h"
#include "StringUtility.h"

#if defined NEXGEN_LINUX
#include <mntent.h>
#endif

using namespace Gensys;

namespace
{
#if defined NEXGEN_OG || defined NEXGEN_360
	bool m_initialized;
#endif
	std::vector<Drive> m_drives;
}

bool DriveManager::GetTotalNumberOfBytes(std::wstring mountPoint, uint64_t& totalSize)
{
	InitOrRefresh();

	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			totalSize = (uint64_t)currentDrive->GetTotalNumberOfBytes();
			return true;
		}
	}
	return false;
}

bool DriveManager::GetTotalFreeNumberOfBytes(std::wstring mountPoint, uint64_t& totalFree)
{
	InitOrRefresh();

	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			totalFree = (uint64_t)currentDrive->GetTotalFreeNumberOfBytes();
			return true;
		}
	}
	return false;
}

bool DriveManager::GetMountedDrives(std::vector<std::wstring>& drives)
{
	InitOrRefresh();

	drives.clear();
	for (uint32_t i = 0; i < m_drives.size(); i++)
	{
		Drive* currentDrive = &m_drives.at(i);
		drives.push_back(currentDrive->GetMountPoint());
	}
	return true;
}

std::wstring DriveManager::MapSystemPath(std::wstring const path)
{
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = &m_drives.at(i);
		std::wstring systemPath = currentDrive->GetSystemPath();
		if (StringUtility::StartsWith(path, systemPath + L"\\", true)) {
			std::wstring mountPoint = currentDrive->GetMountPoint();
			std::wstring temp = path.substr(systemPath.length());
			std::wstring result = mountPoint + L":" + temp;
			return result;
		}
	}
	return path;
}

void DriveManager::InitOrRefresh()
{

#if defined NEXGEN_OG

	for (size_t i = 0; i < m_drives.size(); i++)
	{
		Drive* currentDrive = &m_drives.at(i);
		if (!currentDrive->IsMounted()) {
			currentDrive->Mount();
		}
	}
	m_initialized = true;

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

	for (size_t i = 0; i < m_drives.size(); i++)
	{
		Drive* currentDrive = &m_drives.at(i);
		if (!currentDrive->IsMounted()) {
			currentDrive->Mount();
		}
	}
	m_initialized = true;

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

#elif defined NEXGEN_WIN

	m_drives.clear();

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
			std::wstring driveLetter = StringUtility::ToWideString(std::string(buffer));
			m_drives.push_back(Drive(driveLetter, driveLetter + L":\\"));
		}
	}

#elif defined NEXGEN_MAC

	m_drives.clear();

	std::vector<FileSystem::FileInfoDetail> fileInfoDetails;
	if (!FileSystem::FileGetFileInfoDetails(L"/Volumes", fileInfoDetails))
	{
		return false;
	}
	for (uint32_t i = 0; i < fileInfoDetails.size(); i++)
	{
		m_drives.push_back(Drive(FileSystem::GetDurectory(fileInfoDetails.at(i).path), fileInfoDetails.at(i).path));
	}
	return true;

#elif defined NEXGEN_LINUX

  	FILE *file = setmntent("/etc/mtab", "r");
    if (file == NULL) 
	{
        return;
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
				std::wstring nameToAdd = FileSystem::GetFileName(StringUtility::ToWideString(name));
				bool found = false;
				for (uint32_t i = 0; i < (uint32_t)m_drives.size(); i++)
				{
					if (m_drives.at(i).GetMountPoint() == nameToAdd) 
					{
						found = true;
						break;
					}
				}
				if (found == false) 
				{				
					m_drives.push_back(Drive(FileSystem::GetFileName(nameToAdd), StringUtility::ToWideString(entry->mnt_dir)));
				}
				break;
			}
		}
		entry = getmntent(file);
    }
    endmntent(file);

#endif
}
