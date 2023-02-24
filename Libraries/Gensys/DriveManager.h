#pragma once

#include "Drive.h"
#include "Int.h"

#include <string>
#include <vector>

namespace Gensys
{
	class DriveManager
	{
	public:
		static void Refresh();
		static std::vector<std::wstring> GetAllMountPoints();
		static std::vector<std::wstring> GetAllSystemPaths();
		static std::wstring GetSystemPath(std::wstring mountPoint);
		static std::wstring GetMountPoint(std::wstring systemPath);
		static bool IsMounted(std::wstring mountPoint);
		static long GetTotalNumberOfBytes(std::wstring mountPoint);
		static long GetFreeNumberOfBytes(std::wstring mountPoint);
		static long GetVolumeSerialNumber(std::wstring mountPoint);		
		static bool GetMountedDrives(std::vector<std::wstring>& drives);
	private:
		static void Init();
	};
}