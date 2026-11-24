#pragma once

#include "assets/assets.h"

namespace nx
{
    // Reads only the header, so a huge image can be refused before anything is allocated for it.
    bool imageSize(const u8* data, size_t size, u32& width, u32& height);

    // RGBA8, top row first. nullptr if it can't be decoded. Free with freeImage.
    u8* decodeImage(const u8* data, size_t size, u32& width, u32& height);
    void freeImage(u8* pixels);

    struct FontImage
    {
        u8* pixels = nullptr;  // RGBA8, white with the shape in alpha; free with memFree
        u32 width = 0, height = 0;
    };

    // Draws every glyph of an SSFN font (gzipped or not) at a pixel size into one image.
    bool renderFont(const u8* data, size_t size, u32 pixels, FontData& font, FontImage& image);
}
