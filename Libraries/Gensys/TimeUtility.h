#pragma once

#include "Int.h"

namespace Gensys
{
	class TimeUtility
	{
	public:

		typedef struct Time
		{
			uint16_t month;
			uint16_t day;
			uint16_t year;
			uint16_t hour;
			uint16_t minute;
			uint16_t second;
		} Time;
		
		static Time GetTimeNow();
		static uint64_t GetMillisecondsNow();
		static double GetDurationSeconds(uint64_t start, uint64_t end);
		static double CalculateFramesPerSecond(uint32_t frameCount, double duration);
		static void SleepMilliseconds(uint32_t milliseconds);
	};
}
