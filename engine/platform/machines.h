#pragma once

#include "base/types.h"
#include "platform/console.h"

#include <string>

// What one kind of machine can do that others can't, beside the original Xbox's in console.h. Every function is
// there on every platform, so scripts that use them run anywhere: elsewhere available() is false, readings are -1
// or empty, and the rest do nothing.
namespace nx
{
    namespace xbox360
    {
        bool available();
        // Degrees Celsius, or -1. 0 the processor, 1 the graphics, 2 the memory, 3 the board.
        int temperature(u32 which);
        Tray tray();
        bool moveTray(bool open);
        // The ring of light: a bit for each quarter, green in the low four and red in the high; 0 lets the console
        // decide again.
        bool setLights(u8 pattern);
        enum class Text : u8 { Kernel, Region, Language, Video, Player, Count };
        std::string text(Text what);
        // "game:\\Other\\default.xex". False if it isn't there; otherwise this one ends.
        bool launch(const char* path);
        void toDashboard();
        void restart();
        void powerOff();
    }

    namespace xboxone
    {
        // Running on an Xbox One or Series, as a UWP app.
        bool available();
        enum class Text : u8 { Model, System, Count };
        std::string text(Text what);
        // Megabytes the app may use, or 0.
        u32 memoryLimit();
        // Opens something by its address: a web page, the Store, a setting.
        bool openUri(const char* uri);
    }

    // The desktops, each with what it has.
    namespace windows
    {
        bool available();
        enum class Text : u8 { Version, Computer, User, Count };
        std::string text(Text what);
        // A file, folder or address, in what Windows opens it with.
        bool open(const char* what);
        // A file, selected in Explorer.
        bool reveal(const char* path);
        bool run(const char* program, const char* arguments);
        // 0 to 100, or -1 without a battery.
        int battery();
        bool charging();
        bool darkMode();
        std::string clipboard();
        bool setClipboard(const char* text);
    }

    namespace mac
    {
        bool available();
        enum class Text : u8 { Version, Computer, User, Count };
        std::string text(Text what);
        bool open(const char* what);
        // A file, selected in the Finder.
        bool reveal(const char* path);
        // An application, by its name or path, with arguments.
        bool run(const char* application, const char* arguments);
        int battery();
        bool charging();
        bool darkMode();
        std::string clipboard();
        bool setClipboard(const char* text);
    }

    namespace linux_
    {
        bool available();
        enum class Text : u8 { Distribution, Kernel, Desktop, Computer, User, Count };
        std::string text(Text what);
        // With xdg-open.
        bool open(const char* what);
        bool run(const char* program, const char* arguments);
        int battery();
        bool charging();
    }
}
