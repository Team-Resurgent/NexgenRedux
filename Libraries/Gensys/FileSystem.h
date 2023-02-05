#pragma once

#if defined UWP_ANGLE || defined WIN_ANGLE
#include <WinSock2.h>
#include <Windows.h>
#elif defined XBOX_360 || defined XBOX_OG
#include <xtl.h>
#include <winsockx.h>
#elif defined MAC_ANGLE
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "Int.h"
#include <string>
#include <vector>
#include <time.h>

namespace Gensys
{
	class FileSystem
	{
	public:
		
		typedef struct FileInfo
		{
			void* file;
		} FileInfo;
        
		typedef struct FileInfoDetail
        {
			bool isDirectory;
			bool isNormal;
			std::wstring name;	
			std::wstring path;
			uint32_t size;
			tm lastAccessTime;
			tm lastWriteTime;			
        } FileInfoDetail;

		typedef enum FileMode
		{
			Read = 0,
			Write = 1,
			Append = 2,
			ReadUpdate = 3,
			WriteUpdate = 4,
			AppendUpdate = 5
		} FileMode;

		typedef enum FileSeekMode
		{
			Start = 0,
			End = 1,
			Current = 2
		} FileSeekMode;
		
		static bool FileGetFileInfoDetails(std::wstring const path, std::vector<FileInfoDetail>& fileInfoDetails);
		static bool FileOpen(std::wstring const path, FileMode const fileMode, FileInfo& fileInfo);
		static bool FileRead(FileInfo const fileInfo, char* readBuffer, uint32_t const bytesToRead, uint32_t& bytesRead);
		static bool FileReadAllAsString(std::wstring const path, std::string* buffer);
		static bool FileWrite(FileInfo const fileInfo, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten);
		static bool FileClose(FileInfo const fileInfo);
		static bool FileSeek(FileInfo const fileInfo, FileSeekMode const fileSeekMode, uint32_t const offset);
		static bool FilePosition(FileInfo const fileInfo, uint32_t& position);
		static bool DirectoryCreate(std::wstring const path);
		static bool DirectoryDelete(std::wstring const path, bool const recursive);
		static bool FileDelete(std::wstring const path);
		static bool FileCopy(std::wstring const sourcePath, std::wstring const destPath);
		static bool FileMove(std::wstring const sourcePath, std::wstring const destPath);
		static bool DirectoryCopy(std::wstring const sourcePath, std::wstring const destPath);
		static bool FileSize(FileInfo const fileInfo, uint32_t& size);
		static bool FileExists(std::wstring const path, bool& exists);
		static bool DirectoryExists(std::wstring const path, bool& exists);
		static bool GetMountedDrives(std::vector<std::wstring>& drives);

		//static std::wstring MapSystemPath(std::wstring const path);
		static std::wstring GetFileName(std::wstring const path);
		static std::wstring GetDirectoryName(std::wstring const path);
		static bool GetAppDirectory(std::wstring& appDirectory);
		static bool GetMediaDirectory(std::wstring& mediaDirectory);
		static wchar_t GetPathSeparator();
		static std::wstring CombinePath(std::wstring const first, std::wstring const second);
		static std::wstring GetExtension(std::wstring const path);
		//static std::wstring GetDriveLetter(std::wstring const path);
		//static std::wstring ReplaceDriveLetter(std::wstring const path, std::wstring const driveLetter);
	};
}
