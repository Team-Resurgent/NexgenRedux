#include "net/socket.h"

#include <stdio.h>
#include <stdlib.h>

namespace nx
{
    bool netParseAddress(const char* text, u32& ip)
    {
        u32 parts[4];
        const char* at = text;
        for (u32 i = 0; i < 4; i++)
        {
            char* end = nullptr;
            if (*at < '0' || *at > '9')
            {
                return false;
            }
            const unsigned long v = strtoul(at, &end, 10);
            if (v > 255 || *end != (i < 3 ? '.' : '\0'))
            {
                return false;
            }
            parts[i] = static_cast<u32>(v);
            at = end + 1;
        }
        ip = parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3];
        return true;
    }

    void netFormatAddress(u32 ip, char (&out)[16])
    {
        snprintf(out, sizeof(out), "%u.%u.%u.%u", static_cast<unsigned>(ip >> 24), static_cast<unsigned>(ip >> 16 & 255), static_cast<unsigned>(ip >> 8 & 255), static_cast<unsigned>(ip & 255));
    }
}
