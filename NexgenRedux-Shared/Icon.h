#pragma once

#include <Gensys/Int.h>

namespace NexgenRedux
{
	class Icon
	{		
	public:
    	static uint32_t Width(void);
		static uint32_t Height(void);
        static uint32_t DataSize(void);
		static void Populate(unsigned char *bufffer);
	};
}
