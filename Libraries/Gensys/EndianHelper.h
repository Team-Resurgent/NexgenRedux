#pragma once

#include "Int.h"

namespace Gensys
{
	namespace EndianHelper
	{
		
		static inline bool IsBigEndian()
		{
			union
			{
				uint32_t i;
				char c[4];
			} bint = { 0x01020304 };

			return (bint.c[0] == 1); 
		}

		static inline int16_t SwapInt16(int16_t val)
		{
			return (val << 8) | ((val >> 8) & 0xFF);
		}

		static inline int32_t SwapInt32(int32_t val)
		{
			val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF); 
			return (val << 16) | ((val >> 16) & 0xFFFF);
		}

		static inline int64_t SwapInt64(int64_t val)
		{
			val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF); 
			return (val << 16) | ((val >> 16) & 0xFFFF);
		}

		static inline uint16_t SwapUint16(uint16_t val)
		{
			return (val << 8) | (val >> 8);
		}

		static inline uint32_t SwapUint32(uint32_t val)
		{
			val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
			return (val << 16) | (val >> 16);
		}

		static inline uint64_t SwapUint64(uint64_t val)
		{
			val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
			return (val << 16) | (val >> 16);
		}

		static inline float SwapFloat(float val)
		{
			uint32_t *ptr = (uint32_t *)&val;
			*ptr = SwapUint32(*ptr);
			return val;
		}
		
	}
}
