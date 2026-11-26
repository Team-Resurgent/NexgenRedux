#include "platform/machines.h"

// Not a Mac.
namespace nx::mac
{
    bool available() { return false; }
    std::string text(Text) { return {}; }
    bool open(const char*) { return false; }
    bool reveal(const char*) { return false; }
    bool run(const char*, const char*) { return false; }
    int battery() { return -1; }
    bool charging() { return false; }
    bool darkMode() { return false; }
    std::string clipboard() { return {}; }
    bool setClipboard(const char*) { return false; }
}
