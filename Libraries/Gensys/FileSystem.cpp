#include "FileSystem.h"
#include "StringUtility.h"
#include "XboxInternals.h"
#include "DriveManager.h"
#include <cstring>

#if defined NEXGEN_WIN
#include <windows.h>
#endif
#if defined NEXGEN_MAC || defined NEXGEN_LINUX
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif
#if defined NEXGEN_LINUX
#include <mntent.h>
#endif

using namespace Gensys;

namespace {

#if defined NEXGEN_OG 

	std::wstring MapSystemPath(std::wstring const path)
	{
		std::vector<std::wstring> systemPaths = DriveManager::GetAllSystemPaths();
		for (size_t i = 0; i < systemPaths.size(); i++) {
			std::wstring systemPath = systemPaths.at(i);
			if (StringUtility::StartsWith(path, systemPath + L"\\", true)) {
				std::wstring mountPoint = DriveManager::GetMountPoint(systemPath);
				std::wstring temp = path.substr(systemPath.length());
				std::wstring result = mountPoint + L":" + temp;
				return result;
			}
		}
		return path;
	}

#endif

}

//#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_WIN
//
//time_t FileTimeToTime(FILETIME fileTime)
//{
//	const uint64_t WINDOWS_TICK = 10000000ULL;
//	const uint64_t SEC_TO_UNIX_EPOCH = 11644473600ULL;
//
//	uint64_t windowsTicks = ((uint64_t)fileTime.dwHighDateTime << 32) | fileTime.dwLowDateTime;
//	return (time_t)(windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
//}
//
//#endif

bool FileSystem::FileGetFileInfoDetail(std::wstring const path, FileInfoDetail& fileInfoDetail)
{
#if defined NEXGEN_OG || defined NEXGEN_360

	const DWORD invalidFileAttributes = (DWORD)0xFFFFFFFF;

	DWORD attributes = GetFileAttributesA(StringUtility::ToString(path).c_str());
	if (attributes == invalidFileAttributes)
	{
		return false;
	}

	fileInfoDetail.path = path;
	fileInfoDetail.isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	fileInfoDetail.isNormal = (attributes & FILE_ATTRIBUTE_NORMAL) != 0;

	HANDLE fileHandle = CreateFileA(StringUtility::ToString(path).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, fileInfoDetail.isDirectory ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL);
	if (fileHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

	FILETIME fileTimeAccess;
	FILETIME fileTimeWrite;
	if (GetFileTime(fileHandle, NULL, &fileTimeAccess, &fileTimeWrite) == FALSE)
	{
		return false;
	}

	FILETIME fileTimeAccessLocal;
	if (FileTimeToLocalFileTime(&fileTimeAccess, &fileTimeAccessLocal) == FALSE)
	{
		return false;
	}

	SYSTEMTIME systemTimeAccessLocal;
	if (FileTimeToSystemTime(&fileTimeAccessLocal, &systemTimeAccessLocal) == FALSE)
	{
		return false;
	}

	fileInfoDetail.accessTime.month = systemTimeAccessLocal.wMonth;
	fileInfoDetail.accessTime.day = systemTimeAccessLocal.wDay;
	fileInfoDetail.accessTime.year = systemTimeAccessLocal.wYear;
	fileInfoDetail.accessTime.hour = systemTimeAccessLocal.wHour;
	fileInfoDetail.accessTime.minute = systemTimeAccessLocal.wMinute;
	fileInfoDetail.accessTime.second = systemTimeAccessLocal.wSecond;
	fileInfoDetail.accessTime.milliseconds = systemTimeAccessLocal.wMilliseconds;

	FILETIME fileTimeWriteLocal;
	if (FileTimeToLocalFileTime(&fileTimeWrite, &fileTimeWriteLocal) == FALSE)
	{
		return false;
	}

	SYSTEMTIME systemTimeWriteLocal;
	if (FileTimeToSystemTime(&fileTimeWriteLocal, &systemTimeWriteLocal) == FALSE)
	{
		return false;
	}

	fileInfoDetail.writeTime.month = systemTimeWriteLocal.wMonth;
	fileInfoDetail.writeTime.day = systemTimeWriteLocal.wDay;
	fileInfoDetail.writeTime.year = systemTimeWriteLocal.wYear;
	fileInfoDetail.writeTime.hour = systemTimeWriteLocal.wHour;
	fileInfoDetail.writeTime.minute = systemTimeWriteLocal.wMinute;
	fileInfoDetail.writeTime.second = systemTimeAccessLocal.wSecond;
	fileInfoDetail.writeTime.milliseconds = systemTimeAccessLocal.wMilliseconds;

	DWORD fileSize = GetFileSize(fileHandle, NULL);
	fileInfoDetail.size = fileSize;
		
	CloseHandle(fileHandle);
	return true;

#elif defined NEXGEN_WIN

	const DWORD invalidFileAttributes = (DWORD)0xFFFFFFFF;

	DWORD attributes = GetFileAttributesW(path.c_str());
	if (attributes == invalidFileAttributes)
	{
		return false;
	}

	fileInfoDetail.isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	fileInfoDetail.isNormal = (attributes & FILE_ATTRIBUTE_NORMAL) != 0;

	HANDLE fileHandle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, fileInfoDetail.isDirectory ? FILE_FLAG_BACKUP_SEMANTICS : 0, NULL);
	if (fileHandle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

	FILETIME fileTimeAccess;
	FILETIME fileTimeWrite;
	if (GetFileTime(fileHandle, NULL, &fileTimeAccess, &fileTimeWrite) == FALSE)
	{
		return false;
	}

	FILETIME fileTimeAccessLocal;
	if (FileTimeToLocalFileTime(&fileTimeAccess, &fileTimeAccessLocal) == FALSE)
	{
		return false;
	}

	SYSTEMTIME systemTimeAccessLocal;
	if (FileTimeToSystemTime(&fileTimeAccessLocal, &systemTimeAccessLocal) == FALSE)
	{
		return false;
	}

	fileInfoDetail.path = path;
	fileInfoDetail.accessTime.month = systemTimeAccessLocal.wMonth;
	fileInfoDetail.accessTime.day = systemTimeAccessLocal.wDay;
	fileInfoDetail.accessTime.year = systemTimeAccessLocal.wYear;
	fileInfoDetail.accessTime.hour = systemTimeAccessLocal.wHour;
	fileInfoDetail.accessTime.minute = systemTimeAccessLocal.wMinute;
	fileInfoDetail.accessTime.second = systemTimeAccessLocal.wSecond;
	fileInfoDetail.accessTime.milliseconds = systemTimeAccessLocal.wMilliseconds;

	FILETIME fileTimeWriteLocal;
	if (FileTimeToLocalFileTime(&fileTimeWrite, &fileTimeWriteLocal) == FALSE)
	{
		return false;
	}

	SYSTEMTIME systemTimeWriteLocal;
	if (FileTimeToSystemTime(&fileTimeWriteLocal, &systemTimeWriteLocal) == FALSE)
	{
		return false;
	}

	fileInfoDetail.writeTime.month = systemTimeWriteLocal.wMonth;
	fileInfoDetail.writeTime.day = systemTimeWriteLocal.wDay;
	fileInfoDetail.writeTime.year = systemTimeWriteLocal.wYear;
	fileInfoDetail.writeTime.hour = systemTimeWriteLocal.wHour;
	fileInfoDetail.writeTime.minute = systemTimeWriteLocal.wMinute;
	fileInfoDetail.writeTime.second = systemTimeAccessLocal.wSecond;
	fileInfoDetail.writeTime.milliseconds = systemTimeAccessLocal.wMilliseconds;

	DWORD fileSize = GetFileSize(fileHandle, NULL);
	fileInfoDetail.size = fileSize;
		
	CloseHandle(fileHandle);
	return true;

#elif defined NEXGEN_MAC || defined NEXGEN_LINUX

	return false;

#else

	return false;

#endif
}

bool FileSystem::FileGetFileInfoDetails(std::wstring const path, std::vector<FileInfoDetail>& fileInfoDetails)
{
	std::wstring searchPath = StringUtility::RightTrim(path, GetPathSeparator());

#if defined NEXGEN_OG || defined NEXGEN_360

	WIN32_FIND_DATAA findData;
	HANDLE findHandle = FindFirstFileA(StringUtility::ToString(CombinePath(path, L"*")).c_str(), &findData);
	if (findHandle == INVALID_HANDLE_VALUE) 
	{     
		return false;
	} 
	do 
	{ 
		FileInfoDetail fileInfoDetail;
		std::wstring currentPath = CombinePath(path, StringUtility::ToWideString(findData.cFileName));
		if (FileGetFileInfoDetail(currentPath, fileInfoDetail) == false)
		{
			FindClose(findHandle);
			return false;
		}
		fileInfoDetails.push_back(fileInfoDetail);			
	} 
	while (FindNextFile(findHandle, &findData)); 
	FindClose(findHandle);
	return true;

#elif defined NEXGEN_WIN

	WIN32_FIND_DATAW findData;
	HANDLE findHandle = FindFirstFileW(CombinePath(path, L"*").c_str(), &findData);
	if (findHandle == INVALID_HANDLE_VALUE) 
	{     
		return false;
	} 
	do 
	{ 
		FileInfoDetail fileInfoDetail;
		std::wstring currentPath = CombinePath(path, findData.cFileName);
		if (FileGetFileInfoDetail(currentPath, fileInfoDetail) == false)
		{
			FindClose(findHandle);
			return false;
		}
		fileInfoDetails.push_back(fileInfoDetail);			
	} 
	while (FindNextFile(findHandle, &findData)); 
	FindClose(findHandle);
	return true;

#elif defined NEXGEN_MAC || defined NEXGEN_LINUX
	struct stat statBuffer;
	DIR* dir = opendir(StringUtility::ToString(searchPath).c_str());
	if (dir == NULL) 
	{
		return false;
	}
	struct dirent *entry = readdir(dir);
	while (entry != NULL) 
	{
		std::wstring wideName = StringUtility::ToWideString(entry->d_name);
		if (stat(StringUtility::ToString(CombinePath(searchPath, wideName)).c_str(), &statBuffer) != 0)
		{
			continue;
		}
		FileInfoDetail fileInfoDetail;
		fileInfoDetail.isDirectory = (entry->d_type & DT_DIR) != 0;
		fileInfoDetail.isNormal = (entry->d_type & DT_REG) != 0;
		fileInfoDetail.name =wideName;
		fileInfoDetail.path = searchPath;
		fileInfoDetail.size = statBuffer.st_size;
		#if defined NEXGEN_MAC
		struct tm *lastAccessTime = localtime(&statBuffer.st_atimespec.tv_sec);
 		memcpy(&fileInfoDetail.lastAccessTime, lastAccessTime, sizeof(fileInfoDetail.lastAccessTime));
		struct tm *lastWriteTime = localtime(&statBuffer.st_mtimespec.tv_sec);
 		memcpy(&fileInfoDetail.lastWriteTime, lastWriteTime, sizeof(fileInfoDetail.lastWriteTime));
		#else
		struct tm *lastAccessTime = localtime(&statBuffer.st_atim.tv_sec);
 		memcpy(&fileInfoDetail.lastAccessTime, lastAccessTime, sizeof(fileInfoDetail.lastAccessTime));
		struct tm *lastWriteTime = localtime(&statBuffer.st_mtim.tv_sec);
 		memcpy(&fileInfoDetail.lastWriteTime, lastWriteTime, sizeof(fileInfoDetail.lastWriteTime));
		#endif
		fileInfoDetails.push_back(fileInfoDetail);		
		entry = readdir(dir);
	}
	closedir(dir);
	return true;
#else
	return false;
#endif
}

bool FileSystem::FileOpen(std::wstring const path, FileMode const fileMode, FileInfo& fileInfo)
{
	std::wstring access = L"";
	if (fileMode == FileModeRead) {
		access = L"r";
	}
	else if (fileMode == FileModeWrite) {
		access = L"w";
	}
	else if (fileMode == FileModeAppend) {
		access = L"a";
	}
	else if (fileMode == FileModeReadUpdate) {
		access = L"r+";
	}
	else if (fileMode == FileModeWriteUpdate) {
		access = L"w+";
	}
	else if (fileMode == FileModeAppendUpdate) {
		access = L"a+";
	}
	access = access + L"b";
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_MAC || defined NEXGEN_LINUX
	fileInfo.file = fopen(StringUtility::ToString(path).c_str(), StringUtility::ToString(access).c_str());
#elif defined NEXGEN_WIN 
	fopen_s((FILE **)&fileInfo.file, StringUtility::ToString(path).c_str(), StringUtility::ToString(access).c_str());
#endif
    return fileInfo.file != NULL;
}

bool FileSystem::FileRead(FileInfo const fileInfo, char* readBuffer, uint32_t const bytesToRead, uint32_t& bytesRead)
{
	bytesRead = (uint32_t)fread(readBuffer, bytesToRead, 1, (FILE*)fileInfo.file);
	return true;
}

bool FileSystem::FileReadAllAsString(std::wstring const path, std::string* buffer)
{
	FILE *file;
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_MAC || defined NEXGEN_LINUX
	file = fopen(StringUtility::ToString(path).c_str(), "rb");
#elif defined NEXGEN_WIN
	fopen_s(&file, StringUtility::ToString(path).c_str(), "rb");
#endif
	if(file == NULL)
	{
		return false;
	}

	fseek(file, 0, SEEK_END);
	uint32_t fileLength = ftell(file);
	fseek(file, 0, SEEK_SET);

	buffer->resize(fileLength);
	uint32_t bytesRead = (uint32_t)fread((void*)buffer->data(), 1, fileLength, file);
	fclose(file);

	return bytesRead == fileLength;
}

bool FileSystem::FileWrite(FileInfo const fileInfo, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten)
{
	bytesWritten = (uint32_t)fwrite(writeBuffer, 1, bytesToWrite, (FILE*)fileInfo.file);
	return bytesWritten == bytesToWrite;
}

bool FileSystem::FileClose(FileInfo const fileInfo)
{
	return fclose((FILE*)fileInfo.file) == 0;
}

bool FileSystem::FileSeek(FileInfo const fileInfo, FileSeekMode const fileSeekMode, uint32_t const offset)
{
	int seek = SEEK_CUR;
	if (fileSeekMode == FileSeekModeStart)
	{
		seek = SEEK_SET;
	}
	else if (fileSeekMode == FileSeekModeEnd)
	{
		seek = SEEK_END;
	}
	FILE *file = (FILE*)fileInfo.file;
	fseek(file, 0, seek);
	return true;
}

bool FileSystem::FilePosition(FileInfo const fileInfo, uint32_t& position)
{
	FILE *file = (FILE*)fileInfo.file;
	position = ftell(file);
	return true;
}

bool FileSystem::DirectoryCreate(const std::wstring path)
{	
#if defined NEXGEN_OG || defined NEXGEN_360
    return CreateDirectoryA(StringUtility::ToString(path).c_str(), NULL) == TRUE;
#elif defined NEXGEN_WIN
	return CreateDirectoryW(path.c_str(), NULL) == TRUE;
#elif defined NEXGEN_MAC || defined NEXGEN_LINUX
	return mkdir(StringUtility::ToString(path).c_str(), 0777) == 0;
#else
	return false;
#endif
}

bool FileSystem::DirectoryDelete(std::wstring const path, bool const recursive)
{
	std::vector<FileInfoDetail> fileInfoDetails;
	if (!FileSystem::FileGetFileInfoDetails(path, fileInfoDetails))
	{
		return false;
	}

	for (uint32_t i = 0; i < fileInfoDetails.size(); i++)
	{
		FileInfoDetail fileInfoDetail = fileInfoDetails.at(i);
		if (fileInfoDetail.isDirectory) 
		{
			std::wstring directoryToDelete = fileInfoDetail.path;
			if (recursive && !DirectoryDelete(directoryToDelete, true)) 
			{
				return false;
			}
		}
		else
		{
			std::wstring fileToDelete = fileInfoDetail.path;
			if (!FileDelete(fileToDelete))
			{
				return false;
			}
		}
	}

	if (recursive && !DirectoryDelete(path, false))
	{
		return false;
	}

	return true;
}

bool FileSystem::FileDelete(std::wstring const path)
{
	return remove(StringUtility::ToString(path).c_str()) == 0;
}

bool FileSystem::FileCopy(std::wstring const sourcePath, std::wstring const destPath)
{
	FileInfo sourceFileInfo;
	FileOpen(sourcePath, FileModeRead, sourceFileInfo);
	
	FileInfo destFileInfo;
	FileOpen(destPath, FileModeWrite, destFileInfo);
	
	char *buffer = (char*)malloc(32758);
	uint32_t bytesRead = (uint32_t)fread(buffer, 1, 32768, (FILE*)sourceFileInfo.file);
	while (bytesRead > 0)
	{
		fwrite(buffer, 1, bytesRead, (FILE*)destFileInfo.file);
		bytesRead = (uint32_t)fread(buffer, 1, 32768, (FILE*)sourceFileInfo.file);
	}
	free(buffer);
	
	FileClose(sourceFileInfo);
	FileClose(destFileInfo);
	return true;
}

bool FileSystem::FileMove(std::wstring const sourcePath, std::wstring const destPath)
{
	FileCopy(sourcePath, destPath);
	FileDelete(sourcePath);
    return true;
}

bool FileSystem::DirectoryCopy(std::wstring const sourcePath, std::wstring const destPath)
{
	std::wstring directory = GetFileName(sourcePath);
	std::wstring baseSourcePath = GetDirectory(sourcePath);
		
	std::vector<std::wstring> directoriesToCopy;
	directoriesToCopy.push_back(directory);
	while (directoriesToCopy.size() > 0)
	{
		std::wstring directoryToCopy;
		directoryToCopy = directoriesToCopy.front();
		directoriesToCopy.erase(directoriesToCopy.begin());

		std::wstring tempSourcePath = FileSystem::CombinePath(baseSourcePath, directoryToCopy);
		std::wstring tempDestPath = FileSystem::CombinePath(destPath, directoryToCopy);
		if (!FileSystem::DirectoryCreate(tempDestPath))
		{
			return false;
		}

		std::vector<FileInfoDetail> fileInfoDetails;
		if (!FileSystem::FileGetFileInfoDetails(tempSourcePath, fileInfoDetails))
		{
			return false;
		}

		for (uint32_t i = 0; i < fileInfoDetails.size(); i++)
		{
			FileInfoDetail fileInfoDetail = fileInfoDetails.at(i);
			if (fileInfoDetail.isDirectory) 
			{
				std::wstring directoryToAdd = fileInfoDetail.path;
				directoriesToCopy.push_back(directoryToAdd);
			}
			else
			{
				std::wstring fileToCopy = FileSystem::GetFileName(fileInfoDetail.path);
				std::wstring sourceFile = FileSystem::CombinePath(baseSourcePath, fileToCopy);
				std::wstring destFile = FileSystem::CombinePath(destPath, fileToCopy);
				if (!FileSystem::FileCopy(sourceFile, destFile))
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool FileSystem::FileSize(FileInfo const fileInfo, uint32_t& size)
{
	FILE* file = (FILE*)fileInfo.file;
	uint32_t filePosition = ftell(file);
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, filePosition, SEEK_SET);
    return true;
}

bool FileSystem::FileExists(std::wstring const path, bool& exists)
{
#if defined NEXGEN_OG || defined NEXGEN_360
	WIN32_FIND_DATAA findData;
	HANDLE handle = FindFirstFileA(StringUtility::ToString(path).c_str(), &findData);
	if (handle == INVALID_HANDLE_VALUE) 
	{     
		return false;
	} 
    exists = (findData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) != 0;
	FindClose(handle);
	return true;
#elif defined NEXGEN_WIN
	WIN32_FIND_DATAW findData;
	HANDLE handle = FindFirstFileW(path.c_str(), &findData);
	if (handle == INVALID_HANDLE_VALUE) 
	{     
		return false;
	} 
    exists = (findData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) != 0;
	FindClose(handle);
	return true;
#elif defined NEXGEN_MAC || NEXGEN_LINUX
	struct stat statBuffer;
	if (stat(StringUtility::ToString(path).c_str(), &statBuffer) != 0)
	{
		return false;
	}
    exists = (statBuffer.st_mode & S_IFMT) == S_IFREG;
	return true;
#else
	return false;
#endif
}

bool FileSystem::DirectoryExists(std::wstring const path, bool& exists)
{
#if defined NEXGEN_OG || defined NEXGEN_360
	WIN32_FIND_DATAA findData;
	HANDLE handle = FindFirstFileA(StringUtility::ToString(path).c_str(), &findData);
	if (handle == INVALID_HANDLE_VALUE) 
	{     
		return false;
	} 
    exists = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	FindClose(handle);
	return true;
#elif defined NEXGEN_WIN
	WIN32_FIND_DATAW findData;
	HANDLE handle = FindFirstFileW(path.c_str(), &findData);
	if (handle == INVALID_HANDLE_VALUE) 
	{     
		return false;
	} 
    exists = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	FindClose(handle);
	return true;
#elif defined NEXGEN_MAC || NEXGEN_LINUX
	struct stat statBuffer;
	if (stat(StringUtility::ToString(path).c_str(), &statBuffer) != 0)
	{
		return false;
	}
    exists = (statBuffer.st_mode & S_IFMT) == S_IFDIR;
	return true;
#else
	return false;
#endif
}

std::wstring FileSystem::GetFileName(std::wstring const path)
{
	const std::size_t found = path.find_last_of(GetPathSeparator());
	return found == std::wstring::npos ? L"" : path.substr(found + 1);
}

std::wstring FileSystem::GetDirectory(std::wstring const path)
{
	const std::size_t found = path.find_last_of(GetPathSeparator());
	return found == std::wstring::npos ? path : path.substr(0, found);
}

bool FileSystem::GetAppDirectory(std::wstring& appDirectory)
{
#if defined NEXGEN_OG
	char buffer[260];
	STRING *temp = (STRING*)XeImageFileName;
	sprintf(buffer, temp->Buffer);
	appDirectory = MapSystemPath(GetDirectory(StringUtility::ToWideString(std::string(&buffer[0], temp->Length))));	
	return true;
#elif defined NEXGEN_360
	appDirectory = L"Game:";
	return true;
#elif defined NEXGEN_WIN
	wchar_t buffer[260];
	const size_t length = GetModuleFileNameW(0, &buffer[0], 260);
	std::wstring exeFilePath = std::wstring(buffer, length);
	appDirectory = GetDirectory(exeFilePath);
	return true;	
#elif defined NEXGEN_MAC 
	char result[PATH_MAX];
	getcwd(result, PATH_MAX);
  	appDirectory = StringUtility::ToWideString(std::string( result));
	return true;
#elif defined NEXGEN_LINUX
  	char result[PATH_MAX];
  	ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX );
  	appDirectory = StringUtility::ToWideString(std::string( result, (count > 0) ? count : 0 ));
	return true;
#else
    return false;
#endif
}

bool FileSystem::GetMediaDirectory(std::wstring& mediaDirectory)
{
#if defined NEXGEN_UWP
	std::wstring dataPath = Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data();
	mediaDirectory = CombinePath(dataPath, L"Media");
	return true;
#elif defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX
	std::wstring appDirectory;
	if (!GetAppDirectory(appDirectory))
	{
		return false;
	}
	mediaDirectory = CombinePath(appDirectory, L"Media");
	return true;
#else
	return false;
#endif
}

wchar_t FileSystem::GetPathSeparator()
{
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_WIN
	return L'\\';
#else
	return L'/';
#endif
}

std::wstring FileSystem::CombinePath(std::wstring const first, std::wstring const second)
{
	return StringUtility::RightTrim(first, GetPathSeparator()) + GetPathSeparator() + StringUtility::LeftTrim(second, GetPathSeparator());
}

std::wstring FileSystem::GetExtension(std::wstring const path)
{
	const std::size_t found = path.find_last_of(L'.');
	return found == std::wstring::npos ? L"" : path.substr(found + 1);
}

// std::wstring FileSystem::GetDriveLetter(std::wstring const path)
// {
// 	if (path.length() > 1 && path.at(1) == L':') {
// 		return path.substr(0, 1);
// 	}
// 	return L"";
// }

// std::wstring FileSystem::ReplaceDriveLetter(std::wstring const path, std::wstring const driveLetter)
// {
// 	if (path.length() > 1 && path.at(1) == L':') {
// 		return driveLetter + path.substr(1);
// 	}
// 	return L"";
// }

