#include "TimeUtility.h"
#include <cstring>
#include <chrono>

using namespace Gensys;

tm TimeUtility::GetTimeInfo(time_t time)
{
	tm* timeInfo = gmtime(&time);
	tm timeInfoCopy;
	memcpy(&timeInfoCopy, timeInfo, sizeof(timeInfoCopy));
	return timeInfoCopy;
}

tm TimeUtility::GetNow()
{
	time_t now;
	time(&now);
	struct tm* timeInfo = gmtime(&now);
	tm timeInfoCopy;
	memcpy(&timeInfoCopy, timeInfo, sizeof(timeInfoCopy));
	return timeInfoCopy;
}

void TimeUtility::Test()
{
	std::chrono::steady_clock::time_point timePoint = std::chrono::steady_clock::now();
}