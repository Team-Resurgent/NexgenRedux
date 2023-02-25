#include "Test.h"

#include "FileSystem.h"
#include "DriveManager.h"
#include "DebugUtility.h"

#include <string>
#include <assert.h>

using namespace Gensys;

static std::wstring m_appDirectory;
static std::wstring m_mediaDirectory;

static bool FileSystem_GetAppDirectory(void)
{
	std::wstring appDirectory = L"";
	bool result = FileSystem::GetAppDirectory(appDirectory);
	if (result == false) 
	{
		return false;
	}

	if (appDirectory.length() == 0)
	{
		return false;
	}

	m_appDirectory = appDirectory;
	return true;
}

static bool FileSystem_GetMediaDirectory(void)
{
	std::wstring mediaDirectory = L"";
	bool result = FileSystem::GetMediaDirectory(mediaDirectory);
	if (result == false) 
	{
		return false;
	}

	if (mediaDirectory.length() == 0)
	{
		return false;
	}

	m_mediaDirectory = mediaDirectory;
	return true;
}

static bool FileSystem_FileGetFileInfoDetail(void)
{
	bool result;
	FileSystem::FileInfoDetail fileInfoDetail;

	result = FileSystem::FileGetFileInfoDetail(m_appDirectory, fileInfoDetail);
	if (result == false) 
	{
		return false;
	}

	result = FileSystem::FileGetFileInfoDetail(FileSystem::CombinePath(m_mediaDirectory, L"config.ini"), fileInfoDetail);
	if (result == false) 
	{
		return false;
	}

	if (fileInfoDetail.path.length() == 0) 
	{
		return false;
	}

	return true;
}


static bool FileSystem_FileGetFileInfoDetails(void)
{
	std::vector<FileSystem::FileInfoDetail> fileInfoDetails;

	bool result = FileSystem::FileGetFileInfoDetails(m_appDirectory, fileInfoDetails);
	if (result == false) 
	{
		return false;
	}

	if (fileInfoDetails.size() == 0) 
	{
		return false;
	}
	
	return true;
}

static bool DriveManager_GetMountedDrivesAndDetails(void)
{
	std::vector<std::wstring> drives;

	bool result = DriveManager::GetMountedDrives(drives);
	if (result == false) 
	{
		return false;
	}

	if (drives.size() == 0) 
	{
		return false;
	}

	for (uint32_t i = 0; i < (uint32_t)drives.size(); i ++)
	{
		uint64_t totalSize = 0;
		if (DriveManager::GetTotalNumberOfBytes(drives.at(i), totalSize) == false)
		{
			return false;
		}
		if (totalSize == 0)
		{
			return false;
		}
		uint64_t totalFree = 0;
		if (DriveManager::GetTotalFreeNumberOfBytes(drives.at(i), totalFree) == false)
		{
			return false;
		}
		if (totalFree == 0)
		{
			return false;
		}
		uint32_t serial = 0;
		if (DriveManager::GetVolumeSerialNumber(drives.at(i), serial) == false)
		{
			return false;
		}
		if (serial == 0)
		{
			return false;
		}
	}
	return true;
}

void Test::RunTests(void)
{
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing FileSystem_GetAppDirectory");
	assert(FileSystem_GetAppDirectory());

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing FileSystem_GetMediaDirectory");
	assert(FileSystem_GetMediaDirectory());

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing FileSystem_FileGetFileInfoDetail");
	assert(FileSystem_FileGetFileInfoDetail());

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing FileSystem_FileGetFileInfoDetails");
	assert(FileSystem_FileGetFileInfoDetails());

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing DriveManager_GetMountedDrives");
	assert(DriveManager_GetMountedDrivesAndDetails());
}