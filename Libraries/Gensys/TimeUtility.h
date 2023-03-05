#pragma once

#if defined UWP_ANGLE || defined NEXGEN_WIN
#include <WinSock2.h>
#include <Windows.h>
#elif defined NEXGEN_360 || defined NEXGEN_OG
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
		static void Test();
	};
}
