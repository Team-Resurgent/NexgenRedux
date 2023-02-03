#pragma once

#include <string>

#if defined UWP_ANGLE || defined WIN_ANGLE
#include <WinSock2.h>
#include <Windows.h>
#elif defined XBOX_360 || defined XBOX_OG
#include <xtl.h>
#include <winsockx.h>
#endif	

namespace Gensys
{
	class Drive
	{
	public:
		Drive(std::wstring mountPoint, std::wstring systemPath);
		bool Mount();  		
		bool Unmount();
		bool IsMounted();
		void RefreshDetails();
		std::wstring GetMountPoint();
		std::wstring GetSystemPath();
		long GetTotalNumberOfBytes();
		long GetFreeNumberOfBytes();
		long GetVolumeSerialNumber();
	protected:
		std::wstring m_mountPoint;
		std::wstring m_systemPath;
		long m_totalNumberOfBytes;
		long m_freeNumberOfBytes;
		long m_volumeSerialNumber;
	};
}