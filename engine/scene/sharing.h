#pragma once

#include "base/grow_array.h"
#include "net/net.h"
#include "scene/world.h"

// Entities kept alike in every copy of a game in a room. Each player shares those that are theirs, and every other
// player has a copy of each, made by its own script, which follows where it is, how it is turned, how it looks and
// which animation it plays. Their messages go on the room's own channel, so a script's never see them.
namespace nx
{
    class Sharing
    {
    public:
        // Makes the copy of something another player shares, of a kind they named, or gives back nullptr not to. What
        // it gives back is held: its refs have been added to.
        using Maker = EntityRef* (*)(void* user, const char* kind, int player);

        // The room's own, made the first time it is asked for; nullptr if there is no room for it.
        static Sharing* of(Room& room);
        static Sharing* find(const Room& room) { return static_cast<Sharing*>(room.sharing()); }

        ~Sharing();

        // Kept alike from now on. False if it is already shared, or a copy, or there are too many.
        bool share(Room& room, EntityRef* entity, const char* kind);
        // No longer: the copies go. It stays as it is here.
        void unshare(Room& room, EntityRef* entity);
        // Once a frame: sends where ours are, now and then, and moves the copies of everyone else's.
        void sync(Room& room, double now, Maker maker, void* user);
        // The player one belongs to: this one for those shared here, another for a copy, -1 for neither.
        int ownerOf(const Room& room, const EntityRef* entity) const;
        u32 shared() const { return m_mine.count(); }
        u32 copies() const { return m_copies.count(); }

        static constexpr u32 kMost = 256;      // shared here, and copies
        static constexpr float kEvery = 0.05f; // seconds between where they are

    private:
        struct Mine
        {
            EntityRef* ref;
            u16 id;
            char kind[24];
        };

        // How one was last said to be.
        struct State
        {
            bool space, visible, flip, loop;
            Vec2 position, velocity;
            float rotation, alpha;
            u16 frame;
            Vec3 place;
            Quat turn;
            int animation;
        };

        struct Copy
        {
            EntityRef* ref;
            u8 player;
            u16 id;
            bool heard;           // a state has come; until then it is hidden
            State target;
            Vec3 placeSpeed;      // in space, worked out from one state to the next
            double heardAt;
        };

        void spawn(Room& room, const Mine& mine, int to);
        void sendStates(Room& room);
        void handle(Room& room, Message& m, Maker maker, void* user);
        void apply(Copy& copy, float dt, bool snap);
        void removeCopy(u32 index);
        void removeCopiesOf(int player);
        int copyOf(int player, u16 id) const;

        GrowArray<Mine, MemTag::General> m_mine;
        GrowArray<Copy, MemTag::General> m_copies;
        u16 m_next = 1;
        u16 m_sequence = 0;
        u16 m_heardSequence[kRoomPlayers] = {};
        bool m_heardAny[kRoomPlayers] = {};
        u8 m_present = 0;     // the others in the room, last time
        int m_me = -1;
        double m_last = 0, m_sent = 0, m_now = 0;
    };
}
