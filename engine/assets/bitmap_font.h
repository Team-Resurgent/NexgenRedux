#pragma once

#include "assets/assets.h"

// Fonts drawn beforehand as pictures. The glyphs' sizes are in the picture's pixels, and scale is left at 1.
namespace nx
{
    // A BMFont .fnt file, as text: its glyphs, and the name of its one picture, beside it. False if it isn't one, or
    // has more than one picture.
    bool parseBitmapFont(const char* text, size_t size, FontData& font, char* page, size_t pageSize, u32& width, u32& height);
    // A strip of glyphs, in the order given (UTF-8), with columns of the top left pixel's colour between them, which
    // are made see-through. spacing is added after each.
    bool cutImageFont(u8* rgba, u32 width, u32 height, const char* glyphs, u32 spacing, FontData& font);
}
