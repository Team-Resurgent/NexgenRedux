#pragma once

#include "base/types.h"

namespace nx
{
    // File formats are little endian everywhere. Reading through these gives the same result on
    // the big-endian Xbox 360, and never relies on alignment.
    inline u16 readU16(const u8* p)
    {
        return static_cast<u16>(p[0] | (p[1] << 8));
    }

    inline u32 readU32(const u8* p)
    {
        return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) | (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24);
    }

    inline u64 readU64(const u8* p)
    {
        return static_cast<u64>(readU32(p)) | (static_cast<u64>(readU32(p + 4)) << 32);
    }

    inline void writeU16(u8* p, u16 v)
    {
        p[0] = static_cast<u8>(v);
        p[1] = static_cast<u8>(v >> 8);
    }

    inline void writeU32(u8* p, u32 v)
    {
        writeU16(p, static_cast<u16>(v));
        writeU16(p + 2, static_cast<u16>(v >> 16));
    }

    inline void writeU64(u8* p, u64 v)
    {
        writeU32(p, static_cast<u32>(v));
        writeU32(p + 4, static_cast<u32>(v >> 32));
    }
}
