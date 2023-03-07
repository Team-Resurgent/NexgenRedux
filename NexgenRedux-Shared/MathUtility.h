#pragma once

#include <Gensys/Int.h>

namespace NexgenRedux
{
	class MathUtility
	{		
	public:

		typedef struct Vec2D
		{
			double x;
			double y;
		} Vec2D;

		typedef struct Size
		{
			uint32_t width;
			uint32_t height;
		} Size;

		static void SeedRandom();
		static void SeedRandomWithValue(uint32_t value);
		static double GetRandomDouble();
		static int32_t GetRandomIntInRange(int32_t start, int32_t end);
	};
}
