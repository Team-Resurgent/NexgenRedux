#include "platform/machines.h"

// Not an Xbox 360.
namespace nx::xbox360
{
    bool available() { return false; }
    int temperature(u32) { return -1; }
    Tray tray() { return Tray::Unknown; }
    bool moveTray(bool) { return false; }
    bool setLights(u8) { return false; }
    std::string text(Text) { return {}; }
    bool launch(const char*) { return false; }
    void toDashboard() {}
    void restart() {}
    void powerOff() {}
}
