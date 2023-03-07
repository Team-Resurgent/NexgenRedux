#include "MathUtility.h"

#include <ctime>

#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
#include <random>
#elif defined NEXGEN_OG || defined NEXGEN_360
#include <xtl.h>
#endif

using namespace NexgenRedux;

void MathUtility::SeedRandom()
{
    srand((uint32_t)time(NULL));
}

void MathUtility::SeedRandomWithValue(uint32_t value)
{
    srand(value);
}

double MathUtility::GetRandomDouble()
{
    return (double)rand() / RAND_MAX;
}

int32_t MathUtility::GetRandomIntInRange(int32_t start, int32_t end)
{
    int first = start > end ? end : start;
    int last =  start > end ? start : end;;
    return (rand() % (last - first)) + first;
}