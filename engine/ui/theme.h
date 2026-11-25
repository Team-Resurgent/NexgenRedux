#pragma once

#include "assets/assets.h"
#include "base/grow_array.h"
#include "base/json_tree.h"
#include "canvas/canvas.h"

namespace nx
{
    // What a widget looks like at one moment. All of it moves smoothly from one state to another.
    struct UiLook
    {
        Color back{ 0, 0, 0, 0 };   // behind it
        Color text{ 1, 1, 1, 1 };
        Color line{ 0, 0, 0, 0 };   // round it, and what a slider or a switch fills with
        Color track{ 0, 0, 0, 0 };  // what is yet to be filled
        float lineWidth = 0;
        float radius = 0;
        float scale = 1;
        float shadow = 0;           // how dark
    };

    // From plainest to most important: later ones are put on top of earlier ones.
    enum class UiState : u8 { Normal, Hovered, Checked, Focused, Pressed, Disabled, Count };

    struct UiStyle
    {
        char name[32] = {};
        UiLook looks[static_cast<size_t>(UiState::Count)];
        u16 given[static_cast<size_t>(UiState::Count)] = {};  // which parts of it; a bit each
        Asset* font = nullptr;     // held
        Asset* image = nullptr;    // held; stretched by its middle
        char fontName[96] = {};
        char imageName[96] = {};
        u32 size = 24;             // of the text
        float slice = 0;           // of the image's corners, in its own pixels
        float padding[4] = {};     // left, top, right, bottom
        float gap = 0;
        float minWidth = 0, minHeight = 0;
        Align align = Align::Left;
        float seconds = 0.1f;      // from one state to another
    };

    // Styles by name, from a file over the engine's own. Plain data until its fonts and images are asked for.
    class Theme : NonCopyable
    {
    public:
        static constexpr u32 kMaxStyles = 48;

        ~Theme() { clear(); }

        // The engine's own, with whatever a file changes: nullptr for nothing. Folder is where "asset:" is.
        // False, with the line and what was wrong, leaving the engine's own.
        bool load(Assets& assets, const char* json, size_t size, const char* file, const char* folder);
        void clear();

        // The plain one if there is none of that name.
        const UiStyle* find(const char* name) const;
        Asset* sound(u32 which) const { return m_sounds[which]; }
        const char* error() const { return m_error; }

        enum Sound { Move, Accept, Back, Change, SoundCount };

        // "asset:x", "global:x" or the web.
        static bool path(const char* folder, const char* name, char* out, size_t size);

    private:
        struct Named
        {
            char name[24];
            Color color;
        };

        bool parse(const char* json, size_t size, const char* file, bool colorsOnly);
        bool readColor(JsonTree::Value v, Color& out, const char* file);
        bool readLook(JsonTree::Value v, UiStyle& style, UiState state, const char* file);
        bool readStyle(JsonTree::Value v, const char* file);
        bool fail(const char* file, u32 line, const char* what, const char* name);
        UiStyle* named(const char* name);

        Assets* m_assets = nullptr;
        UiStyle m_styles[kMaxStyles];
        u32 m_count = 0;
        Named m_colors[32];
        u32 m_colorCount = 0;
        char m_font[96] = {};
        char m_soundNames[SoundCount][96] = {};
        Asset* m_sounds[SoundCount] = {};
        char m_folder[96] = {};
        char m_error[200] = {};
    };
}
