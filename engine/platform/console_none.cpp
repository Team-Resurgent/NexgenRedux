#include "platform/console.h"
#include "net/ftp.h"

namespace nx
{
    bool consoleAvailable() { return false; }
    int consoleTemperature(bool) { return -1; }
    int consoleFan() { return -1; }
    bool consoleSetFan(int) { return false; }
    bool consoleSetLight(u8) { return false; }
    Tray consoleTray() { return Tray::Unknown; }
    bool consoleMoveTray(bool) { return false; }
    int consoleBusRead(u32, u32, bool) { return -1; }
    bool consoleBusWrite(u32, u32, u32, bool) { return false; }
    std::string consoleText(ConsoleText) { return {}; }
    bool consoleDrive(char, u32&, u32&) { return false; }
    void consoleList(const char*, bool, std::vector<std::string>&) {}
    void consoleExit(ConsoleExit) {}
    bool consoleLaunch(const char*) { return false; }
    FtpFiles* consoleFiles() { return nullptr; }
}
