#include "MathUtility.h"

#include <ctime>

#if defined NEXGEN_WIN || defined NEXGEN_UWP || defined NEXGEN_MAC || defined NEXGEN_LINUX 
#include <random>
#elif defined NEXGEN_OG || defined NEXGEN_360
#include <xtl.h>
#endif

using namespace NexgenRedux;

float MathUtility::DegreesToRadians(float const degrees)
{
    return degrees * (3.14159265358979323846f / 180.0f);
}

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
    int last =  start > end ? start : end;
    return (rand() % (last - first)) + first;
}

void MathUtility::ConvertEndian(uint32_t& value) 
{
    value = ((value << 24) & 0xFF000000) | ((value <<  8) & 0x00FF0000) | ((value >>  8) & 0x0000FF00) | ((value >> 24) & 0x000000FF);
}

void MathUtility::ConvertEndian(uint16_t& value) 
{
    value = ((value << 8) & 0xFF00) | ((value >> 8) & 0x00FF);
}

void MathUtility::ConvertEndian(float& value) 
{
    float retVal;
    char *floatToConvert = (char*)&value;
    char *returnFloat = (char*)&retVal;
    returnFloat[0] = floatToConvert[3];
    returnFloat[1] = floatToConvert[2];
    returnFloat[2] = floatToConvert[1];
    returnFloat[3] = floatToConvert[0];
    value = retVal;
}

bool MathUtility::IsPowerOf2(uint32_t value)
{
    return (value & (value - 1)) == 0;
}