#pragma once

#include "base/types.h"

// What rooms and the meeting place say to each other: every packet begins with these, then its kind.
namespace nx
{
    namespace wire
    {
        constexpr u32 kMagic = 0x3252584E;   // "NXR2"
        constexpr size_t kPacket = 1200;     // what a room sends at most; passed on, a little more
        constexpr size_t kRelayHeader = 9 + 6;

        enum Kind : u8
        {
            // Between rooms.
            kFind = 1, kHere, kJoin, kWelcome, kFull, kData, kLeave,
            // To and from the meeting place.
            kList = 20,    // a host: here I am, and who is in it
            kUnlist,       // a host: gone
            kAsk,          // a finder: what rooms are there?
            kListed,       // the answer
            kMeet,         // a joiner: introduce me to this host; and, passed on, to the host: someone wants in
            kPunch,        // a host to a joiner, through its own router, so the joiner's packets are let in
            kRelay,        // to the meeting place: pass this on
            kRelayed,      // from it: this came from someone
        };

        inline void put16(u8* p, u16 v)
        {
            p[0] = static_cast<u8>(v);
            p[1] = static_cast<u8>(v >> 8);
        }

        inline void put32(u8* p, u32 v)
        {
            p[0] = static_cast<u8>(v);
            p[1] = static_cast<u8>(v >> 8);
            p[2] = static_cast<u8>(v >> 16);
            p[3] = static_cast<u8>(v >> 24);
        }

        inline u16 get16(const u8* p) { return static_cast<u16>(p[0] | p[1] << 8); }
        inline u32 get32(const u8* p) { return static_cast<u32>(p[0]) | static_cast<u32>(p[1]) << 8 | static_cast<u32>(p[2]) << 16 | static_cast<u32>(p[3]) << 24; }
    }
}
