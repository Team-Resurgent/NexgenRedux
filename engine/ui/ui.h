#pragma once

#include "audio/audio.h"
#include "base/grow_array.h"
#include "base/json_tree.h"
#include "canvas/canvas.h"
#include "input/input.h"
#include "script/script_host.h"
#include "ui/theme.h"

#include <string>
#include <vector>

namespace nx
{
    class Ui;

    enum class WidgetKind : u8
    {
        Column, Row, Grid, Stack, Scroll,
        Label, Image, Button, Toggle, Slider, Progress, Choice, Field, List, Tabs, Space, Line, Custom,
        Count
    };

    // Along or across a container.
    enum class UiPlace : u8 { Start, Center, End, Stretch, Spread, Inherit };

    enum class UiEvent : u8 { Click, Change, Focus, Done, Draw, Row, Count };

    // One thing on a page. Counted: its parent holds it, and so may scripts. Outlives its Ui, and then does nothing.
    struct Widget : NonCopyable
    {
        int refs = 1;
        Ui* ui = nullptr;            // nullptr once that has gone
        Widget* nextAll = nullptr;   // of all its Ui has
        Widget* previousAll = nullptr;
        Widget* parent = nullptr;
        GrowArray<Widget*, MemTag::UI> children;   // held

        WidgetKind kind = WidgetKind::Column;
        std::string id, text, hint;
        char styleName[32] = {};
        const UiStyle* style = nullptr;

        bool visible = true, enabled = true;
        bool checked = false;        // a toggle
        bool wrap = false;           // text
        bool secret = false;         // a field's text is hidden
        bool numeric = false;        // a field takes only digits
        float width = 0, height = 0; // 0: as large as what is in it
        float minWidth = 0, maxWidth = 0;
        float grow = 0;              // its share of what is left over
        float gap = -1;              // below 0: the style's
        float padding[4] = { -1, -1, -1, -1 };
        UiPlace align = UiPlace::Inherit;    // of what is in it, across
        UiPlace justify = UiPlace::Start;    // and along
        UiPlace self = UiPlace::Inherit;     // of itself, across what it is in
        u32 columns = 0;             // of a grid or a list; 0: as many as fit
        float columnWidth = 0;
        float value = 0, low = 0, high = 1, step = 0.1f;
        int chosen = 0;              // of a choice, tabs or a list
        std::vector<std::string> items;
        u32 count = 0;               // of a list's rows
        float rowHeight = 56;
        u32 limit = 64;              // characters in a field
        Asset* image = nullptr;      // held
        Color color{ 1, 1, 1, 1 };
        int index = -1;              // of a row in a list
        int tag = 0;                 // whatever a script likes
        asIScriptFunction* events[static_cast<size_t>(UiEvent::Count)] = {};
        void (*native)(Ui& ui, Widget& w) = nullptr;   // instead, for the engine's own

        // Where it is: from its parent's corner, before that is scrolled.
        float x = 0, y = 0, w = 0, h = 0;
        float naturalW = 0, naturalH = 0;
        float scroll = 0, scrollTo = 0, contentH = 0;

        UiLook look;
        bool lookReady = false;
        float amount = 0;            // of a switch or a slider, as it moves
        float pressed = 0;           // seconds it still looks so
        u32 caret = 0;               // bytes into a field's text
        float textShift = 0;         // so that the caret is seen

        ~Widget();
        void addRef() { refs++; }
        void release();
        bool focusable() const;
        bool container() const { return kind <= WidgetKind::Scroll || kind == WidgetKind::List || kind == WidgetKind::Tabs; }
        float pad(u32 side) const { return padding[side] >= 0 ? padding[side] : style ? style->padding[side] : 0; }
        float space() const { return gap >= 0 ? gap : style ? style->gap : 0; }
        Widget* add(WidgetKind kind, const char* style);
        void remove();
        void removeAll();
        void setText(const char* text);
        void setEvent(UiEvent event, asIScriptFunction* function);   // takes the reference
        void setImage(Asset* image, Assets& assets);                 // takes the reference
        Widget* row(int index);
        Widget* addTabs();
        Widget* addTab(const char* title);
        void changed();   // its size may have
    };

    // The widgets of one screen. Nothing is asked of it, drawn or updated until there are some.
    class Ui : NonCopyable
    {
    public:
        Ui(Canvas& canvas, Assets& assets, Audio& audio, Input& input, ScriptHost& host, Platform& platform, const char* folder);
        ~Ui();

        Widget& root() { return *m_root; }
        // False if a script failed.
        bool update(float dt);
        // In the canvas's design, which is remembered for the next layout.
        void draw(Canvas& canvas, float x, float y, float w, float h);

        // --- Pages ---

        bool loadTheme(const char* name);
        // Widgets from a file, put into one of them. Click and change name methods of the screen.
        bool load(const char* name, Widget& into, asIScriptObject* screen);
        bool words(const char* name);
        const char* word(const char* key) const;
        const char* error() const { return m_error; }
        // Whether a file it was made from has changed. For development.
        bool changed();
        Widget* find(const char* id);

        // --- Focus ---

        void focus(Widget* w, bool sound = false);
        Widget* focused() { return m_focus; }
        bool typing() const { return m_editing != nullptr; }

        // --- What is in front ---

        // Something that must be dealt with first. Closed by close(), or by Cancel.
        Widget* popup();
        void close(Widget& popup);
        // The answer is a button's number, or -1.
        void dialog(const char* title, const char* text, const std::vector<std::string>& buttons, asIScriptFunction* answer);
        void toast(const char* text, float seconds);
        void keyboard(Widget& field);
        bool busy() const { return m_layerCount > 0; }

        float safeArea = 0;   // part of the screen's height kept clear at its edges

        // --- For widgets ---

        void touch() { m_dirty = true; }
        void forget(Widget& w);
        void queue(Widget& w, UiEvent event, int number = 0);
        Widget* make(WidgetKind kind, const char* style);
        Assets& assets() { return m_assets; }
        const char* shown(const Widget& w) const;   // its text, in the language chosen
        void restyle(Widget& w);
        void act(Widget& w);                        // as if it had been pressed
        void showTab(Widget& tabs, int index) { setTab(tabs, index); }
        void place(const Widget& w, float& x, float& y) const;   // in the canvas's design
        void choose(Widget& list, int index, bool sound);
        void editKey(u32 code);                     // a character, or 8, or 13
        Widget* editing() { return m_editing; }
        void endEdit();
        Theme& theme() { return m_theme; }

    private:
        friend struct Widget;

        enum class LayerPlace : u8 { Center, Bottom, Below };

        struct Layer
        {
            Widget* root;      // held
            Widget* focus;     // what had it before
            Widget* anchor;    // what it is below
            LayerPlace place;
            float shown;
            asIScriptFunction* answer;   // of a dialog
            bool dim;
        };

        struct Waiting
        {
            Widget* widget;   // held, or nullptr for an answer
            asIScriptFunction* function;   // held
            UiEvent event;
            int number;
        };

        struct Toast
        {
            char text[120];
            float left, shown;
        };

        struct Word
        {
            std::string key, text;
        };

        static constexpr u32 kMaxLayers = 8;
        static constexpr u32 kMaxToasts = 4;

        // layout.cpp
        void layout();
        void layoutLayer(Widget& root, float w, float h);
        float naturalWidth(Widget& w);
        void giveWidth(Widget& w, float width);
        float naturalHeight(Widget& w);
        void giveHeight(Widget& w, float height);
        float textWidth(const Widget& w, const char* text) const;
        float textHeight(const Widget& w, float width) const;
        float lineHeight(const Widget& w) const;
        u32 gridColumns(const Widget& w, float inner) const;

        // draw.cpp
        void drawWidget(Canvas& c, Widget& w, float x, float y, float alpha);
        void drawBox(Canvas& c, const UiLook& look, const UiStyle& style, float x, float y, float w, float h);
        void drawText(Canvas& c, const Widget& w, const char* text, float x, float y, float width, float height, const Color& color, Align align);
        void drawField(Canvas& c, Widget& w, float x, float y);
        void drawScrollBar(Canvas& c, const Widget& w, float x, float y);
        void animate(Widget& w, float dt);

        // ui.cpp
        Widget& top() { return m_layerCount ? *m_layers[m_layerCount - 1].root : *m_root; }
        void corner(const Widget& w, float& x, float& y) const;
        bool inside(const Widget& w, const Widget& root) const;
        bool attached(const Widget& w) const;
        bool reachable(const Widget& w) const;
        void collect(Widget& w, GrowArray<Widget*, MemTag::UI>& out) const;
        Widget* firstFocusable(Widget& w) const;
        Widget* neighbour(Widget& from, Action way);
        void move(Action way);
        void reveal(Widget& w);
        bool fillLists(Widget& w);
        bool fillList(Widget& w);
        void updateMouse(float dt);
        void updateEditing(float dt);
        Widget* hit(Widget& w, float x, float y, float mx, float my, bool focusableOnly);
        void slideTo(Widget& w, float value);
        void step(Widget& w, int way);
        void beginEdit(Widget& w);
        void open(Widget* root, LayerPlace place, Widget* anchor, bool dim);
        void closeTop(int number);
        void openChoice(Widget& w);
        void setTab(Widget& tabs, int index);
        void play(Theme::Sound sound);
        bool dispatch();
        bool readFile(const char* name, FileData& out, char (&path)[192]);
        bool fail(const char* file, u32 line, const char* what, const char* name);
        // load.cpp
        Widget* build(JsonTree::Value v, Widget& into, const char* file, asIScriptObject* screen);
        bool watch(const char* path);

        Canvas& m_canvas;
        Assets& m_assets;
        Audio& m_audio;
        Input& m_input;
        ScriptHost& m_host;
        Platform& m_platform;
        char m_folder[96];
        Theme m_theme;
        bool m_themeReady = false;

        Widget* m_all = nullptr;
        Widget* m_root = nullptr;
        Widget* m_focus = nullptr;
        Widget* m_wanted = nullptr;     // to be gone to once there is something in it
        Widget* m_hovered = nullptr;
        Widget* m_held = nullptr;       // under the mouse button
        Widget* m_dragged = nullptr;    // a scrolling thing
        Widget* m_editing = nullptr;
        Widget* m_keyboardFor = nullptr;
        bool m_shift = false;
        u32 m_keyPage = 0;
        Layer m_layers[kMaxLayers] = {};
        u32 m_layerCount = 0;
        Toast m_toasts[kMaxToasts] = {};
        u32 m_toastCount = 0;
        GrowArray<Waiting, MemTag::UI> m_waiting;
        std::vector<Word> m_words;

        struct Watched
        {
            char path[192];
            u64 time;
        };
        Watched m_watched[8] = {};
        u32 m_watchedCount = 0;

        float m_x = 0, m_y = 0, m_w = 1280, m_h = 720;
        bool m_dirty = true;
        bool m_drawn = false;
        float m_time = 0;
        float m_mouseX = 0, m_mouseY = 0;
        float m_dragFrom = 0, m_dragScroll = 0;
        bool m_mouseDown = false;
        float m_keyHeld = 0;            // an arrow, while typing
        u32 m_keyBefore = 0;
        u32 m_padBefore = 0;
        float m_padHeld = 0;
        char m_error[200] = {};

        friend void keyboardBuild(Ui& ui);
        friend void keyPressed(Ui& ui, Widget& key);
    };
}
