#include "TimeUtility.h"
#include <ctime>
#include <time.h>

#if defined NEXGEN_WIN 
#include <Windows.h>
#elif defined NEXGEN_OG || defined NEXGEN_360
#include <Xtl.h>
#elif defined NEXGEN_MAC || defined NEXGEN_LINUX 
#include <unistd.h>
#endif

#include <Gensys/Int.h>

using namespace Gensys;

TimeUtility::Time TimeUtility::GetTimeNow()
{
	time_t now;
	time(&now);
	struct tm* localTime = localtime(&now);
	Time time;
	time.month = localTime->tm_mon + 1;
	time.day = localTime->tm_mday;
	time.year = localTime->tm_year + 1900;
	time.hour = localTime->tm_hour;
	time.minute = localTime->tm_min;
	time.second = localTime->tm_sec;
	return time;
}

uint64_t TimeUtility::GetMillisecondsNow()
{
#if defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_WIN
	double clockRsolution = (CLOCKS_PER_SEC / (double)1000);
	uint64_t nowMilliseconds = (uint64_t)(clock() / clockRsolution);
	return nowMilliseconds;
#elif defined NEXGEN_MAC || defined NEXGEN_LINUX
   	struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now); 
	return (now.tv_sec * 1000) + (now.tv_nsec / 1000000);
#endif
}

double TimeUtility::GetDurationSeconds(uint64_t start, uint64_t end)
{
	return (end - start) / (double)1000;
}

double TimeUtility::CalculateFramesPerSecond(uint32_t frameCount, uint64_t start, uint64_t end)
{
	return frameCount / ((end - start) / (double)1000);
}

void TimeUtility::SleepMilliseconds(uint32_t milliseconds)
{
#if defined NEXGEN_WIN || defined NEXGEN_OG || defined NEXGEN_360
	Sleep(milliseconds);
#elif defined NEXGEN_MAC || defined NEXGEN_LINUX
	usleep(milliseconds * 1000);
#endif
}