#pragma once

#if defined UWP_ANGLE || defined WIN_ANGLE
#include <WinSock2.h>
#include <Windows.h>
#elif defined XBOX_360 || defined XBOX_OG
#include <xtl.h>
#include <winsockx.h>
#endif	
#include <ctime>

namespace Gensys
{
	class TimeUtility
	{
	public:
		
		static tm GetTimeInfo(time_t time);
		static tm GetNow();
	};
}
