#include "assets/bitmap_font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fonts drawn beforehand as pictures: AngelCode's BMFont (.fnt, as text, which BMFont, Hiero, Littera and others write),
// and a strip of glyphs with a line of one colour between each, as LÖVE's image fonts are.
namespace nx
{
    namespace
    {
        // The number after key= on a line, or fallback.
        int field(const char* line, const char* end, const char* key, int fallback)
        {
            const size_t n = strlen(key);
            for (const char* p = line; p + n + 1 < end; p++)
            {
                if ((p == line || p[-1] == ' ' || p[-1] == '\t') && strncmp(p, key, n) == 0 && p[n] == '=')
                {
                    return atoi(p + n + 1);
                }
            }
            return fallback;
        }

        bool starts(const char* line, const char* end, const char* word)
        {
            const size_t n = strlen(word);
            return static_cast<size_t>(end - line) > n && strncmp(line, word, n) == 0 && (line[n] == ' ' || line[n] == '\t');
        }

        // Next character of UTF-8, as the canvas reads it.
        u32 nextChar(const char*& p)
        {
            const u8 first = static_cast<u8>(*p++);
            if (first < 0x80)
            {
                return first;
            }
            const u32 extra = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : first >= 0xC0 ? 1 : 0;
            u32 code = first & (0x3F >> extra);
            for (u32 i = 0; i < extra; i++)
            {
                if ((static_cast<u8>(*p) & 0xC0) != 0x80)
                {
                    return '?';
                }
                code = (code << 6) | (static_cast<u8>(*p++) & 0x3F);
            }
            return extra ? code : '?';
        }

        void clearGlyphs(FontData& font)
        {
            memset(font.glyphs, 0, sizeof(font.glyphs));
        }

        // What isn't in the font takes the room of a space, or of a '?' if it has one.
        void fillGaps(FontData& font, float fallbackAdvance)
        {
            Glyph& space = font.glyphs[' ' - kFirstGlyph];
            if (space.advance <= 0)
            {
                space.advance = fallbackAdvance;
            }
        }
    }

    bool parseBitmapFont(const char* text, size_t size, FontData& font, char* page, size_t pageSize, u32& width, u32& height)
    {
        clearGlyphs(font);
        page[0] = 0;
        width = height = 0;
        int base = 0, lineHeight = 0;
        bool sawChar = false;
        float widest = 0;
        const char* end = text + size;
        for (const char* line = text; line < end;)
        {
            const char* stop = line;
            while (stop < end && *stop != '\n' && *stop != '\r')
            {
                stop++;
            }
            if (starts(line, stop, "common"))
            {
                lineHeight = field(line, stop, "lineHeight", 0);
                base = field(line, stop, "base", 0);
                width = static_cast<u32>(field(line, stop, "scaleW", 0));
                height = static_cast<u32>(field(line, stop, "scaleH", 0));
                if (field(line, stop, "pages", 1) != 1)
                {
                    return false;  // one picture only
                }
            }
            else if (starts(line, stop, "page") && field(line, stop, "id", 0) == 0)
            {
                const char* quote = static_cast<const char*>(memchr(line, '"', static_cast<size_t>(stop - line)));
                const char* close = quote ? static_cast<const char*>(memchr(quote + 1, '"', static_cast<size_t>(stop - quote - 1))) : nullptr;
                if (close == nullptr || static_cast<size_t>(close - quote) >= pageSize)
                {
                    return false;
                }
                memcpy(page, quote + 1, static_cast<size_t>(close - quote - 1));
                page[close - quote - 1] = 0;
            }
            else if (starts(line, stop, "char") && width && height)
            {
                const int id = field(line, stop, "id", -1);
                if (id >= static_cast<int>(kFirstGlyph) && id <= static_cast<int>(kLastGlyph) && field(line, stop, "page", 0) == 0)
                {
                    Glyph& g = font.glyphs[id - kFirstGlyph];
                    const float x = static_cast<float>(field(line, stop, "x", 0)), y = static_cast<float>(field(line, stop, "y", 0));
                    g.width = static_cast<float>(field(line, stop, "width", 0));
                    g.height = static_cast<float>(field(line, stop, "height", 0));
                    g.x = static_cast<float>(field(line, stop, "xoffset", 0));
                    g.y = static_cast<float>(field(line, stop, "yoffset", 0) - base);
                    g.advance = static_cast<float>(field(line, stop, "xadvance", 0));
                    g.u0 = x / static_cast<float>(width);
                    g.v0 = y / static_cast<float>(height);
                    g.u1 = (x + g.width) / static_cast<float>(width);
                    g.v1 = (y + g.height) / static_cast<float>(height);
                    widest = g.advance > widest ? g.advance : widest;
                    sawChar = true;
                }
            }
            line = stop;
            while (line < end && (*line == '\n' || *line == '\r'))
            {
                line++;
            }
        }
        if (!sawChar || page[0] == 0 || lineHeight <= 0)
        {
            return false;
        }
        font.ascent = static_cast<float>(base);
        font.lineHeight = static_cast<float>(lineHeight);
        font.scale = 1;
        fillGaps(font, widest * 0.4f);
        return true;
    }

    bool cutImageFont(u8* rgba, u32 width, u32 height, const char* glyphs, u32 spacing, FontData& font)
    {
        clearGlyphs(font);
        if (rgba == nullptr || width == 0 || height == 0 || glyphs == nullptr || *glyphs == 0)
        {
            return false;
        }
        // The colour of the top left pixel is what lies between them.
        u32 between;
        memcpy(&between, rgba, 4);
        const auto column = [&](u32 x)
        {
            for (u32 y = 0; y < height; y++)
            {
                u32 c;
                memcpy(&c, rgba + (static_cast<size_t>(y) * width + x) * 4, 4);
                if (c != between)
                {
                    return false;
                }
            }
            return true;
        };
        u32 x = 0;
        const char* p = glyphs;
        float widest = 0;
        u32 found = 0;
        while (*p)
        {
            const u32 code = nextChar(p);
            while (x < width && column(x))
            {
                x++;
            }
            if (x >= width)
            {
                break;
            }
            const u32 start = x;
            while (x < width && !column(x))
            {
                x++;
            }
            if (code >= kFirstGlyph && code <= kLastGlyph)
            {
                Glyph& g = font.glyphs[code - kFirstGlyph];
                g.x = 0;
                g.y = -static_cast<float>(height);
                g.width = static_cast<float>(x - start);
                g.height = static_cast<float>(height);
                g.advance = g.width + static_cast<float>(spacing);
                g.u0 = static_cast<float>(start) / static_cast<float>(width);
                g.u1 = static_cast<float>(x) / static_cast<float>(width);
                g.v0 = 0;
                g.v1 = 1;
                widest = g.advance > widest ? g.advance : widest;
                found++;
            }
        }
        // What lies between is see-through when drawn.
        for (size_t i = 0; i < static_cast<size_t>(width) * height; i++)
        {
            u32 c;
            memcpy(&c, rgba + i * 4, 4);
            if (c == between)
            {
                memset(rgba + i * 4, 0, 4);
            }
        }
        // A little room between lines, as a font's own leading would be.
        font.ascent = static_cast<float>(height);
        font.lineHeight = static_cast<float>(height + (height + 3) / 4);
        font.scale = 1;
        fillGaps(font, widest * 0.5f);
        return found > 0;
    }
}
