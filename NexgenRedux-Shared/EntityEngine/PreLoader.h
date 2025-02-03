#pragma once

#include <string>

namespace NexgenRedux
{
    class PreLoader
    {
    public:
        static void Close(void);
        static bool LoadTexture(const std::string value);
		static bool UnLoadTexture(const std::string value);
		static bool AllTexturesLoaded();
    };
}