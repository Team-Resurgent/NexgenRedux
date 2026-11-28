#include "assets/decode.h"
#include "assets/third_party.h"
#include "base/endian.h"
#include "base/log.h"

#include <string.h>

// The third-party libraries allocate through these, so their memory is counted too.
extern "C" void* nxDecodeRealloc(void* old, size_t oldSize, size_t size)
{
    void* memory = size ? nx::memAlloc(size, nx::MemTag::Texture) : nullptr;
    if (old && memory)
    {
        memcpy(memory, old, oldSize < size ? oldSize : size);
    }
    if (old && (memory || size == 0))
    {
        nx::memFree(old);
    }
    return memory;
}

extern "C" void nxDecodeFree(void* memory)
{
    nx::memFree(memory);
}

namespace nx
{
    bool imageSize(const u8* data, size_t size, u32& width, u32& height)
    {
        int w = 0, h = 0, channels = 0;
        if (size == 0 || size > 0x7FFFFFFF || !nx_stbi_info(data, static_cast<int>(size), &w, &h, &channels) || w <= 0 || h <= 0)
        {
            return false;
        }
        width = static_cast<u32>(w);
        height = static_cast<u32>(h);
        return true;
    }

    u8* decodeImage(const u8* data, size_t size, u32& width, u32& height)
    {
        int w = 0, h = 0, channels = 0;
        u8* pixels = size <= 0x7FFFFFFF ? nx_stbi_load(data, static_cast<int>(size), &w, &h, &channels, 4) : nullptr;
        width = static_cast<u32>(w);
        height = static_cast<u32>(h);
        return pixels;
    }

    void freeImage(u8* pixels)
    {
        nxDecodeFree(pixels);
    }

    u8* encodePng(const u8* rgba, u32 width, u32 height, size_t& size)
    {
        int written = 0;
        u8* png = rgba && width && height && width <= 8192 && height <= 8192 ? nx_png_write(rgba, static_cast<int>(width), static_cast<int>(height), &written) : nullptr;
        size = png ? static_cast<size_t>(written) : 0;
        return png;
    }

    u8* compressBytes(const u8* data, size_t size, size_t& outSize, int level)
    {
        int written = 0;
        u8* out = size <= 0x3FFFFFFF ? nx_zlib_compress(data, static_cast<int>(size), &written, level < 1 ? 1 : level > 9 ? 9 : level) : nullptr;
        outSize = out ? static_cast<size_t>(written) : 0;
        return out;
    }

    u8* uncompressBytes(const u8* data, size_t size, size_t& outSize)
    {
        int written = 0;
        u8* out = data && size && size <= 0x3FFFFFFF ? nx_zlib_uncompress(data, static_cast<int>(size), &written) : nullptr;
        outSize = out ? static_cast<size_t>(written) : 0;
        return out;
    }

    u8* deflateBytes(const u8* data, size_t size, size_t& outSize, int level)
    {
        // zlib's format is two bytes, the deflate, and four of its check after.
        size_t zlib = 0;
        u8* out = compressBytes(data, size, zlib, level);
        if (out == nullptr || zlib < 6)
        {
            freeImage(out);
            outSize = 0;
            return nullptr;
        }
        memmove(out, out + 2, zlib - 6);
        outSize = zlib - 6;
        return out;
    }

    u8* inflateBytes(const u8* data, size_t size, size_t& outSize)
    {
        int written = 0;
        u8* out = data && size && size <= 0x3FFFFFFF ? nx_inflate_raw(data, static_cast<int>(size), &written) : nullptr;
        outSize = out ? static_cast<size_t>(written) : 0;
        return out;
    }

    u32 crc32Of(const u8* data, size_t size, u32 crc)
    {
        struct Table
        {
            u32 entries[256];
            Table()
            {
                for (u32 i = 0; i < 256; i++)
                {
                    u32 c = i;
                    for (int k = 0; k < 8; k++)
                    {
                        c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
                    }
                    entries[i] = c;
                }
            }
        };
        static const Table table;  // made once, safely, by whichever thread is first
        crc = ~crc;
        for (size_t i = 0; i < size; i++)
        {
            crc = table.entries[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        }
        return ~crc;
    }

    bool renderFont(const u8* data, size_t size, u32 pixels, FontData& font, FontImage& image)
    {
        NxFont* source = nx_font_open(data, size, static_cast<int>(pixels));
        if (source == nullptr)
        {
            return false;
        }

        // Every glyph gets the same cell, big enough for the largest.
        const u32 cell = pixels * 2 + 4;
        u8* scratch = static_cast<u8*>(memAlloc(static_cast<size_t>(cell) * cell * 4, MemTag::UI));
        bool ok = scratch != nullptr;

        // Glyphs go in rows, left to right. Start small and grow until they fit.
        u32 width = 128;
        u32 height = 128;
        while (ok && image.pixels == nullptr)
        {
            u8* atlas = static_cast<u8*>(memAlloc(static_cast<size_t>(width) * height * 4, MemTag::UI));
            if (atlas == nullptr)
            {
                ok = false;
                break;
            }
            // White everywhere, so filtering at the edges never picks up another colour.
            for (size_t i = 0; i < static_cast<size_t>(width) * height; i++)
            {
                atlas[i * 4 + 0] = atlas[i * 4 + 1] = atlas[i * 4 + 2] = 255;
                atlas[i * 4 + 3] = 0;
            }

            u32 x = 2, y = 2, rowHeight = 0;
            float ascent = 0, descent = 0;
            bool fits = true;
            for (u32 i = 0; i < kGlyphCount && fits; i++)
            {
                const u32 code = kFirstGlyph + i;
                char text[3] = { static_cast<char>(code), 0, 0 };
                if (code >= 0x80)
                {
                    text[0] = static_cast<char>(0xC0 | (code >> 6));
                    text[1] = static_cast<char>(0x80 | (code & 0x3F));
                }
                Glyph& g = font.glyphs[i];
                g = Glyph();

                memset(scratch, 0, static_cast<size_t>(cell) * cell * 4);
                const int penX = static_cast<int>(cell / 4);
                const int penY = static_cast<int>(cell * 5 / 8);
                int advance = 0;
                if (!nx_font_glyph(source, text, scratch, static_cast<int>(cell), penX, penY, &advance))
                {
                    continue;
                }
                g.advance = static_cast<float>(advance);

                // Only the part that has ink.
                u32 left = cell, top = cell, right = 0, bottom = 0;
                for (u32 py = 0; py < cell; py++)
                {
                    for (u32 px = 0; px < cell; px++)
                    {
                        if (scratch[(static_cast<size_t>(py) * cell + px) * 4 + 3])
                        {
                            left = px < left ? px : left;
                            right = px + 1 > right ? px + 1 : right;
                            top = py < top ? py : top;
                            bottom = py + 1 > bottom ? py + 1 : bottom;
                        }
                    }
                }
                if (right <= left || bottom <= top)
                {
                    continue;
                }
                const u32 w = right - left;
                const u32 h = bottom - top;
                if (x + w + 2 > width)
                {
                    x = 2;
                    y += rowHeight + 2;
                    rowHeight = 0;
                }
                if (x + w + 2 > width || y + h + 2 > height)
                {
                    fits = false;
                    break;
                }
                for (u32 py = 0; py < h; py++)
                {
                    for (u32 px = 0; px < w; px++)
                    {
                        atlas[(static_cast<size_t>(y + py) * width + x + px) * 4 + 3] = scratch[(static_cast<size_t>(top + py) * cell + left + px) * 4 + 3];
                    }
                }
                // With a clear pixel all round: drawn smaller, an i or an l could otherwise fall between two of
                // the screen's pixels and not be seen at all.
                g.x = static_cast<float>(static_cast<int>(left) - penX - 1);
                g.y = static_cast<float>(static_cast<int>(top) - penY - 1);
                g.width = static_cast<float>(w + 2);
                g.height = static_cast<float>(h + 2);
                g.u0 = static_cast<float>(x - 1) / static_cast<float>(width);
                g.v0 = static_cast<float>(y - 1) / static_cast<float>(height);
                g.u1 = static_cast<float>(x + w + 1) / static_cast<float>(width);
                g.v1 = static_cast<float>(y + h + 1) / static_cast<float>(height);
                if (code < 0x80)
                {
                    const float up = static_cast<float>(penY) - static_cast<float>(top), down = static_cast<float>(bottom) - static_cast<float>(penY);
                    ascent = up > ascent ? up : ascent;
                    descent = down > descent ? down : descent;
                }
                x += w + 2;
                rowHeight = h > rowHeight ? h : rowHeight;
            }

            if (fits)
            {
                image.pixels = atlas;
                image.width = width;
                image.height = height;
                font.ascent = ascent;
                font.lineHeight = (ascent + descent) * 1.15f;
            }
            else
            {
                memFree(atlas);
                (width <= height ? width : height) *= 2;
                ok = width <= gpu::kMaxTextureSize && height <= gpu::kMaxTextureSize;
            }
        }
        memFree(scratch);
        nx_font_close(source);
        return ok && image.pixels != nullptr && font.lineHeight > 0;
    }
}
