#pragma once

#include "base/types.h"

#include <string>
#include <vector>

// What only the original Xbox has. Elsewhere consoleAvailable() is false, and the rest do nothing.
namespace nx
{
    bool consoleAvailable();

    // Degrees Celsius, or -1.
    int consoleTemperature(bool processor);
    // 0 to 100, or -1.
    int consoleFan();
    // -1 lets the console decide.
    bool consoleSetFan(int percent);
    // Four moments, red in the high bits and green in the low; 0 lets the console decide.
    bool consoleSetLight(u8 pattern);

    enum class Tray : u8 { Closed, Open, Moving, Empty, Unknown };
    Tray consoleTray();
    bool consoleMoveTray(bool open);

    // Anything on the bus that the sensors are on. -1 if it couldn't be read. Not the memory that holds the
    // console's keys and settings: a mistake there can't be undone.
    int consoleBusRead(u32 address, u32 command, bool word);
    bool consoleBusWrite(u32 address, u32 command, u32 value, bool word);

    enum class ConsoleText : u8 { Kernel, Hardware, Serial, Ethernet, Region, Video, Cable, Count };
    std::string consoleText(ConsoleText what);

    // Megabytes. False if there is no such drive.
    bool consoleDrive(char letter, u32& total, u32& free);
    // Names in a folder such as "E:\Games".
    void consoleList(const char* path, bool folders, std::vector<std::string>& out);

    enum class ConsoleExit : u8 { Firmware, Restart, Dashboard, Off };
    // Don't come back, if they work.
    void consoleExit(ConsoleExit how);
    // "E:\Games\Thing\default.xbe". False if it isn't there.
    bool consoleLaunch(const char* path);
}
