#include "platform/machines.h"

// Not an Xbox One or Series.
namespace nx::xboxone
{
    bool available() { return false; }
    std::string text(Text) { return {}; }
    u32 memoryLimit() { return 0; }
    bool openUri(const char*) { return false; }
}
