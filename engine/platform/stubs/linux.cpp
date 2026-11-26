#include "platform/machines.h"

// Not Linux.
namespace nx::linux_
{
    bool available() { return false; }
    std::string text(Text) { return {}; }
    bool open(const char*) { return false; }
    bool run(const char*, const char*) { return false; }
    int battery() { return -1; }
    bool charging() { return false; }
}
