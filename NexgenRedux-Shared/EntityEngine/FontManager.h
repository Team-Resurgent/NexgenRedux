#pragma once

#include <Gensys/Int.h>

#include <string>

namespace NexgenRedux
{
    class FontManager 
    {
    public:

        static void Close(void);
        static bool LoadFont(const std::string fontPath);
        static bool SelectFont(const std::string fontName, const uint32_t fontSize, const uint32_t fontstyle);

        static bool RenderText(const std::string text, void** textHandle);
        static bool GetTextPixelBuffer(void** textHandle, uint8_t** pixelBuffer, uint32_t& width, uint32_t& height);
        static void DeleteRenderText(void* textHandle);
    };
}