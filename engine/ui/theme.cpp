#include "ui/theme.h"
#include "base/log.h"

#include <stdio.h>
#include <string.h>

namespace nx
{
    using Json = JsonTree::Value;

    namespace
    {
        // What everything looks like until a theme says otherwise.
        const char kOwn[] = R"({
  "font": "global:freesans.sfn",
  "sounds": { "move": "global:sound/move.wav", "accept": "global:sound/accept.wav", "back": "global:sound/whoosh.wav", "change": "global:sound/move.wav" },
  "colors": {
    "accent": "#4590f5", "strong": "#ffffff", "text": "#e4eaf6", "dim": "#8f9db8",
    "panel": "#1a2032", "control": "#27304a", "raised": "#33405f", "track": "#39445f", "shade": "#000000b0", "warning": "#f0605a"
  },
  "styles": {
    "plain": { "text": "$text", "size": 24 },
    "title": { "base": "plain", "text": "$strong", "size": 44 },
    "heading": { "base": "plain", "text": "$strong", "size": 30 },
    "small": { "base": "plain", "text": "$dim", "size": 19 },
    "warning": { "base": "plain", "text": "$warning" },
    "panel": { "base": "plain", "back": "$panel", "radius": 18, "padding": 24, "gap": 14 },
    "control": {
      "base": "plain", "back": "$control", "line": "$accent", "track": "$track", "radius": 12, "padding": [20, 11], "minHeight": 50, "gap": 12,
      "hovered": { "back": "$raised" },
      "focused": { "back": "$raised", "lineWidth": 3, "text": "$strong" },
      "pressed": { "scale": 0.97 },
      "disabled": { "text": "#8f9db870", "back": "#27304a70" }
    },
    "button": { "base": "control", "align": "center", "focused": { "back": "$accent", "lineWidth": 0, "scale": 1.03, "shadow": 0.35 } },
    "quiet": { "base": "control", "back": "#00000000", "align": "center", "track": "$track" },
    "toggle": { "base": "control" },
    "slider": { "base": "control", "minWidth": 260 },
    "progress": { "base": "plain", "line": "$accent", "track": "$track", "minHeight": 14, "minWidth": 260 },
    "choice": { "base": "control", "align": "center", "minWidth": 220 },
    "field": { "base": "control", "minWidth": 320 },
    "tab": { "base": "quiet", "text": "$dim", "checked": { "back": "$control", "text": "$strong" }, "focused": { "lineWidth": 3, "back": "$raised" } },
    "row": { "base": "control", "back": "#ffffff0c", "radius": 10, "padding": [18, 8], "minHeight": 0, "focused": { "back": "$raised", "lineWidth": 3 } },
    "line": { "base": "plain", "line": "#ffffff24" },
    "bar": { "base": "plain", "line": "#ffffff50", "track": "#ffffff10" },
    "dialog": { "base": "panel", "padding": 32, "gap": 20, "shadow": 0.6, "minWidth": 420 },
    "shade": { "base": "plain", "back": "$shade" },
    "menu": { "base": "panel", "padding": 8, "gap": 4, "radius": 14, "shadow": 0.6 },
    "keyboard": { "base": "panel", "padding": 20, "gap": 8, "radius": 22, "shadow": 0.6 },
    "key": { "base": "button", "padding": [4, 8], "minHeight": 48, "size": 26 },
    "toast": { "base": "panel", "padding": [28, 14], "radius": 30, "shadow": 0.5 }
  }
})";

        constexpr u16 kBack = 1, kText = 2, kLine = 4, kTrack = 8, kLineWidth = 16, kRadius = 32, kScale = 64, kShadow = 128;

        const char* const kStates[] = { "normal", "hovered", "checked", "focused", "pressed", "disabled" };
        static_assert(sizeof(kStates) / sizeof(kStates[0]) == static_cast<size_t>(UiState::Count));

        int hex(char c)
        {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        }
    }

    bool Theme::path(const char* folder, const char* name, char* out, size_t size)
    {
        int length = -1;
        if (strncmp(name, "global:", 7) == 0)
        {
            length = snprintf(out, size, "global/%s", name + 7);
        }
        else if (strncmp(name, "asset:", 6) == 0)
        {
            length = snprintf(out, size, "%s/asset/%s", folder, name + 6);
        }
        else if (strncmp(name, "https://", 8) == 0 || strncmp(name, "http://", 7) == 0)
        {
            length = snprintf(out, size, "%s", name);
        }
        return length > 0 && static_cast<size_t>(length) < size;
    }

    void Theme::clear()
    {
        if (m_assets)
        {
            for (u32 i = 0; i < m_count; i++)
            {
                if (m_styles[i].font)
                {
                    m_assets->release(m_styles[i].font);
                }
                if (m_styles[i].image)
                {
                    m_assets->release(m_styles[i].image);
                }
            }
            for (Asset*& sound : m_sounds)
            {
                if (sound)
                {
                    m_assets->release(sound);
                }
                sound = nullptr;
            }
        }
        m_count = 0;
        m_colorCount = 0;
        m_font[0] = 0;
        memset(m_soundNames, 0, sizeof(m_soundNames));
    }

    bool Theme::fail(const char* file, u32 line, const char* what, const char* name)
    {
        snprintf(m_error, sizeof(m_error), "%s:%u: %s %s", file, static_cast<unsigned>(line), name, what);
        return false;
    }

    UiStyle* Theme::named(const char* name)
    {
        for (u32 i = 0; i < m_count; i++)
        {
            if (strcmp(m_styles[i].name, name) == 0)
            {
                return &m_styles[i];
            }
        }
        return nullptr;
    }

    const UiStyle* Theme::find(const char* name) const
    {
        for (u32 i = 0; i < m_count; i++)
        {
            if (strcmp(m_styles[i].name, name) == 0)
            {
                return &m_styles[i];
            }
        }
        return m_count ? &m_styles[0] : nullptr;
    }

    bool Theme::load(Assets& assets, const char* json, size_t size, const char* file, const char* folder)
    {
        bool ok = true;
        m_error[0] = 0;
        for (u32 attempt = 0; attempt < 2; attempt++)
        {
            clear();
            m_assets = &assets;
            snprintf(m_folder, sizeof(m_folder), "%s", folder);
            const char* extra = ok ? json : nullptr;
            // A theme may change only a colour, so they are all known before anything uses one.
            const bool read = parse(kOwn, sizeof(kOwn) - 1, "theme", true) && (extra == nullptr || parse(extra, size, file, true)) &&
                parse(kOwn, sizeof(kOwn) - 1, "theme", false) && (extra == nullptr || parse(extra, size, file, false));
            if (read)
            {
                break;
            }
            if (extra == nullptr)
            {
                NX_ERROR("%s", m_error);
                return false;
            }
            ok = false;
        }
        char where[192];
        for (u32 i = 0; i < m_count; i++)
        {
            UiStyle& s = m_styles[i];
            if (path(folder, s.fontName[0] ? s.fontName : m_font, where, sizeof(where)))
            {
                s.font = assets.font(where, s.size);
            }
            if (s.imageName[0] && path(folder, s.imageName, where, sizeof(where)))
            {
                s.image = assets.texture(where);
            }
        }
        for (u32 i = 0; i < SoundCount; i++)
        {
            if (m_soundNames[i][0] && path(folder, m_soundNames[i], where, sizeof(where)))
            {
                m_sounds[i] = assets.sound(where);
            }
        }
        return ok;
    }

    bool Theme::readColor(Json v, Color& out, const char* file)
    {
        if (v.isArray())
        {
            out = { v[0u].number(), v[1u].number(), v[2u].number(), v[3u].number(1) };
            return true;
        }
        const char* text = v.text();
        if (text[0] == '$')
        {
            for (u32 i = 0; i < m_colorCount; i++)
            {
                if (strcmp(m_colors[i].name, text + 1) == 0)
                {
                    out = m_colors[i].color;
                    return true;
                }
            }
            return fail(file, v.line(), "isn't one of its colours", text);
        }
        const size_t length = strlen(text);
        float parts[4] = { 1, 1, 1, 1 };
        bool ok = text[0] == '#' && (length == 7 || length == 9);
        for (size_t i = 0; ok && i < (length - 1) / 2; i++)
        {
            const int high = hex(text[1 + i * 2]), low = hex(text[2 + i * 2]);
            ok = high >= 0 && low >= 0;
            parts[i] = static_cast<float>(high * 16 + low) / 255.0f;
        }
        if (!ok)
        {
            return fail(file, v.line(), "isn't a colour: use \"$name\", \"#rrggbb\", \"#rrggbbaa\" or [r, g, b, a]", text);
        }
        out = { parts[0], parts[1], parts[2], parts[3] };
        return true;
    }

    bool Theme::readLook(Json v, UiStyle& style, UiState state, const char* file)
    {
        UiLook& look = style.looks[static_cast<size_t>(state)];
        u16& given = style.given[static_cast<size_t>(state)];
        const struct { const char* name; Color* color; u16 bit; } colors[] = { { "back", &look.back, kBack }, { "text", &look.text, kText }, { "line", &look.line, kLine }, { "track", &look.track, kTrack } };
        const struct { const char* name; float* number; u16 bit; } numbers[] = { { "lineWidth", &look.lineWidth, kLineWidth }, { "radius", &look.radius, kRadius }, { "scale", &look.scale, kScale }, { "shadow", &look.shadow, kShadow } };
        const char* key = v.key();
        for (const auto& c : colors)
        {
            if (strcmp(key, c.name) == 0)
            {
                given |= c.bit;
                return readColor(v, *c.color, file);
            }
        }
        for (const auto& n : numbers)
        {
            if (strcmp(key, n.name) == 0)
            {
                given |= n.bit;
                *n.number = v.number();
                return v.isNumber() || fail(file, v.line(), "must be a number", key);
            }
        }
        return fail(file, v.line(), "isn't part of a style", key);
    }

    bool Theme::readStyle(Json v, const char* file)
    {
        const char* name = v.key();
        UiStyle* style = named(name);
        if (style == nullptr)
        {
            if (m_count == kMaxStyles || strlen(name) >= sizeof(style->name))
            {
                return fail(file, v.line(), "is one style too many, or its name is too long", name);
            }
            style = &m_styles[m_count++];
            *style = UiStyle();
            // Anything starts as the plain one.
            if (m_count > 1)
            {
                *style = m_styles[0];
            }
            snprintf(style->name, sizeof(style->name), "%s", name);
        }
        if (const Json base = v["base"])
        {
            const UiStyle* other = named(base.text());
            if (other == nullptr)
            {
                return fail(file, base.line(), "isn't a style before it", base.text());
            }
            const UiStyle copy = *other;
            *style = copy;
            snprintf(style->name, sizeof(style->name), "%s", name);
        }
        for (Json p = v.first(); p.exists(); p = p.next())
        {
            const char* key = p.key();
            bool done = strcmp(key, "base") == 0;
            for (u32 i = 1; i < static_cast<u32>(UiState::Count) && !done; i++)
            {
                if (strcmp(key, kStates[i]) == 0)
                {
                    done = true;
                    for (Json q = p.first(); q.exists(); q = q.next())
                    {
                        if (!readLook(q, *style, static_cast<UiState>(i), file))
                        {
                            return false;
                        }
                    }
                }
            }
            if (done)
            {
                continue;
            }
            if (strcmp(key, "size") == 0) style->size = static_cast<u32>(p.number(24));
            else if (strcmp(key, "gap") == 0) style->gap = p.number();
            else if (strcmp(key, "minWidth") == 0) style->minWidth = p.number();
            else if (strcmp(key, "minHeight") == 0) style->minHeight = p.number();
            else if (strcmp(key, "slice") == 0) style->slice = p.number();
            else if (strcmp(key, "seconds") == 0) style->seconds = p.number();
            else if (strcmp(key, "font") == 0) snprintf(style->fontName, sizeof(style->fontName), "%s", p.text());
            else if (strcmp(key, "image") == 0) snprintf(style->imageName, sizeof(style->imageName), "%s", p.text());
            else if (strcmp(key, "align") == 0)
            {
                const char* a = p.text();
                style->align = strcmp(a, "center") == 0 ? Align::Center : strcmp(a, "right") == 0 ? Align::Right : Align::Left;
            }
            else if (strcmp(key, "padding") == 0)
            {
                for (u32 i = 0; i < 4; i++)
                {
                    style->padding[i] = p.isArray() ? p[p.count() == 4 ? i : i % 2].number() : p.number();
                }
            }
            else if (!readLook(p, *style, UiState::Normal, file))
            {
                return false;
            }
        }
        return true;
    }

    bool Theme::parse(const char* json, size_t size, const char* file, bool colorsOnly)
    {
        JsonTree tree;
        if (!tree.parse(json, size))
        {
            return fail(file, tree.errorLine(), tree.error(), "");
        }
        const Json root = tree.root();
        for (Json part = root.first(); part.exists(); part = part.next())
        {
            const char* key = part.key();
            if (strcmp(key, "colors") == 0)
            {
                for (Json c = part.first(); c.exists() && colorsOnly; c = c.next())
                {
                    u32 at = 0;
                    while (at < m_colorCount && strcmp(m_colors[at].name, c.key()) != 0)
                    {
                        at++;
                    }
                    Color color;
                    if (at == sizeof(m_colors) / sizeof(m_colors[0]) || strlen(c.key()) >= sizeof(m_colors[0].name))
                    {
                        return fail(file, c.line(), "is one colour too many, or its name is too long", c.key());
                    }
                    if (!readColor(c, color, file))
                    {
                        return false;
                    }
                    snprintf(m_colors[at].name, sizeof(m_colors[at].name), "%s", c.key());
                    m_colors[at].color = color;
                    m_colorCount = at == m_colorCount ? at + 1 : m_colorCount;
                }
            }
            else if (colorsOnly)
            {
                continue;
            }
            else if (strcmp(key, "font") == 0)
            {
                snprintf(m_font, sizeof(m_font), "%s", part.text());
            }
            else if (strcmp(key, "sounds") == 0)
            {
                static const char* const names[SoundCount] = { "move", "accept", "back", "change" };
                for (Json s = part.first(); s.exists(); s = s.next())
                {
                    bool known = false;
                    for (u32 i = 0; i < SoundCount; i++)
                    {
                        if (strcmp(s.key(), names[i]) == 0)
                        {
                            known = true;
                            snprintf(m_soundNames[i], sizeof(m_soundNames[i]), "%s", s.text());
                        }
                    }
                    if (!known)
                    {
                        return fail(file, s.line(), "isn't a sound it makes", s.key());
                    }
                }
            }
            else if (strcmp(key, "styles") == 0)
            {
                for (Json s = part.first(); s.exists(); s = s.next())
                {
                    if (!readStyle(s, file))
                    {
                        return false;
                    }
                }
            }
            else
            {
                return fail(file, part.line(), "isn't part of a theme", key);
            }
        }
        return true;
    }
}
