#include "canvas/canvas.h"
#include "assets/assets.h"

#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

// Text with some of it marked, as a few tags in square brackets. The tags are taken out first, so that the words wrap as a
// plain paragraph's do; then each line is drawn in pieces, each piece as its marks say.
namespace nx
{
    namespace
    {
        struct Style
        {
            Color color;
            bool bold = false, underline = false, strike = false;
        };

        // Where in the plain text a style begins.
        struct Change
        {
            size_t at;
            Style style;
        };

        bool hex(const char* p, size_t n, u32& out)
        {
            out = 0;
            for (size_t i = 0; i < n; i++)
            {
                const char c = p[i];
                const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                if (d < 0)
                {
                    return false;
                }
                out = out * 16 + static_cast<u32>(d);
            }
            return true;
        }

        bool colorOf(const std::string& name, Color& out)
        {
            static const struct { const char* name; Color color; } named[] = {
                { "white", { 1, 1, 1, 1 } }, { "black", { 0, 0, 0, 1 } }, { "red", { 1, 0.3f, 0.3f, 1 } }, { "green", { 0.4f, 0.9f, 0.4f, 1 } },
                { "blue", { 0.4f, 0.6f, 1, 1 } }, { "yellow", { 1, 0.9f, 0.3f, 1 } }, { "orange", { 1, 0.6f, 0.2f, 1 } }, { "pink", { 1, 0.5f, 0.8f, 1 } },
                { "purple", { 0.7f, 0.5f, 1, 1 } }, { "cyan", { 0.4f, 0.9f, 1, 1 } }, { "gray", { 0.6f, 0.6f, 0.6f, 1 } }, { "grey", { 0.6f, 0.6f, 0.6f, 1 } },
            };
            for (const auto& n : named)
            {
                if (name == n.name)
                {
                    out = n.color;
                    return true;
                }
            }
            u32 v;
            if (name.size() == 7 && name[0] == '#' && hex(name.c_str() + 1, 6, v))
            {
                out = { static_cast<float>((v >> 16) & 255) / 255, static_cast<float>((v >> 8) & 255) / 255, static_cast<float>(v & 255) / 255, 1 };
                return true;
            }
            if (name.size() == 9 && name[0] == '#' && hex(name.c_str() + 1, 8, v))
            {
                out = { static_cast<float>((v >> 24) & 255) / 255, static_cast<float>((v >> 16) & 255) / 255, static_cast<float>((v >> 8) & 255) / 255,
                        static_cast<float>(v & 255) / 255 };
                return true;
            }
            return false;
        }

        // The words without their tags, and where each style starts. Tags it doesn't know are left as they are.
        void parse(const char* text, const Color& base, std::string& plain, std::vector<Change>& changes)
        {
            Style now;
            now.color = base;
            std::vector<Color> colors;
            changes.push_back({ 0, now });
            for (const char* p = text; *p;)
            {
                if (p[0] == '[' && p[1] == '[')
                {
                    plain += '[';
                    p += 2;
                    continue;
                }
                const char* close = p[0] == '[' ? strchr(p, ']') : nullptr;
                if (close == nullptr)
                {
                    plain += *p++;
                    continue;
                }
                const std::string tag(p + 1, close);
                Style next = now;
                bool known = true;
                if (tag == "b") next.bold = true;
                else if (tag == "/b") next.bold = false;
                else if (tag == "u") next.underline = true;
                else if (tag == "/u") next.underline = false;
                else if (tag == "s") next.strike = true;
                else if (tag == "/s") next.strike = false;
                else if (tag.compare(0, 6, "color=") == 0)
                {
                    Color c;
                    known = colorOf(tag.substr(6), c);
                    if (known)
                    {
                        colors.push_back(now.color);
                        next.color = Color(c.r, c.g, c.b, c.a * base.a);
                    }
                }
                else if (tag == "/color")
                {
                    next.color = colors.empty() ? base : colors.back();
                    if (!colors.empty())
                    {
                        colors.pop_back();
                    }
                }
                else
                {
                    known = false;
                }
                if (!known)
                {
                    plain += *p++;
                    continue;
                }
                now = next;
                if (changes.back().at == plain.size())
                {
                    changes.back().style = now;
                }
                else
                {
                    changes.push_back({ plain.size(), now });
                }
                p = close + 1;
            }
        }
    }

    float Canvas::rich(const Asset& font, const char* text, float x, float y, float width, const Color& color, Align align)
    {
        if (font.font == nullptr)
        {
            return 0;
        }
        std::string plain;
        std::vector<Change> changes;
        parse(text, color, plain, changes);
        const FontData& f = *font.font;
        const float line = f.lineHeight * f.scale;
        const float thick = line * 0.07f > 1 ? line * 0.07f : 1;
        const float top = y;
        const char* start = plain.c_str();
        for (const char* l = start; *l;)
        {
            const char* next;
            const char* stop = lineEnd(*this, f, l, width, next);
            const bool justify = align == Align::Justify;
            const float extra = justify ? justifyGap(f, l, stop, next, width) : 0;
            const float spare = align == Align::Left || justify ? 0 : width - lineWidth(f, l, stop);
            float at = align == Align::Center ? x + spare * 0.5f : x + spare;
            // The pieces of this line, each in its style.
            for (size_t c = 0; c < changes.size(); c++)
            {
                const char* from = start + changes[c].at;
                const char* to = c + 1 < changes.size() ? start + changes[c + 1].at : start + plain.size();
                from = from < l ? l : from;
                to = to > stop ? stop : to;
                if (from >= to)
                {
                    continue;
                }
                const Style& s = changes[c].style;
                const gpu::TextureInfo info = m_device->textureInfo(font.texture);
                use(font.texture, info.uMax, info.vMax);
                const u32 packed = pack(s.color);
                const float end = spacedRun(font, from, to, at, y, packed, extra);
                if (s.bold)
                {
                    // Again, a little to the side: thicker, as the font has no bold of its own.
                    spacedRun(font, from, to, at + thick * 0.8f, y, packed, extra);
                }
                if (s.underline)
                {
                    rect(at, y + f.ascent * f.scale + thick, end - at, thick, s.color);
                }
                if (s.strike)
                {
                    rect(at, y + f.ascent * f.scale * 0.62f, end - at, thick, s.color);
                }
                at = end;
            }
            y += line;
            l = next;
        }
        return y - top;
    }

    float Canvas::richHeight(const Asset& font, const char* text, float width) const
    {
        if (font.font == nullptr)
        {
            return 0;
        }
        std::string plain;
        std::vector<Change> changes;
        parse(text, Color(1, 1, 1, 1), plain, changes);
        return paragraphHeight(font, plain.c_str(), width);
    }
}
