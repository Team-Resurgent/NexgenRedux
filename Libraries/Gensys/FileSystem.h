#pragma once

#include "Int.h"

#include <vector>
#include <string>

namespace Gensys
{
	class FileSystem
	{
	public:

		typedef enum FileMode
		{
			FileModeRead = 0,
			FileModeWrite = 1,
			FileModeAppend = 2,
			FileModeReadUpdate = 3,
			FileModeWriteUpdate = 4,
			FileModeAppendUpdate = 5
		} FileMode;

		typedef enum FileSeekMode
		{
			FileSeekModeStart = 0,
			FileSeekModeEnd = 1,
			FileSeekModeCurrent = 2
		} FileSeekMode;

		typedef struct FileTime
		{
			uint16_t month;
			uint16_t day;
			uint16_t year;
			uint16_t hour;
			uint16_t minute;
			uint16_t second;
		} FileTime;
        
		typedef struct FileInfoDetail
        {
			bool isDirectory;
			bool isFile;
			std::wstring path;
			uint32_t size;
			FileTime accessTime;
			FileTime writeTime;			
        } FileInfoDetail;
		
		static void Dispose(void);
		static bool FileGetFileInfoDetail(std::wstring const path, FileInfoDetail& fileInfoDetail);
		static bool FileGetFileInfoDetails(std::wstring const path, std::vector<FileInfoDetail>& fileInfoDetails);
		
		static bool FileOpen(std::wstring const path, FileMode const fileMode, uint32_t& fileHandle);
		static bool FileRead(uint32_t fileHandle, char* readBuffer, uint32_t const bytesToRead, uint32_t& bytesRead);
		static bool FileReadAllAsString(std::wstring const path, std::string* buffer);
		static bool FileReadAllAsData(std::wstring const path, std::vector<uint8_t>* buffer);
		static bool FileWrite(uint32_t fileHandle, char* writeBuffer, uint32_t bytesToWrite, uint32_t& bytesWritten);
		static bool FileClose(uint32_t fileHandle);
		static bool FileSeek(uint32_t fileHandle, FileSeekMode const fileSeekMode, uint32_t const offset);
		static bool FilePosition(uint32_t fileHandle, uint32_t& position);

		static bool DirectoryCreate(std::wstring const path);
		static bool DirectoryDelete(std::wstring const path, bool const recursive);
		static bool FileDelete(std::wstring const path);
		static bool FileCopy(std::wstring const sourcePath, std::wstring const destPath);
		static bool FileMove(std::wstring const sourcePath, std::wstring const destPath);
		static bool DirectoryCopy(std::wstring const sourcePath, std::wstring const destPath);
		static bool FileSize(uint32_t fileHandle, uint32_t& size);
		static bool FileExists(std::wstring const path, bool& exists);
		static bool DirectoryExists(std::wstring const path, bool& exists);

		static std::wstring GetFileName(std::wstring const path);
		static std::wstring GetExtension(std::wstring const path);
		static std::wstring GetDirectory(std::wstring const path);
		static bool GetAppDirectory(std::wstring& appDirectory);
		static bool GetMediaDirectory(std::wstring& mediaDirectory);
		static wchar_t GetPathSeparator();
		static std::wstring CombinePath(std::wstring const first, std::wstring const second);
	};
}
