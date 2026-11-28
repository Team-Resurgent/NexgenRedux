#pragma once

#include "base/memory.h"
#include "base/types.h"

#include <string>

namespace nx
{
    struct WindowDesc
    {
        const char* title = "Nexgen";
        u32 width = 1280;
        u32 height = 720;
        bool hidden = false;
    };

    // A whole file. The byte after the end is always 0, so text can be used as a string.
    struct FileData : NonCopyable
    {
        u8* data = nullptr;
        size_t size = 0;

        ~FileData() { memFree(data); }

        void clear()
        {
            memFree(data);
            data = nullptr;
            size = 0;
        }

        // Takes ownership of size + 1 bytes from memAlloc.
        void take(u8* bytes, size_t length)
        {
            memFree(data);
            data = bytes;
            size = length;
            data[size] = 0;
        }
    };

    enum class Key : u8
    {
        None,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
        Up, Down, Left, Right, Space, Enter, Escape, Tab, Backspace, Shift, Control,
        Count
    };

    enum class Button : u8
    {
        Up, Down, Left, Right, Start, Back, LeftStick, RightStick,
        A, B, X, Y,
        LeftShoulder, RightShoulder,  // White and Black on the original Xbox
        Count
    };

    constexpr u32 kMaxPads = 4;

    struct Gamepad
    {
        bool connected = false;
        u32 buttons = 0;                               // one bit per Button
        float leftX = 0, leftY = 0, rightX = 0, rightY = 0;  // -1 to 1, y up, dead zone removed
        float leftTrigger = 0, rightTrigger = 0;       // 0 to 1

        bool down(Button b) const { return (buttons & (1u << static_cast<u32>(b))) != 0; }
    };

    // What is held right now.
    struct InputState
    {
        bool keys[static_cast<size_t>(Key::Count)] = {};
        float mouseX = 0, mouseY = 0;  // pixels from the top left
        float wheel = 0;               // turned since the start: up is more
        float turnX = 0, turnY = 0;    // how far the mouse has moved since the start, in pixels, even where the pointer can't
        bool mouseButtons[3] = {};     // left, right, middle
        bool hasMouse = false;
        Gamepad pads[kMaxPads];
        // What a keyboard types, after layouts and Shift: the latest, round and round. Backspace is 8, Enter 13.
        static constexpr u32 kTyped = 32;
        u32 typed[kTyped] = {};
        u32 typedCount = 0;            // ever

        void type(u32 code)
        {
            typed[typedCount % kTyped] = code;
            typedCount++;
        }
    };

    // Sticks rarely rest at exactly zero: ignores the middle and stretches the rest to 0..1.
    inline float removeDeadZone(float value)
    {
        constexpr float dead = 0.24f;
        const float size = value < 0 ? -value : value;
        if (size < dead)
        {
            return 0;
        }
        const float scaled = (size - dead) / (1.0f - dead);
        const float limited = scaled > 1 ? 1 : scaled;
        return value < 0 ? -limited : limited;
    }

    // Everything the engine needs from an operating system or console.
    // Names of files that players bring are small letters, digits, dots, dashes and lines.
    inline char plainLetter(char c)
    {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ? c : '_';
    }

    // The pointer's look over the window.
    enum class PointerShape : u8 { Arrow, Hand, Text, Crosshair, Wait, Move, ResizeAcross, ResizeUpDown, NotAllowed, Count };

    // How Windows 11 draws a window's frame. Colours are 0xRRGGBB, or kSystem for the system's own.
    struct TitleBar
    {
        static constexpr u32 kSystem = 0xFFFFFFFFu;
        static constexpr u32 kNone = 0xFFFFFFFEu;   // a border that isn't drawn
        enum class Corners : u8 { System, Square, Round, SmallRound };
        enum class Backdrop : u8 { System, None, Mica, Acrylic, Tabbed };

        int dark = -1;                 // -1 as the system's apps are, 0 light, 1 dark
        u32 back = kSystem, text = kSystem, border = kSystem;
        Corners corners = Corners::System;
        Backdrop backdrop = Backdrop::System;
    };

    class Platform
    {
    public:
        virtual ~Platform() = default;

        // Does nothing where there is only the one screen.
        virtual bool openWindow(const WindowDesc& desc) = 0;
        virtual void* nativeWindow() = 0;

        // Handles pending events and reads the input devices. False once asked to close.
        virtual bool pumpEvents() = 0;
        virtual const InputState& input() const = 0;

        // High-resolution time since start-up.
        virtual double seconds() = 0;
        virtual void sleep(double seconds) = 0;

        // Path is UTF-8, relative to the media folder, and uses '/'. May be called from any thread.
        virtual bool readFile(const char* path, FileData& out, MemTag tag) = 0;

        // Changes when the file does; 0 if it can't be told (consoles). May be called from any thread.
        virtual u64 fileTime(const char* path) = 0;

        // Small files that the player keeps: settings and scores. Names are plain, like "settings".
        virtual bool loadSave(const char* name, FileData& out) = 0;
        // Over a file in the media, while developing. False where it can't be.
        virtual bool writeFile(const char*, const void*, size_t) { return false; }
        virtual bool writeSave(const char* name, const void* data, size_t size) = 0;
        virtual void removeSave(const char* name) = 0;
        // Somewhere else for them, so that a trial leaves the player's own alone. False if there is no such place.
        virtual bool setSaveFolder(const char*) { return false; }

        // Calls back with the name of each folder, or each file, directly inside one.
        using FolderCallback = void (*)(const char* name, void* user);
        virtual void list(const char* path, bool folders, FolderCallback callback, void* user) = 0;
        void listFolders(const char* path, FolderCallback callback, void* user) { list(path, true, callback, user); }

        // Files that the player brought: in the folder "own", which is beside the saves, or in the media on the
        // original Xbox. Those that have just been dropped on the window are put there, and are told of once.
        virtual bool takeDropped(std::string&) { return false; }

        // The pointer is hidden and kept in the window, for looking about with the mouse. Let go when the window is left.
        virtual void lockMouse(bool) {}
        // A pad's motors, 0 to 1: the heavy, slow one and the light, quick one. They run until told otherwise.
        virtual void rumble(u32, float, float) {}
        // Whether the engine draws the mouse pointer, where nothing else does, and it is to be seen now.
        virtual bool drawsPointer() const { return false; }

        // Physical memory in bytes; 0 if unknown.
        virtual u64 totalMemory() = 0;
        virtual u64 freeMemory() = 0;

        virtual const char* name() const = 0;

        // The window, on a desktop. Elsewhere there is only the one screen: false, and nothing happens.
        virtual bool setFullscreen(bool) { return false; }
        virtual bool fullscreen() const { return false; }
        virtual void setTitle(const char*) {}
        // Windows 11's frame: dark or light, colours, corners and backdrop. False where it can't be, wholly or in part
        // (Windows 10 has only dark and light, and other systems none of it).
        virtual bool setTitleBar(const TitleBar&) { return false; }
        // Its inside, in pixels, while it isn't full-screen.
        virtual bool setWindowSize(u32, u32) { return false; }
        virtual void setPointer(PointerShape) {}
        virtual bool focused() const { return true; }
        // Waits for it to be answered: true for OK or yes. False at once where there are none.
        virtual bool showMessage(const char*, const char*, bool) { return false; }
        // The system's own clipboard; false where there is none, and the engine keeps one of its own.
        virtual bool clipboard(std::string&) { return false; }
        virtual bool setClipboard(const char*) { return false; }
        // Language and region as the system is set: "en-GB", or "en" where there is no region. Empty if unknown.
        virtual std::string locale() { return std::string(); }
        // Threads that can run at once.
        virtual u32 processors() { return 1; }
    };

    // nullptr on failure, after logging why. Release with nx::destroy.
    Platform* createPlatform();
}
