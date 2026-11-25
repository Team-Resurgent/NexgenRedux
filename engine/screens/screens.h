#pragma once

#include "bindings/bindings.h"

class asIScriptObject;
class asIScriptFunction;

namespace nx
{
    // A screen that can be opened: a folder with a screen.json and a main.as.
    struct ScreenInfo
    {
        char id[48] = {};
        char title[64] = {};
        char description[160] = {};
        char category[32] = {};
        char demo[400] = {};  // what to press when it is shown off by itself
        float seconds = 0;    // and for how long, if it needs more than the others
        int order = 0;
    };

    // The stack of open screens. Each has its own script module and its own hold on assets, and closing
    // one lets go of everything it had. The engine's menu can always be reached, and a script error
    // stops only the screen it happened in.
    class Screens : NonCopyable
    {
    public:
        ~Screens() { shutdown(); }

        // Folder holds one folder per screen. The first screen opened is the home screen.
        bool init(Services& services, const char* folder, const char* home);
        void shutdown();

        // Requests; carried out by applyChanges().
        void push(const char* id);
        void pop();
        void replace(const char* id);
        void home();
        void restart();

        // Restarts the current screen if its script file has changed. For development.
        void reloadChanged();

        // One fixed step, then any changes it asked for.
        void update(float dt);
        void draw();

        u32 depth() const { return m_depth; }
        const char* current() const { return m_depth ? m_stack[m_depth - 1].info.id : ""; }
        bool menuOpen() const { return m_menuOpen; }
        bool loading() const { return m_depth && m_stack[m_depth - 1].loading; }
        bool failed() const { return m_depth && m_stack[m_depth - 1].failed; }
        const char* error() const { return m_depth ? m_stack[m_depth - 1].error : ""; }
        bool quitRequested() const { return m_quit; }
        // Off where closing the app makes no sense.
        void setCanQuit(bool can) { m_canQuit = can; }

        u32 count() const { return m_count; }
        const ScreenInfo* info(u32 index) const { return index < m_count ? &m_infos[index] : nullptr; }

    private:
        static constexpr u32 kMaxDepth = 8;
        static constexpr u32 kMaxScreens = 64;

        enum class Change : u8 { None, Push, Pop, Replace, Home, Restart };

        struct Entry
        {
            ScreenInfo info;
            char module[32] = {};
            asIScriptObject* object = nullptr;
            asIScriptFunction* update = nullptr;
            asIScriptFunction* draw = nullptr;
            bool failed = false;
            bool loading = false;  // waiting for its assets
            float loadingFor = 0;
            u32 group = 0;         // of its sounds and coroutines
            u64 scriptTime = 0;
            World* world = nullptr;
            char error[256] = {};
        };

        void request(Change change, const char* id);
        void applyChanges();
        bool open(const ScreenInfo& info);
        void close(bool callExit);
        bool load(Entry& entry);
        void unload(Entry& entry, bool callExit);
        void callOptional(Entry& entry, const char* declaration);
        void fail(Entry& entry);
        void enter(const Entry& entry);
        void setMenu(bool open);
        void arrive(Entry& entry);
        void addInfo(const char* folderName);
        const ScreenInfo* findInfo(const char* id) const;
        u32 menuItems(const char** items) const;
        void updateMenu();
        void drawMenu();
        void drawError(const Entry& entry);
        void drawLoading(const Entry& entry);

        Services* m_services = nullptr;
        char m_folder[64] = {};
        ScreenInfo m_infos[kMaxScreens];
        u32 m_count = 0;

        Entry m_stack[kMaxDepth];
        u32 m_depth = 0;
        u32 m_nextModule = 0;

        Change m_change = Change::None;
        char m_changeId[48] = {};

        Asset* m_font = nullptr;
        Asset* m_smallFont = nullptr;
        bool m_menuOpen = false;
        u32 m_menuChoice = 0;
        bool m_canQuit = true;
        bool m_quit = false;
    };
}
