#include "DriveManager.h"
#include "FileSystem.h"
#include "Drive.h"

using namespace Gensys;

namespace 
{
	std::vector<Drive*> m_drives;
}

void DriveManager::Init()
{
#if defined XBOX_OG
	m_drives.push_back(new Drive(L"C", L"\\Device\\Harddisk0\\Partition2"));
	m_drives.push_back(new Drive(L"D", L"\\Device\\Cdrom0"));
	m_drives.push_back(new Drive(L"E", L"\\Device\\Harddisk0\\Partition1"));
	m_drives.push_back(new Drive(L"F", L"\\Device\\Harddisk0\\Partition6"));
	m_drives.push_back(new Drive(L"G", L"\\Device\\Harddisk0\\Partition7"));
	m_drives.push_back(new Drive(L"X", L"\\Device\\Harddisk0\\Partition3"));
	m_drives.push_back(new Drive(L"Y", L"\\Device\\Harddisk0\\Partition4"));
	m_drives.push_back(new Drive(L"Z", L"\\Device\\Harddisk0\\Partition5"));

	m_drives.push_back(new Drive(L"R", L"\\Device\\Harddisk0\\Partition8"));
	m_drives.push_back(new Drive(L"S", L"\\Device\\Harddisk0\\Partition9"));
	m_drives.push_back(new Drive(L"V", L"\\Device\\Harddisk0\\Partition10"));
	m_drives.push_back(new Drive(L"W", L"\\Device\\Harddisk0\\Partition11"));
	m_drives.push_back(new Drive(L"A", L"\\Device\\Harddisk0\\Partition12"));
	m_drives.push_back(new Drive(L"B", L"\\Device\\Harddisk0\\Partition13"));

	m_drives.push_back(new Drive(L"H", L"\\Device\\MU0"));
	m_drives.push_back(new Drive(L"I", L"\\Device\\MU1"));
	m_drives.push_back(new Drive(L"J", L"\\Device\\MU2"));
	m_drives.push_back(new Drive(L"K", L"\\Device\\MU3"));
	m_drives.push_back(new Drive(L"L", L"\\Device\\MU4"));
	m_drives.push_back(new Drive(L"M", L"\\Device\\MU5"));
	m_drives.push_back(new Drive(L"N", L"\\Device\\MU6"));
	m_drives.push_back(new Drive(L"O", L"\\Device\\MU7"));

	for (size_t i = 0; i < m_drives.size(); i++)
    {
		m_drives.at(i)->Mount();
    }
#elif defined XBOX_360
	m_drives.push_back(new Drive(L"Game", L"\\Device\\Game"));
	m_drives.push_back(new Drive(L"Flash", L"\\Device\\Flash"));
	m_drives.push_back(new Drive(L"Memunit0", L"\\Device\\Mu0"));
	m_drives.push_back(new Drive(L"Memunit1", L"\\Device\\Mu1"));
	m_drives.push_back(new Drive(L"OnBoardMU", L"\\Device\\BuiltInMuSfc"));
	m_drives.push_back(new Drive(L"Dvd", L"\\Device\\Cdrom0"));
	m_drives.push_back(new Drive(L"Hdd1", L"\\Device\\Harddisk0\\Partition1"));
	m_drives.push_back(new Drive(L"Hdd0", L"\\Device\\Harddisk0\\Partition0"));
	m_drives.push_back(new Drive(L"HddX", L"\\Device\\Harddisk0\\SystemPartition"));
	m_drives.push_back(new Drive(L"SysExt", L"\\Device\\Harddisk0\\SystemExtPartition"));
	m_drives.push_back(new Drive(L"Usb0", L"\\Device\\Mass0"));
	m_drives.push_back(new Drive(L"Usb1", L"\\Device\\Mass1"));
	m_drives.push_back(new Drive(L"Usb2", L"\\Device\\Mass2"));
	m_drives.push_back(new Drive(L"HdDvdPlayer", L"\\Device\\HdDvdPlayer"));
	m_drives.push_back(new Drive(L"HdDvdStorage", L"\\Device\\HdDvdStorage"));
	m_drives.push_back(new Drive(L"Transfercable", L"\\Device\\Transfercable"));
	m_drives.push_back(new Drive(L"TransfercableXbox1", L"\\Device\\Transfercable\\Compatibility\\Xbox1"));
	m_drives.push_back(new Drive(L"USBMU0", L"\\Device\\Mass0PartitionFile\\Storage"));
	m_drives.push_back(new Drive(L"USBMU1", L"\\Device\\Mass1PartitionFile\\Storage"));
	m_drives.push_back(new Drive(L"USBMU2", L"\\Device\\Mass2PartitionFile\\Storage"));
	m_drives.push_back(new Drive(L"USBMUCache0", L"\\Device\\Mass0PartitionFile\\StorageSystem"));
	m_drives.push_back(new Drive(L"USBMUCache1", L"\\Device\\Mass1PartitionFile\\StorageSystem"));
	m_drives.push_back(new Drive(L"USBMUCache2", L"\\Device\\Mass2PartitionFile\\StorageSystem"));

	for (size_t i = 0; i < m_drives.size(); i++)
    {
		m_drives.at(i)->Mount();
    }
#else

	std::vector<std::wstring> driveLetters;
	FileSystem::GetMountedDrives(driveLetters);
    for (int i = 0; i < driveLetters.size(); i++)
	{
        std::wstring driveLetter = driveLetters[i];
		m_drives.push_back(new Drive(driveLetter, driveLetter));
	}
	
#endif
}

void DriveManager::Refresh()
{
	for (size_t i = 0; i < m_drives.size(); i++)
    {
		Drive* currentDrive = m_drives.at(i);
		if (!currentDrive->IsMounted()) {
			m_drives.at(i)->Mount();
		}
    }
}

std::vector<std::wstring> DriveManager::GetAllMountPoints()
{
	std::vector<std::wstring> mountPoints;
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = m_drives.at(i);
		mountPoints.push_back(currentDrive->GetMountPoint());		
    }
	return mountPoints;
}

std::vector<std::wstring> DriveManager::GetAllSystemPaths()
{
	std::vector<std::wstring> systemPaths;
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = m_drives.at(i);
		systemPaths.push_back(currentDrive->GetSystemPath());		
    }
	return systemPaths;
}

std::wstring DriveManager::GetSystemPath(std::wstring mountPoint)
{
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return currentDrive->GetSystemPath();
		}
	}
	return L"";
}

std::wstring DriveManager::GetMountPoint(std::wstring systemPath)
{
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = m_drives.at(i);
		if (currentDrive->GetSystemPath() == systemPath) {
			return currentDrive->GetMountPoint();
		}
	}
	return L"";
}

bool DriveManager::IsMounted(std::wstring mountPoint)
{
#if defined UWP_ANGLE || defined WIN_ANGLE
	return true;
#else	
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return currentDrive->IsMounted();
		}
	}
	return false;
#endif
}

long DriveManager::GetTotalNumberOfBytes(std::wstring mountPoint)
{
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return (long)currentDrive->GetTotalNumberOfBytes();
		}
	}
	return 0;
}

long DriveManager::GetFreeNumberOfBytes(std::wstring mountPoint)
{
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return (long)currentDrive->GetFreeNumberOfBytes();
		}
	}
	return 0;
}

long DriveManager::GetVolumeSerialNumber(std::wstring mountPoint)
{
	for (size_t i = 0; i < m_drives.size(); i++) {
		Drive* currentDrive = m_drives.at(i);
		if (currentDrive->GetMountPoint() == mountPoint) {
			return currentDrive->GetVolumeSerialNumber();
		}
	}
	return 0;
}
