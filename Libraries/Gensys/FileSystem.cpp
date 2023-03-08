#include "FileSystem.h"
#include "StringUtility.h"
#include "XboxInternals.h"
#include "DriveManager.h"

#include <cstring>
#include <vector>
#include <map>
#include <ctime>


#if defined NEXGEN_WIN || defined NEXGEN_UWP
#include <windows.h>
#elif defined NEXGEN_MAC
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#elif defined NEXGEN_LINUX
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <mntent.h>
#endif

using namespace Gensys;

namespace 
{
	typedef struct FileContainer
	{
		void* file;
	} FileContainer;

	uint32_t m_maxFileContainerID = 0;
	std::map<uint32_t, FileContainer> m_fileContainerMap;

	uint32_t AddFileContainer(FileContainer fileContainer)
	{
		uint32_t result = m_maxFileContainerID;
		m_fileContainerMap.insert(std::pair<uint32_t, FileContainer>(result, fileContainer));
		m_maxFileContainerID++;
		return result;
	}

	FileContainer* GetFileContainer(uint32_t fileHandle)
	{
		std::map<uint32_t, FileContainer>::iterator it = m_fileContainerMap.find(fileHandle);
		if (it == m_fileContainerMap.end()) {
			return NULL;
		}
		return (FileContainer*)&it->second;
	}

	void DeleteFileContainer(uint32_t fileHandle)
	{
		m_fileContainerMap.erase(fileHandle);
	}
}

void FileSystem::Dispose(void) 
{
	while (m_fileContainerMap.size() > 0)
	{
		std::map<uint32_t, FileContainer>::iterator it = m_fileContainerMap.begin();
		FileSystem::FileClose(it->first);
	}
}

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
	fileInfoDetail.isFile = (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;

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

	fileInfoDetail.path = path;
	fileInfoDetail.isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	fileInfoDetail.isFile = (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;

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

	fileInfoDetail.accessTime.month = systemTimeAccessLocal.wMonth;
	fileInfoDetail.accessTime.day = systemTimeAccessLocal.wDay;
	fileInfoDetail.accessTime.year = systemTimeAccessLocal.wYear;
	fileInfoDetail.accessTime.hour = systemTimeAccessLocal.wHour;
	fileInfoDetail.accessTime.minute = systemTimeAccessLocal.wMinute;
	fileInfoDetail.accessTime.second = systemTimeAccessLocal.wSecond;

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

	DWORD fileSize = GetFileSize(fileHandle, NULL);
	fileInfoDetail.size = fileSize;

	CloseHandle(fileHandle);
	return true;

#elif defined NEXGEN_UWP

	const DWORD invalidFileAttributes = (DWORD)0xFFFFFFFF;

	DWORD attributes = GetFileAttributesW(path.c_str());
	if (attributes == invalidFileAttributes)
	{
		return false;
	}

	fileInfoDetail.path = path;
	fileInfoDetail.isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	fileInfoDetail.isFile = (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;

	CREATEFILE2_EXTENDED_PARAMETERS extParams = { 0 };
	extParams.dwSize = sizeof(extParams);
	extParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	extParams.dwFileFlags = fileInfoDetail.isDirectory ? FILE_FLAG_BACKUP_SEMANTICS : 0;
	HANDLE fileHandle = CreateFile2(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, &extParams);
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

	LARGE_INTEGER fileSize;
	if (GetFileSizeEx(fileHandle, &fileSize) == false)
	{
		return false;
	}
	fileInfoDetail.size = fileSize.LowPart;

	CloseHandle(fileHandle);
	return true;

#elif defined NEXGEN_MAC

	struct stat statBuffer;
	if (stat(StringUtility::ToString(path).c_str(), &statBuffer) != 0)
	{
		return false;
	}

	fileInfoDetail.path = path;
	fileInfoDetail.isDirectory = S_ISDIR(statBuffer.st_mode);
	fileInfoDetail.isFile = !S_ISDIR(statBuffer.st_mode);

	struct tm *accessTime = localtime(&statBuffer.st_atimespec.tv_sec);

	fileInfoDetail.accessTime.month = accessTime->tm_mon + 1;
	fileInfoDetail.accessTime.day = accessTime->tm_mday;
	fileInfoDetail.accessTime.year = accessTime->tm_year + 1900;
	fileInfoDetail.accessTime.hour = accessTime->tm_hour;
	fileInfoDetail.accessTime.minute = accessTime->tm_min;
	fileInfoDetail.accessTime.second = accessTime->tm_sec;

	struct tm *writeTime = localtime(&statBuffer.st_mtimespec.tv_sec);

	fileInfoDetail.writeTime.month = writeTime->tm_mon + 1;
	fileInfoDetail.writeTime.day = writeTime->tm_mday;
	fileInfoDetail.writeTime.year = writeTime->tm_year + 1900;
	fileInfoDetail.writeTime.hour = writeTime->tm_hour;
	fileInfoDetail.writeTime.minute = writeTime->tm_min;
	fileInfoDetail.writeTime.second = writeTime->tm_sec;

	fileInfoDetail.size = statBuffer.st_size;

	return true;

#elif defined NEXGEN_LINUX

	struct stat statBuffer;
	if (stat(StringUtility::ToString(path).c_str(), &statBuffer) != 0)
	{
		return false;
	}

	fileInfoDetail.path = path;
	fileInfoDetail.isDirectory = S_ISDIR(statBuffer.st_mode);
	fileInfoDetail.isFile = !S_ISDIR(statBuffer.st_mode);

	struct tm *accessTime = localtime(&statBuffer.st_atim.tv_sec);

	fileInfoDetail.accessTime.month = accessTime->tm_mon + 1;
	fileInfoDetail.accessTime.day = accessTime->tm_mday;
	fileInfoDetail.accessTime.year = accessTime->tm_year + 1900;
	fileInfoDetail.accessTime.hour = accessTime->tm_hour;
	fileInfoDetail.accessTime.minute = accessTime->tm_min;
	fileInfoDetail.accessTime.second = accessTime->tm_sec;

	struct tm *writeTime = localtime(&statBuffer.st_mtim.tv_sec);

	fileInfoDetail.writeTime.month = writeTime->tm_mon + 1;
	fileInfoDetail.writeTime.day = writeTime->tm_mday;
	fileInfoDetail.writeTime.year = writeTime->tm_year + 1900;
	fileInfoDetail.writeTime.hour = writeTime->tm_hour;
	fileInfoDetail.writeTime.minute = writeTime->tm_min;
	fileInfoDetail.writeTime.second = writeTime->tm_sec;

	fileInfoDetail.size = statBuffer.st_size;

	return true;

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
		if (findData.cFileName == "." || findData.cFileName == "..")
		{
			continue;
		}
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

#elif defined NEXGEN_WIN || defined NEXGEN_UWP

	WIN32_FIND_DATAW findData;
	HANDLE findHandle = FindFirstFileW(CombinePath(path, L"*").c_str(), &findData);
	if (findHandle == INVALID_HANDLE_VALUE) 
	{     
		return false;
	} 
	do 
	{ 
		if (std::wstring(findData.cFileName) == L"." || std::wstring(findData.cFileName) == L"..")
		{
			continue;
		}
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
	if (entry == NULL) 
	{
		return false;
	}
	while (entry != NULL)
	{
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
		{
			entry = readdir(dir);
			continue;
		}
		FileInfoDetail fileInfoDetail;
		std::wstring currentPath = CombinePath(path, StringUtility::ToWideString(entry->d_name));
		if (FileGetFileInfoDetail(currentPath, fileInfoDetail) == false)
		{
			closedir(dir);
			return false;
		}
		fileInfoDetails.push_back(fileInfoDetail);
		entry = readdir(dir);
	}
	closedir(dir);
	return true;

#else

	return false;

#endif
}

bool FileSystem::FileOpen(std::wstring const path, FileMode const fileMode, uint32_t& fileHandle)
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
	FileContainer fileContainer;
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_MAC || defined NEXGEN_LINUX
	fileContainer.file = fopen(StringUtility::ToString(path).c_str(), StringUtility::ToString(access).c_str());
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
	fopen_s((FILE **)&fileContainer.file, StringUtility::ToString(path).c_str(), StringUtility::ToString(access).c_str());
#endif
	if (fileContainer.file == NULL)
	{
		return false;
	}
	fileHandle = AddFileContainer(fileContainer);
    return true;
}

bool FileSystem::FileRead(uint32_t fileHandle, char* readBuffer, uint32_t const bytesToRead, uint32_t& bytesRead)
{
	FileContainer* fileContainer = GetFileContainer(fileHandle);
	if (fileContainer == NULL) {
		return false;
	}
	bytesRead = (uint32_t)fread(readBuffer, 1, bytesToRead, (FILE*)fileContainer->file);
	return true;
}

bool FileSystem::FileReadAllAsString(std::wstring const path, std::string* buffer)
{
	FILE *file;
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_MAC || defined NEXGEN_LINUX
	file = fopen(StringUtility::ToString(path).c_str(), "rb");
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
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

bool FileSystem::FileWrite(uint32_t fileHandle, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten)
{
	FileContainer* fileContainer = GetFileContainer(fileHandle);
	if (fileContainer == NULL) {
		return false;
	}
	bytesWritten = (uint32_t)fwrite(writeBuffer, 1, bytesToWrite, (FILE*)fileContainer->file);
	return bytesWritten == bytesToWrite;
}

bool FileSystem::FileClose(uint32_t fileHandle)
{
	FileContainer* fileContainer = GetFileContainer(fileHandle);
	if (fileContainer == NULL) {
		return false;
	}
	bool result = fclose((FILE*)fileContainer->file) == 0;
	DeleteFileContainer(fileHandle);
	return result;
}

bool FileSystem::FileSeek(uint32_t fileHandle, FileSeekMode const fileSeekMode, uint32_t const offset)
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
	FileContainer* fileContainer = GetFileContainer(fileHandle);
	if (fileContainer == NULL) {
		return false;
	}
	FILE *file = (FILE*)fileContainer->file;
	fseek(file, offset, seek);
	return true;
}

bool FileSystem::FilePosition(uint32_t fileHandle, uint32_t& position)
{
	FileContainer* fileContainer = GetFileContainer(fileHandle);
	if (fileContainer == NULL) {
		return false;
	}
	FILE *file = (FILE*)fileContainer->file;
	position = ftell(file);
	return true;
}

bool FileSystem::DirectoryCreate(const std::wstring path)
{	
	bool exists;
	if (DirectoryExists(path, exists) == false) 
	{
		return false;
	}
	if (exists) 
	{
		return true;
	}
#if defined NEXGEN_OG || defined NEXGEN_360
    return CreateDirectoryA(StringUtility::ToString(path).c_str(), NULL) == TRUE;
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
	return CreateDirectoryW(path.c_str(), NULL) == TRUE;
#elif defined NEXGEN_MAC || defined NEXGEN_LINUX
	return mkdir(StringUtility::ToString(path).c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0;
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
			if (recursive && DirectoryDelete(directoryToDelete, true) == false) 
			{
				return false;
			}
		}
		else
		{
			std::wstring fileToDelete = fileInfoDetail.path;
			if (FileDelete(fileToDelete) == false)
			{
				return false;
			}
		}
	}

#if defined NEXGEN_OG || defined NEXGEN_360
    return RemoveDirectoryA(StringUtility::ToString(path).c_str()) == TRUE;
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
	return RemoveDirectoryW(path.c_str()) == TRUE;
#elif defined NEXGEN_MAC || defined NEXGEN_LINUX
	return rmdir(StringUtility::ToString(path).c_str()) == 0;
#else
	return false;
#endif
}

bool FileSystem::FileDelete(std::wstring const path)
{
	return remove(StringUtility::ToString(path).c_str()) == 0;
}

bool FileSystem::FileCopy(std::wstring const sourcePath, std::wstring const destPath)
{
	uint32_t sourceFileHandle;
	FileOpen(sourcePath, FileModeRead, sourceFileHandle);
	
	uint32_t destFileHandle;
	FileOpen(destPath, FileModeWrite, destFileHandle);
	
	char *buffer = (char*)malloc(32758);

	uint32_t bytesRead;
	if (FileRead(sourceFileHandle, buffer, 32768, bytesRead) == false)
	{
		FileClose(sourceFileHandle);
		FileClose(destFileHandle);
		return false;
	}
	
	while (bytesRead > 0)
	{
		uint32_t bytesWritten;
		if (FileWrite(destFileHandle, buffer, bytesRead, bytesWritten) == false || bytesWritten != bytesRead)
		{
			FileClose(sourceFileHandle);
			FileClose(destFileHandle);
			return false;
		}
		if (FileRead(sourceFileHandle, buffer, 32768, bytesRead) == false)
		{
			FileClose(sourceFileHandle);
			FileClose(destFileHandle);
			return false;
		}
	}
	
	free(buffer);
	
	FileClose(sourceFileHandle);
	FileClose(destFileHandle);
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
	if (FileSystem::DirectoryCreate(destPath) == false)
	{
		return false;
	}
	
	std::vector<std::wstring> directoriesToCopy;
	directoriesToCopy.push_back(sourcePath);
	while (directoriesToCopy.size() > 0)
	{
		std::wstring directoryToCopy;
		directoryToCopy = directoriesToCopy.front();
		directoriesToCopy.erase(directoriesToCopy.begin());

		std::wstring directoryToCreate = directoryToCopy.substr(sourcePath.length());
		std::wstring tempDestPath = FileSystem::CombinePath(destPath, directoryToCreate);
		if (FileSystem::DirectoryCreate(tempDestPath) == false)
		{
			return false;
		}

		std::vector<FileInfoDetail> fileInfoDetails;
		if (!FileSystem::FileGetFileInfoDetails(directoryToCopy, fileInfoDetails))
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
			else if (fileInfoDetail.isFile) 
			{
				std::wstring fileToCopy = FileSystem::GetFileName(fileInfoDetail.path);
				std::wstring destFile = FileSystem::CombinePath(tempDestPath, fileToCopy);
				if (!FileSystem::FileCopy(fileInfoDetail.path, destFile))
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool FileSystem::FileSize(uint32_t fileHandle, uint32_t& size)
{
	FileContainer* fileContainer = GetFileContainer(fileHandle);
	if (fileContainer == NULL) {
		return false;
	}
	FILE* file = (FILE*)fileContainer->file;
	uint32_t filePosition = ftell(file);
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, filePosition, SEEK_SET);
    return true;
}

bool FileSystem::FileExists(std::wstring const path, bool& exists)
{
	FileSystem::FileInfoDetail fileInfoDetail;
	if (FileSystem::FileGetFileInfoDetail(path, fileInfoDetail) == false) 
	{
		exists = false;
		return true;
	}
	exists = fileInfoDetail.isFile;
	return true;
}

bool FileSystem::DirectoryExists(std::wstring const path, bool& exists)
{
	FileSystem::FileInfoDetail fileInfoDetail;
	if (FileSystem::FileGetFileInfoDetail(path, fileInfoDetail) == false) 
	{
		exists = false;
		return true;
	}
	exists = fileInfoDetail.isDirectory;
	return true;
}

std::wstring FileSystem::GetFileName(std::wstring const path)
{
	const std::size_t found = path.find_last_of(GetPathSeparator());
	return found == std::wstring::npos ? L"" : path.substr(found + 1);
}

std::wstring FileSystem::GetExtension(std::wstring const path)
{
	std::wstring fileName = GetFileName(path);
	const std::size_t found = fileName.find_last_of(L'.');
	return found == std::wstring::npos ? L"" : fileName.substr(found);
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
	appDirectory = DriveManager::MapSystemPath(GetDirectory(StringUtility::ToWideString(std::string(&buffer[0], temp->Length))));	
	return true;
#elif defined NEXGEN_360
	appDirectory = L"Game:";
	return true;
#elif defined NEXGEN_WIN || defined NEXGEN_UWP
	wchar_t buffer[260];
	const size_t length = GetModuleFileNameW(0, &buffer[0], 260);
	std::wstring exeFilePath = std::wstring(buffer, length);
	appDirectory = GetDirectory(exeFilePath);
	return true;	
#elif defined NEXGEN_MAC 
	char result[PATH_MAX];
	getcwd(result, PATH_MAX);
  	appDirectory = StringUtility::ToWideString(std::string(result));
	return true;
#elif defined NEXGEN_LINUX
  	char result[PATH_MAX];
  	ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX );
  	appDirectory = GetDirectory(StringUtility::ToWideString(std::string(result, (count > 0) ? count : 0 )));
	return true;
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
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_WIN || defined NEXGEN_UWP
	return L'\\';
#else defined NEXGEN_MAC || defined NEXGEN_LINUX
	return L'/';
#endif
}

std::wstring FileSystem::CombinePath(std::wstring const first, std::wstring const second)
{
	if (second.length() == 0)
	{
		return first;
	}
	return StringUtility::RightTrim(first, GetPathSeparator()) + GetPathSeparator() + StringUtility::LeftTrim(second, GetPathSeparator());
}