#include "scene/sharing.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        enum : u8 { kSpawn = 1, kDestroy = 2, kState = 3 };
        enum : u8 { kVisible = 1, kFlip = 2, kSpace = 4, kLoop = 8 };

        // Bytes the same on every machine, whichever way round it keeps them.
        void put8(Message& m, u32 v)
        {
            const u8 b = static_cast<u8>(v);
            m.write(&b, 1);
        }
        void put16(Message& m, u32 v)
        {
            const u8 b[2] = { static_cast<u8>(v), static_cast<u8>(v >> 8) };
            m.write(b, 2);
        }
        u32 get8(Message& m)
        {
            u8 b = 0;
            m.read(&b, 1);
            return b;
        }
        u32 get16(Message& m)
        {
            u8 b[2] = {};
            m.read(b, 2);
            return static_cast<u32>(b[0]) | static_cast<u32>(b[1]) << 8;
        }

        void freeSharing(void* sharing) { destroy(static_cast<Sharing*>(sharing)); }

        float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

        // The shorter way round from one angle to another, in degrees.
        float towards(float from, float to)
        {
            float d = fmodf(to - from, 360.0f);
            if (d > 180)
            {
                d -= 360;
            }
            else if (d < -180)
            {
                d += 360;
            }
            return d;
        }
    }

    Sharing* Sharing::of(Room& room)
    {
        if (Sharing* s = find(room))
        {
            return s;
        }
        Sharing* s = create<Sharing>(MemTag::General);
        if (s)
        {
            room.setSharing(s, freeSharing);
        }
        return s;
    }

    Sharing::~Sharing()
    {
        for (const Mine& mine : m_mine)
        {
            mine.ref->release();
        }
        while (m_copies.count())
        {
            removeCopy(m_copies.count() - 1);
        }
    }

    bool Sharing::share(Room& room, EntityRef* entity, const char* kind)
    {
        if (entity == nullptr || entity->world == nullptr || entity->world->find(entity->id) == nullptr || m_mine.count() >= kMost)
        {
            return false;
        }
        for (const Mine& mine : m_mine)
        {
            if (mine.ref == entity)
            {
                return false;
            }
        }
        for (const Copy& copy : m_copies)
        {
            if (copy.ref == entity)
            {
                return false;
            }
        }
        Mine mine = {};
        mine.ref = entity;
        mine.id = m_next;
        snprintf(mine.kind, sizeof(mine.kind), "%s", kind);
        if (!m_mine.push(mine))
        {
            return false;
        }
        entity->addRef();
        m_next = static_cast<u16>(m_next == 0xFFFF ? 1 : m_next + 1);
        if (room.state() == RoomState::In)
        {
            spawn(room, mine, -1);
        }
        return true;
    }

    void Sharing::unshare(Room& room, EntityRef* entity)
    {
        for (u32 i = 0; i < m_mine.count(); i++)
        {
            if (m_mine[i].ref == entity)
            {
                if (room.state() == RoomState::In)
                {
                    Message m;
                    put8(m, kDestroy);
                    put16(m, m_mine[i].id);
                    room.sendShared(m, true, -1);
                }
                m_mine[i].ref->release();
                m_mine.swapRemove(i);
                return;
            }
        }
    }

    int Sharing::ownerOf(const Room& room, const EntityRef* entity) const
    {
        for (const Mine& mine : m_mine)
        {
            if (mine.ref == entity)
            {
                return room.me() >= 0 ? room.me() : 0;
            }
        }
        for (const Copy& copy : m_copies)
        {
            if (copy.ref == entity)
            {
                return copy.player;
            }
        }
        return -1;
    }

    void Sharing::spawn(Room& room, const Mine& mine, int to)
    {
        Message m;
        put8(m, kSpawn);
        put16(m, mine.id);
        const u32 length = static_cast<u32>(strlen(mine.kind));
        put8(m, length);
        m.write(mine.kind, length);
        room.sendShared(m, true, to);
    }

    void Sharing::sync(Room& room, double now, Maker maker, void* user)
    {
        const float dt = clampf(static_cast<float>(now - m_last), 0, 0.1f);
        m_last = m_now = now;

        if (room.state() != RoomState::In)
        {
            // Out of it: nobody else's are here, and everyone will be told of ours again.
            while (m_copies.count())
            {
                removeCopy(m_copies.count() - 1);
            }
            m_present = 0;
            memset(m_heardAny, 0, sizeof(m_heardAny));
            while (Message* m = room.receiveShared())
            {
                m->release();
            }
            return;
        }
        m_me = room.me();

        // Ours that have gone.
        for (u32 i = 0; i < m_mine.count();)
        {
            EntityRef* ref = m_mine[i].ref;
            if (ref->world == nullptr || ref->world->find(ref->id) == nullptr)
            {
                unshare(room, ref);
                continue;
            }
            i++;
        }

        // Who has come and gone: those who have come are told of ours, and the copies of those who went go.
        u8 present = 0;
        for (u32 p = 0; p < kRoomPlayers; p++)
        {
            if (static_cast<int>(p) != m_me && room.present(static_cast<int>(p)))
            {
                present |= static_cast<u8>(1 << p);
            }
        }
        for (u32 p = 0; p < kRoomPlayers; p++)
        {
            const u8 bit = static_cast<u8>(1 << p);
            if ((present & bit) && !(m_present & bit))
            {
                for (const Mine& mine : m_mine)
                {
                    spawn(room, mine, static_cast<int>(p));
                }
            }
            else if (!(present & bit) && (m_present & bit))
            {
                removeCopiesOf(static_cast<int>(p));
            }
        }
        m_present = present;

        while (Message* m = room.receiveShared())
        {
            handle(room, *m, maker, user);
            m->release();
        }

        // Copies whose entities the script has destroyed are forgotten; the rest follow.
        for (u32 i = 0; i < m_copies.count();)
        {
            Copy& copy = m_copies[i];
            if (copy.ref->world == nullptr || copy.ref->world->find(copy.ref->id) == nullptr)
            {
                copy.ref->release();
                m_copies.swapRemove(i);
                continue;
            }
            if (copy.heard)
            {
                apply(copy, dt, false);
            }
            i++;
        }

        if (now - m_sent >= kEvery || now < m_sent)
        {
            m_sent = now;
            sendStates(room);
        }
    }

    void Sharing::sendStates(Room& room)
    {
        if (m_mine.count() == 0 || m_present == 0)
        {
            return;
        }
        Message m;
        const auto begin = [&]()
        {
            m.size = 0;
            m.full = false;
            put8(m, kState);
            put16(m, ++m_sequence);
        };
        begin();
        u32 entries = 0;
        for (const Mine& mine : m_mine)
        {
            World& world = *mine.ref->world;
            const World::Entity* e = world.find(mine.ref->id);
            if (e == nullptr)
            {
                continue;
            }
            if (m.size + 40 > Message::kMost)
            {
                room.sendShared(m, false, -1);
                begin();
                entries = 0;
            }
            const Spatial* s = world.space().getSpatial(mine.ref->id);
            const Visual* v = world.getVisual(mine.ref->id);
            const Appearance* a = s ? world.space().getAppearance(mine.ref->id) : nullptr;
            u32 flags = (e->visible ? kVisible : 0) | (v && v->flipX ? kFlip : 0) | (s ? kSpace : 0) | (a && a->loop ? kLoop : 0);
            put16(m, mine.id);
            put8(m, flags);
            if (s)
            {
                m.writeFloat(s->place.x);
                m.writeFloat(s->place.y);
                m.writeFloat(s->place.z);
                m.writeFloat(s->turn.x);
                m.writeFloat(s->turn.y);
                m.writeFloat(s->turn.z);
                m.writeFloat(s->turn.w);
                put8(m, a && a->animation >= 0 && a->animation < 254 ? static_cast<u32>(a->animation + 1) : 0);
            }
            else
            {
                const Body* b = world.getBody(mine.ref->id);
                m.writeFloat(e->position.x);
                m.writeFloat(e->position.y);
                m.writeFloat(e->rotation);
                m.writeFloat(b ? b->velocity.x : 0);
                m.writeFloat(b ? b->velocity.y : 0);
                put16(m, v ? v->frame : 0);
                put8(m, v ? static_cast<u32>(clampf(v->color.a, 0, 1) * 255 + 0.5f) : 255);
            }
            entries++;
        }
        if (entries)
        {
            room.sendShared(m, false, -1);
        }
    }

    int Sharing::copyOf(int player, u16 id) const
    {
        for (u32 i = 0; i < m_copies.count(); i++)
        {
            if (m_copies[i].player == player && m_copies[i].id == id)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void Sharing::handle(Room& room, Message& m, Maker maker, void* user)
    {
        (void)room;
        const int from = m.from;
        if (from < 0 || from >= static_cast<int>(kRoomPlayers) || from == m_me)
        {
            return;
        }
        const u32 type = get8(m);
        if (type == kSpawn)
        {
            const u16 id = static_cast<u16>(get16(m));
            const u32 length = get8(m);
            char kind[24] = {};
            if (length >= sizeof(kind) || !m.read(kind, length) || copyOf(from, id) >= 0 || m_copies.count() >= kMost || maker == nullptr)
            {
                return;
            }
            EntityRef* ref = maker(user, kind, from);
            if (ref == nullptr)
            {
                return;
            }
            World::Entity* e = ref->world ? ref->world->find(ref->id) : nullptr;
            Copy copy = {};
            copy.ref = ref;
            copy.player = static_cast<u8>(from);
            copy.id = id;
            if (e == nullptr || !m_copies.push(copy))
            {
                ref->release();
                return;
            }
            e->visible = false;  // until it is known where it is
        }
        else if (type == kDestroy)
        {
            const int at = copyOf(from, static_cast<u16>(get16(m)));
            if (at >= 0)
            {
                removeCopy(static_cast<u32>(at));
            }
        }
        else if (type == kState)
        {
            // Sent once, so they may come out of order: an older one is out of date.
            const u16 sequence = static_cast<u16>(get16(m));
            if (m_heardAny[from] && static_cast<i16>(static_cast<u16>(sequence - m_heardSequence[from])) <= 0)
            {
                return;
            }
            m_heardAny[from] = true;
            m_heardSequence[from] = sequence;
            while (m.more() && !m.short_)
            {
                const u16 id = static_cast<u16>(get16(m));
                const u32 flags = get8(m);
                State t = {};
                t.space = (flags & kSpace) != 0;
                t.visible = (flags & kVisible) != 0;
                t.flip = (flags & kFlip) != 0;
                t.loop = (flags & kLoop) != 0;
                if (t.space)
                {
                    t.place.x = m.readFloat();
                    t.place.y = m.readFloat();
                    t.place.z = m.readFloat();
                    t.turn.x = m.readFloat();
                    t.turn.y = m.readFloat();
                    t.turn.z = m.readFloat();
                    t.turn.w = m.readFloat();
                    t.animation = static_cast<int>(get8(m)) - 1;
                }
                else
                {
                    t.position.x = m.readFloat();
                    t.position.y = m.readFloat();
                    t.rotation = m.readFloat();
                    t.velocity.x = m.readFloat();
                    t.velocity.y = m.readFloat();
                    t.frame = static_cast<u16>(get16(m));
                    t.alpha = static_cast<float>(get8(m)) / 255.0f;
                }
                if (m.short_)
                {
                    return;
                }
                const int at = copyOf(from, id);
                if (at < 0)
                {
                    continue;  // its spawn hasn't come yet
                }
                Copy& copy = m_copies[static_cast<u32>(at)];
                if (copy.heard && t.space)
                {
                    const float apart = static_cast<float>(m_now - copy.heardAt);
                    if (apart > 0.01f && apart < 0.5f)
                    {
                        const Vec3 d = t.place - copy.target.place;
                        copy.placeSpeed = { d.x / apart, d.y / apart, d.z / apart };
                    }
                    else
                    {
                        copy.placeSpeed = {};
                    }
                }
                const bool first = !copy.heard;
                copy.target = t;
                copy.heard = true;
                copy.heardAt = m_now;
                if (first)
                {
                    apply(copy, 0, true);
                }
            }
        }
    }

    // Moves a copy towards where it was last said to be, and on as it was going for a moment after.
    void Sharing::apply(Copy& copy, float dt, bool snap)
    {
        World& world = *copy.ref->world;
        const u32 id = copy.ref->id;
        World::Entity* e = world.find(id);
        if (e == nullptr)
        {
            return;
        }
        State& t = copy.target;
        const bool going = m_now - copy.heardAt < 0.25;
        const float follow = snap ? 1 : clampf(dt * 12, 0, 1);
        e->visible = t.visible;
        if (t.space)
        {
            Spatial* s = world.space().getSpatial(id);
            if (s == nullptr)
            {
                return;
            }
            if (going)
            {
                t.place = t.place + copy.placeSpeed * dt;
            }
            const Vec3 gap = t.place - s->place;
            const float speed = sqrtf(copy.placeSpeed.x * copy.placeSpeed.x + copy.placeSpeed.y * copy.placeSpeed.y + copy.placeSpeed.z * copy.placeSpeed.z);
            const float far = speed * 0.5f + 4;
            if (snap || gap.x * gap.x + gap.y * gap.y + gap.z * gap.z > far * far)
            {
                s->place = t.place;
                s->turn = t.turn;
            }
            else
            {
                s->place = s->place + gap * follow;
                s->turn = Quat::slerp(s->turn, t.turn, follow);
            }
            Appearance* a = world.space().getAppearance(id);
            if (a && t.animation >= 0 && a->model && a->model->model && static_cast<u32>(t.animation) < a->model->model->animations.count() &&
                (a->animation != t.animation || a->loop != t.loop))
            {
                world.space().play(id, a->model->model->animations[static_cast<u32>(t.animation)].name, t.loop, snap ? 0 : 0.2f);
            }
            return;
        }
        if (going)
        {
            t.position = t.position + t.velocity * dt;
        }
        const Vec2 gap = t.position - e->position;
        const float far = sqrtf(t.velocity.x * t.velocity.x + t.velocity.y * t.velocity.y) * 0.5f + 64;
        if (snap || gap.x * gap.x + gap.y * gap.y > far * far)
        {
            e->position = t.position;
            e->rotation = t.rotation;
        }
        else
        {
            e->position = e->position + gap * follow;
            e->rotation += towards(e->rotation, t.rotation) * follow;
        }
        if (Visual* v = world.getVisual(id))
        {
            v->frame = t.frame;
            v->flipX = t.flip;
            v->color.a = t.alpha;
        }
    }

    void Sharing::removeCopy(u32 index)
    {
        EntityRef* ref = m_copies[index].ref;
        if (ref->world)
        {
            ref->world->destroyEntity(ref->id);
        }
        ref->release();
        m_copies.swapRemove(index);
    }

    void Sharing::removeCopiesOf(int player)
    {
        for (u32 i = 0; i < m_copies.count();)
        {
            if (m_copies[i].player == player)
            {
                removeCopy(i);
                continue;
            }
            i++;
        }
        m_heardAny[player] = false;
    }
}
