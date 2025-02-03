#pragma once

#include <string>

namespace NexgenRedux
{
    class PreLoader
    {
    public:
        static void Close(void);
        static void LoadTexture(const std::string value);
		static void UnLoadTexture(const std::string value);
		static void WaitTexturesLoaded();
    };
}