#include "FileSystem.h"
#include "StringUtility.h"
#include "XboxInternals.h"
#include "DriveManager.h"
#include <time.h>
#include <sys/stat.h>

using namespace Gensys;

#if defined XBOX_OG || defined XBOX_360 || defined UWP_ANGLE || defined WIN_ANGLE

time_t FileTimeToTime(FILETIME fileTime)
{
	const uint64_t WINDOWS_TICK = 10000000ULL;
	const uint64_t SEC_TO_UNIX_EPOCH = 11644473600ULL;

	uint64_t windowsTicks = ((uint64_t)fileTime.dwHighDateTime << 32) | fileTime.dwLowDateTime;
	return (time_t)(windowsTicks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
}

#endif

bool FileSystem::FileGetFileInfoDetails(std::wstring const path, std::vector<FileInfoDetail>& fileInfoDetails)
{
#if defined XBOX_OG || defined XBOX_360 || defined UWP_ANGLE || defined WIN_ANGLE
	HANDLE handle;
	WIN32_FIND_DATA findData;
	
	std::wstring searchPath = StringUtility::RightTrim(path, GetPathSeparator()) + GetPathSeparator();
	#if defined XBOX_OG
	handle = FindFirstFile(StringUtility::ToString(searchPath + L'*').c_str(), &findData);
	#else
	handle = FindFirstFile((searchPath + L'*').c_str(), &findData);
	#endif
	if (handle == INVALID_HANDLE_VALUE) 
	{     
		return false;
	} 
	do 
	{ 
		FileInfoDetail fileInfoDetail;
		fileInfoDetail.isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		fileInfoDetail.isNormal = (findData.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) != 0;
		fileInfoDetail.name = StringUtility::ToWideString(findData.cFileName);
		fileInfoDetail.path = searchPath;
		fileInfoDetail.size = findData.nFileSizeLow;
		fileInfoDetail.lastAccessTime = FileTimeToTime(findData.ftLastAccessTime);
		fileInfoDetail.lastWriteTime = FileTimeToTime(findData.ftLastWriteTime);
		fileInfoDetails.push_back(fileInfoDetail);			
	} 
	while(FindNextFile(handle, &findData)); 
	FindClose(handle);
#else
	//DIR *dir;
	//struct dirent *ent;
	//if ((dir = opendir ("c:\\src\\")) != NULL) {
	///* print all the files and directories within directory */
	//while ((ent = readdir (dir)) != NULL) {
	//	printf ("%s\n", ent->d_name);
	//}
	//closedir(dir);
#endif
	return true;
}

bool FileSystem::FileOpen(std::wstring const path, FileMode const fileMode, FileInfo& fileInfo)
{
	std::wstring access = L"";
	if (fileMode == FileMode::Read) {
		access = L"r";
	}
	else if (fileMode == FileMode::Write) {
		access = L"w";
	}
	else if (fileMode == FileMode::Append) {
		access = L"a";
	}
	else if (fileMode == FileMode::ReadUpdate) {
		access = L"r+";
	}
	else if (fileMode == FileMode::WriteUpdate) {
		access = L"w+";
	}
	else if (fileMode == FileMode::AppendUpdate) {
		access = L"a+";
	}
	access = access + L"b";
	fileInfo.file = _wfopen(path.c_str(), access.c_str());
    return fileInfo.file != NULL;
}

bool FileSystem::FileRead(FileInfo const fileInfo, char* readBuffer, uint32_t const bytesToRead, uint32_t& bytesRead)
{
	bytesRead = fread(readBuffer, bytesToRead, 1, (FILE*)fileInfo.file);
	return true;
}

bool FileSystem::FileReadAllAsString(std::wstring const path, std::string* buffer)
{
	FILE *file = _wfopen(path.c_str(), L"rb");
	if(file == NULL)
	{
		return false;
	}

	fseek(file, 0, SEEK_END);
	uint32_t fileLength = ftell(file);
	fseek(file, 0, SEEK_SET);

	buffer->resize(fileLength);
	uint32_t bytesRead = fread((void*)buffer->data(), 1, fileLength, file);
	fclose(file);

	return bytesRead == fileLength;
}

bool FileSystem::FileWrite(FileInfo const fileInfo, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten)
{
	bytesWritten = fwrite(writeBuffer, 1, bytesToWrite, (FILE*)fileInfo.file);
	return bytesWritten == bytesToWrite;
}

bool FileSystem::FileClose(FileInfo const fileInfo)
{
	return fclose((FILE*)fileInfo.file) == 0;
}

bool FileSystem::FileSeek(FileInfo const fileInfo, FileSeekMode const fileSeekMode, uint32_t const offset)
{
	int seek = SEEK_CUR;
	if (fileSeekMode == FileSeekMode::Start)
	{
		seek = SEEK_SET;
	}
	else if (fileSeekMode == FileSeekMode::End)
	{
		seek = SEEK_END;
	}
	FILE *file = (FILE*)fileInfo.file;
	fseek(file, 0, fileSeekMode);
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
#if defined XBOX_OG || defined XBOX_360
    return CreateDirectoryA(StringUtility::ToString(path).c_str(), NULL) == TRUE;
#elif defined UWP_ANGLE || defined WIN_ANGLE
	return CreateDirectoryW(path.c_str(), NULL) == TRUE;
#else
	return mkdir(StringUtility::ToString(path).c_str(), 0777) == 0;
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
			std::wstring directoryToDelete = FileSystem::CombinePath(path, fileInfoDetail.name);
			if (recursive && !DirectoryDelete(directoryToDelete, true)) 
			{
				return false;
			}
		}
		else
		{
			std::wstring fileToDelete = FileSystem::CombinePath(path, fileInfoDetail.name);
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
	return _wremove(path.c_str()) == 0;
}

bool FileSystem::FileCopy(std::wstring const sourcePath, std::wstring const destPath)
{
	FileInfo sourceFileInfo;
	FileOpen(sourcePath, FileMode::Read, sourceFileInfo);
	
	FileInfo destFileInfo;
	FileOpen(destPath, FileMode::Write, destFileInfo);
	
	char *buffer = (char*)malloc(32758);
	uint32_t bytesRead = fread(buffer, 1, 32768, (FILE*)sourceFileInfo.file);
	while (bytesRead > 0)
	{
		fwrite(buffer, 1, bytesRead, (FILE*)destFileInfo.file);
		bytesRead = fread(buffer, 1, 32768, (FILE*)sourceFileInfo.file);
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
	std::wstring directory = GetFile(sourcePath);
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
				std::wstring directoryToAdd = FileSystem::CombinePath(directoryToCopy, fileInfoDetail.name);
				directoriesToCopy.push_back(directoryToAdd);
			}
			else
			{
				std::wstring fileToCopy = FileSystem::CombinePath(directoryToCopy, fileInfoDetail.name);
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
	struct stat statBuffer;
	if (stat(StringUtility::ToString(path).c_str(), &statBuffer) != 0)
	{
		return false;
	}
    exists = (statBuffer.st_mode & S_IFMT) == S_IFREG;
	return true;
}

bool FileSystem::DirectoryExists(std::wstring const path, bool& exists)
{
	struct stat statBuffer;
	if (stat(StringUtility::ToString(path).c_str(), &statBuffer) != 0)
	{
		return false;
	}
    exists = (statBuffer.st_mode & S_IFMT) == S_IFDIR;
	return true;
}

bool FileSystem::GetDriveLetters(std::vector<std::wstring>& driveLetters)
{
#if defined XBOX_OG || defined XBOX_360

	return false;
	
#elif defined UWP_ANGLE || defined WIN_ANGLE 

	char buffer[2];
	buffer[1] = 0;
	
	driveLetters.clear();
	DWORD driveFlags = GetLogicalDrives();
	for (char i = 0; i < 26; i++)
	{
		const char letterA = 65;
		char driveLetter = letterA + i;
		DWORD testFlag = 1 << i;
		if ((driveFlags & testFlag) > 0)
		{
			buffer[0] = driveLetter;
			driveLetters.push_back(StringUtility::ToWideString(std::string(buffer)));
		}
	}
	return true;

#elif defined MAC_ANGLE
    return false;
#else
    return false;
#endif
}
std::wstring FileSystem::MapSystemPath(std::wstring const path)
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

std::wstring FileSystem::GetFile(std::wstring const path)
{
	const std::size_t found = path.find_last_of(L'\\');
	return found == std::wstring::npos ? L"" : path.substr(found + 1);
}

std::wstring FileSystem::GetDirectory(std::wstring const path)
{
	const std::size_t found = path.find_last_of(L'\\');
	return found == std::wstring::npos ? path : path.substr(0, found);
}

std::wstring FileSystem::GetAppDirectory()
{
#if defined XBOX_OG

	char buffer[260];
	STRING *temp = (STRING*)XeImageFileName;
	sprintf(buffer, temp->Buffer);
	std::wstring path = GetDirectory(StringUtility::ToWideString(std::string(&buffer[0], temp->Length)));	
	std::wstring result = MapSystemPath(path);
	return result;

#elif defined XBOX_360
	return L"Game:";
#elif defined UWP_ANGLE || defined WIN_ANGLE
	wchar_t buffer[260];
	const size_t length = GetModuleFileNameW(0, &buffer[0], 260);
	std::wstring exeFilePath = std::wstring(buffer, length);
	std::wstring result = GetDirectory(exeFilePath);
	return result;	
#elif defined MAC_ANGLE
    return L"";
#else
    return L"";
#endif
}

std::wstring FileSystem::GetMediaDirectory()
{
#if defined UWP_ANGLE
	std::wstring dataPath = Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data();
	std::wstring result = CombinePath(dataPath, L"Media");
	return result;
#else
	std::wstring result = CombinePath(GetAppDirectory(), L"Media");
	return result;
#endif
}

wchar_t FileSystem::GetPathSeparator()
{
#if defined XBOX_OG || defined XBOX_360 || defined UWP_ANGLE || defined WIN_ANGLE
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

std::wstring FileSystem::GetDriveLetter(std::wstring const path)
{
	if (path.length() > 1 && path.at(1) == L':') {
		return path.substr(0, 1);
	}
	return L"";
}

std::wstring FileSystem::ReplaceDriveLetter(std::wstring const path, std::wstring const driveLetter)
{
	if (path.length() > 1 && path.at(1) == L':') {
		return driveLetter + path.substr(1);
	}
	return L"";
}

