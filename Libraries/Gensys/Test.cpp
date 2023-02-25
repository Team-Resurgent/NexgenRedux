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

static bool FileSystem_FileReadOperations(void)
{
	std::wstring configFile = FileSystem::CombinePath(m_mediaDirectory, L"config.ini");
	
	bool fileExists;
	if (FileSystem::FileExists(configFile, fileExists) == false)
	{
		return false;
	}
	if (fileExists == false)
	{
		return false;
	}

	std::string configData;
	if (FileSystem::FileReadAllAsString(configFile, &configData) == false)
	{
		return false;
	}
	if (configData.length() == 0)
	{
		return false;
	}

	FileSystem::FileInfo fileInfo;
	if (FileSystem::FileOpen(configFile, FileSystem::FileMode::FileModeRead, fileInfo) == false)
	{
		return false;
	}

	char buffer;
	uint32_t bytesRead;

	if (FileSystem::FileRead(fileInfo, &buffer, 1, bytesRead) == false)
	{
		return false;
	}
	if (bytesRead != 1 && buffer != '#') 
	{
		return false;
	}

	if (FileSystem::FileSeek(fileInfo, FileSystem::FileSeekMode::FileSeekModeStart, 10) == false)
	{
		return false;
	}

	uint32_t position;
	if (FileSystem::FilePosition(fileInfo, position) == false)
	{
		return false;
	}
	if (position != 10)
	{
		return false;
	}

	if (FileSystem::FileRead(fileInfo, &buffer, 1, bytesRead) == false)
	{
		return false;
	}
	if (bytesRead != 1 && buffer != 'e') 
	{
		return false;
	}

	uint32_t size;
	if (FileSystem::FileSize(fileInfo, size) == false)
	{
		return false;
	}
	if (size == 0)
	{
		return false;
	}

	if (FileSystem::FileClose(fileInfo) == false)
	{
		return false;
	}

	std::wstring extension = FileSystem::GetExtension(configFile);
	if (extension != L".ini")
	{
		return false;
	}

	return true;
}

static bool FileSystem_FileWriteOperations(void)
{
	std::wstring deletemeFile = FileSystem::CombinePath(m_mediaDirectory, L"deletme.txt");
	
	FileSystem::FileInfo fileInfo;
	if (FileSystem::FileOpen(deletemeFile, FileSystem::FileMode::FileModeWrite, fileInfo) == false)
	{
		return false;
	}

	char hello[] = "Hello";
	uint32_t bytesWritten;

	if (FileSystem::FileWrite(fileInfo, &hello[0], 5, bytesWritten) == false)
	{
		return false;
	}
	if (bytesWritten != 5) 
	{
		return false;
	}

	if (FileSystem::FileClose(fileInfo) == false)
	{
		return false;
	}

	if (FileSystem::FileDelete(deletemeFile) == false)
	{
		return false;
	}

	return true;
}

static bool FileSystem_DirectoryReadOperations(void)
{
	bool directoryExists;
	if (FileSystem::DirectoryExists(m_mediaDirectory, directoryExists) == false)
	{
		return false;
	}
	if (directoryExists == false)
	{
		return false;
	}

	return true;
}

static bool FileSystem_DirectoryWriteOperations(void)
{
	std::wstring configFile = FileSystem::CombinePath(m_mediaDirectory, L"config.ini");
	std::wstring deleteMeDirectory = FileSystem::CombinePath(m_mediaDirectory, L"deleteme");
	std::wstring deleteMeChildDirectory = FileSystem::CombinePath(deleteMeDirectory, L"deleteme");
	std::wstring deleteMeDirectoryClone = FileSystem::CombinePath(m_mediaDirectory, L"deletemeClone");

	if (FileSystem::DirectoryCreate(deleteMeDirectory) == false)
	{
		return false;
	}

	if (FileSystem::DirectoryCreate(deleteMeChildDirectory) == false)
	{
		return false;
	}

	bool directoryExists;
	if (FileSystem::DirectoryExists(deleteMeChildDirectory, directoryExists) == false)
	{
		return false;
	}
	if (directoryExists == false)
	{
		return false;
	}

	if (FileSystem::FileCopy(configFile, FileSystem::CombinePath(deleteMeDirectory, L"config.ini")) == false)
	{
		return false;
	}

	if (FileSystem::FileCopy(configFile, FileSystem::CombinePath(deleteMeChildDirectory, L"config.ini")) == false)
	{
		return false;
	}

	if (FileSystem::DirectoryCopy(deleteMeDirectory, deleteMeDirectoryClone) == false)
	{
		return false;
	}
	
	if (FileSystem::DirectoryDelete(deleteMeDirectory, true) == false)
	{
		return false;
	}

	if (FileSystem::DirectoryExists(deleteMeDirectory, directoryExists) == false)
	{
		return false;
	}
	if (directoryExists == true)
	{
		return false;
	}

	if (FileSystem::DirectoryDelete(deleteMeDirectoryClone, true) == false)
	{
		return false;
	}
	if (FileSystem::DirectoryExists(deleteMeDirectoryClone, directoryExists) == false)
	{
		return false;
	}
	if (directoryExists == true)
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

	for (uint32_t i = 0; i < (uint32_t)min(drives.size(), 3); i ++)
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

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing FileSystem_FileOperations");
	assert(FileSystem_FileReadOperations());

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing FileSystem_FileOperations");
	assert(FileSystem_FileWriteOperations());

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing FileSystem_DirectoryReadOperations");
	assert(FileSystem_DirectoryReadOperations());

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing FileSystem_DirectoryWriteOperations");
	assert(FileSystem_DirectoryWriteOperations());

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"Testing DriveManager_GetMountedDrivesAndDetails");
	assert(DriveManager_GetMountedDrivesAndDetails());
}