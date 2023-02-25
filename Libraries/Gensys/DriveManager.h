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
		static bool GetTotalNumberOfBytes(std::wstring mountPoint, uint64_t& totalSize);
		static bool GetTotalFreeNumberOfBytes(std::wstring mountPoint, uint64_t& totalFree);
		static bool GetVolumeSerialNumber(std::wstring mountPoint, uint32_t& serial);		
		static bool GetMountedDrives(std::vector<std::wstring>& drives);
		static std::wstring MapSystemPath(std::wstring const path);
	private:
		static void InitOrRefresh();
	};
}