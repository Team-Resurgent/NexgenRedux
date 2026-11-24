#include "net/net.h"
#include "base/log.h"

#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr u32 kMagic = 0x3252584E;   // "NXR2"
        constexpr size_t kPacket = 1200;
        constexpr double kTimeout = 6, kResend = 0.12, kBeat = 0.25;
        constexpr u32 kQueue = 256;          // messages waiting to be told of, or for a player to catch up
        constexpr u8 kEveryone = 0xFF;

        enum Kind : u8 { kFind = 1, kHere, kJoin, kWelcome, kFull, kData, kLeave };

        u32 hashOf(const char* text)
        {
            u32 h = 2166136261u;
            for (; *text; text++)
            {
                h = (h ^ static_cast<u8>(*text)) * 16777619u;
            }
            return h;
        }

        // Names come from other machines: only what can be shown is kept.
        void cleanName(char (&out)[24], const char* name, size_t length)
        {
            size_t n = 0;
            for (size_t i = 0; i < length && name[i] && n + 1 < sizeof(out); i++)
            {
                out[n++] = static_cast<u8>(name[i]) < 32 || name[i] == 127 ? ' ' : name[i];
            }
            out[n] = 0;
        }
    }

    // --- Messages ---

    void Message::write(const void* bytes, u32 count)
    {
        if (count > kMost - size)
        {
            full = true;
            return;
        }
        memcpy(data + size, bytes, count);
        size += count;
    }

    bool Message::read(void* bytes, u32 count)
    {
        if (count > size - at)
        {
            short_ = true;
            at = size;
            memset(bytes, 0, count);
            return false;
        }
        memcpy(bytes, data + at, count);
        at += count;
        return true;
    }

    // The same bytes on every machine, whichever way round it keeps them.
    void Message::writeInt(i32 v)
    {
        const u32 u = static_cast<u32>(v);
        const u8 b[4] = { static_cast<u8>(u), static_cast<u8>(u >> 8), static_cast<u8>(u >> 16), static_cast<u8>(u >> 24) };
        write(b, 4);
    }

    i32 Message::readInt()
    {
        u8 b[4];
        read(b, 4);
        return static_cast<i32>(static_cast<u32>(b[0]) | static_cast<u32>(b[1]) << 8 | static_cast<u32>(b[2]) << 16 | static_cast<u32>(b[3]) << 24);
    }

    void Message::writeFloat(float v)
    {
        i32 bits;
        memcpy(&bits, &v, 4);
        writeInt(bits);
    }

    float Message::readFloat()
    {
        const i32 bits = readInt();
        float v;
        memcpy(&v, &bits, 4);
        // What isn't a number would spread through everything it touched.
        return v - v == 0 ? v : 0;
    }

    void Message::writeText(const std::string& v)
    {
        const u32 length = static_cast<u32>(v.size() < kMost ? v.size() : kMost);
        if (length + 2 > kMost - size)
        {
            full = true;
            return;
        }
        const u8 b[2] = { static_cast<u8>(length), static_cast<u8>(length >> 8) };
        write(b, 2);
        write(v.data(), length);
    }

    std::string Message::readText()
    {
        u8 b[2];
        read(b, 2);
        const u32 length = static_cast<u32>(b[0]) | static_cast<u32>(b[1]) << 8;
        if (length > size - at)
        {
            short_ = true;
            at = size;
            return {};
        }
        std::string text(reinterpret_cast<const char*>(data + at), length);
        at += length;
        return text;
    }

    // --- Packets ---

    struct Room::Writer
    {
        u8 data[kPacket];
        size_t size = 0;

        void bytes(const void* b, size_t count)
        {
            if (count <= kPacket - size)
            {
                memcpy(data + size, b, count);
                size += count;
            }
        }
        void u8_(u8 v) { bytes(&v, 1); }
        void u16_(u16 v)
        {
            const u8 b[2] = { static_cast<u8>(v), static_cast<u8>(v >> 8) };
            bytes(b, 2);
        }
        void u32_(u32 v)
        {
            const u8 b[4] = { static_cast<u8>(v), static_cast<u8>(v >> 8), static_cast<u8>(v >> 16), static_cast<u8>(v >> 24) };
            bytes(b, 4);
        }
        void start(u32 game, u8 kind)
        {
            u32_(kMagic);
            u32_(game);
            u8_(kind);
        }
        void name(const char* text)
        {
            const size_t length = strlen(text) < 23 ? strlen(text) : 23;
            u8_(static_cast<u8>(length));
            bytes(text, length);
        }
    };

    // Says whether everything that was asked for was there, so it is checked once, at the end.
    struct Room::Reader
    {
        const u8* data;
        size_t size, at = 0;
        bool ok = true;

        const u8* bytes(size_t count)
        {
            if (count > size - at)
            {
                ok = false;
                at = size;
                return nullptr;
            }
            const u8* p = data + at;
            at += count;
            return p;
        }
        u8 u8_()
        {
            const u8* b = bytes(1);
            return b ? b[0] : u8(0);
        }
        u16 u16_()
        {
            const u8* b = bytes(2);
            return b ? static_cast<u16>(b[0] | b[1] << 8) : u16(0);
        }
        u32 u32_()
        {
            const u8* b = bytes(4);
            return b ? static_cast<u32>(b[0]) | static_cast<u32>(b[1]) << 8 | static_cast<u32>(b[2]) << 16 | static_cast<u32>(b[3]) << 24 : 0;
        }
        void name(char (&out)[24])
        {
            const size_t length = u8_();
            const u8* b = bytes(length);
            cleanName(out, b ? reinterpret_cast<const char*>(b) : "", b ? length : 0);
        }
    };

    // --- Coming and going ---

    Room::Room(Net& net) : m_net(net)
    {
        net.m_rooms.push(this);
    }

    Room::~Room()
    {
        leave();
        Net::remove(m_net.m_rooms, this);
    }

    u32 Room::random()
    {
        if (m_random == 0)
        {
            const double now = m_net.now();
            u64 bits;
            memcpy(&bits, &now, sizeof(bits));
            m_random = static_cast<u32>(bits ^ bits >> 32) ^ static_cast<u32>(reinterpret_cast<uintptr_t>(this) >> 3) * 2654435761u ^ netLocalAddress();
            m_random = m_random ? m_random : 1;
        }
        m_random ^= m_random << 13;
        m_random ^= m_random >> 17;
        m_random ^= m_random << 5;
        return m_random;
    }

    bool Room::open(u16 port)
    {
        return m_net.start() && m_socket.open(port, m_net.onlyHere());
    }

    void Room::closeLink(Link& link)
    {
        if (link.sure)
        {
            for (Out& o : *link.sure)
            {
                o.message->release();
            }
            destroy(link.sure);
        }
        for (Message* m : link.early)
        {
            if (m)
            {
                m->release();
            }
        }
        link = {};
    }

    void Room::reset()
    {
        for (Link& link : m_links)
        {
            closeLink(link);
        }
        for (Message* m : m_inbox)
        {
            m->release();
        }
        m_inbox.clear();
        m_socket.close();
        memset(m_players, 0, sizeof(m_players));
        m_hosting = false;
        m_me = -1;
        m_arrived = m_departed = 0;
        m_state = RoomState::Idle;
    }

    void Room::leave()
    {
        if (m_state == RoomState::In)
        {
            for (Link& link : m_links)
            {
                // More than once: nobody says whether it came.
                for (u32 i = 0; link.used && i < 3; i++)
                {
                    Writer out;
                    out.start(m_game, kLeave);
                    out.u32_(link.token);
                    m_socket.send(link.where, out.data, out.size);
                }
            }
        }
        reset();
    }

    bool Room::host(const char* game, const char* name, u32 most)
    {
        leave();
        m_game = hashOf(game);
        m_most = most < 1 ? 1 : most > kRoomPlayers ? kRoomPlayers : most;
        bool opened = false;
        for (u16 i = 0; i < 4 && !opened; i++)
        {
            opened = open(static_cast<u16>(kRoomPort + i));
        }
        if (!opened)
        {
            return false;
        }
        cleanName(m_name, name, strlen(name));
        m_hosting = true;
        m_me = 0;
        m_state = RoomState::In;
        m_players[0].present = true;
        memcpy(m_players[0].name, m_name, sizeof(m_name));
        return true;
    }

    bool Room::find(const char* game)
    {
        leave();
        m_game = hashOf(game);
        m_found.clear();
        if (!open(0))
        {
            return false;
        }
        m_state = RoomState::Finding;
        m_asked = -10;
        return true;
    }

    bool Room::join(const char* game, const Endpoint& where, const char* name)
    {
        const u32 hash = hashOf(game);
        if (m_state != RoomState::Finding || m_game != hash)
        {
            leave();
            m_game = hash;
            if (!open(0))
            {
                return false;
            }
        }
        cleanName(m_name, name, strlen(name));
        m_server = where;
        m_server.port = where.port ? where.port : kRoomPort;
        m_nonce = random();
        m_state = RoomState::Joining;
        m_asked = -10;
        m_began = -1;   // when it is next updated
        return true;
    }

    u32 Room::players() const
    {
        u32 count = 0;
        for (const Player& p : m_players)
        {
            count += p.present;
        }
        return m_state == RoomState::In ? count : 0;
    }

    int Room::ping(int player) const
    {
        if (!present(player) || player == m_me)
        {
            return 0;
        }
        return static_cast<int>(m_links[m_hosting ? player : 0].ping * 1000);
    }

    // --- Messages ---

    void Room::enqueue(Link& link, const Message& message, u8 origin, u8 to, bool internal)
    {
        if (!link.used)
        {
            return;
        }
        if (link.sure == nullptr)
        {
            link.sure = create<GrowArray<Out, MemTag::General>>(MemTag::General);
        }
        Message* copy = create<Message>(MemTag::General);
        if (link.sure == nullptr || copy == nullptr)
        {
            return;
        }
        *copy = message;
        copy->refs = 1;
        link.sure->push({ copy, link.nextOut++, origin, to, internal, -10 });
    }

    bool Room::send(const Message& message, bool sure, int to)
    {
        if (m_state != RoomState::In || message.full || to == m_me || (to != -1 && !present(to)))
        {
            return false;
        }
        const u8 whom = to < 0 ? kEveryone : static_cast<u8>(to);
        const u8 me = static_cast<u8>(m_me);
        for (u32 i = 0; i < kRoomPlayers; i++)
        {
            Link& link = m_links[i];
            if (!link.used || (m_hosting && to >= 0 && static_cast<int>(i) != to))
            {
                continue;
            }
            if (sure)
            {
                enqueue(link, message, me, whom, false);
            }
            else
            {
                flush(link, &message, me, whom);
            }
        }
        return true;
    }

    Message* Room::receive()
    {
        if (m_inbox.count() == 0)
        {
            return nullptr;
        }
        Message* first = m_inbox[0];
        for (u32 i = 1; i < m_inbox.count(); i++)
        {
            m_inbox[i - 1] = m_inbox[i];
        }
        m_inbox.resize(m_inbox.count() - 1);
        return first;
    }

    void Room::deliver(Message* message, int from)
    {
        message->from = from;
        message->at = 0;
        if (m_inbox.count() >= kQueue || !m_inbox.push(message))
        {
            message->release();  // nobody is listening
        }
    }

    // Everything there is for a player that they haven't said they have, and one that is sent once.
    void Room::flush(Link& link, const Message* quick, u8 origin, u8 to)
    {
        const auto begin = [&](Writer& out)
        {
            out.start(m_game, kData);
            out.u32_(link.token);
            out.u16_(link.nextIn);
            out.u32_(static_cast<u32>(m_now * 1000));
            // Their own time back, and how long it was kept.
            out.u32_(link.theirTime);
            out.u16_(static_cast<u16>(link.theirTimeAt > 0 ? (m_now - link.theirTimeAt) * 1000 : 0));
        };
        const auto add = [](Writer& out, const Message& m, u8 flags, u16 number, u8 from, u8 whom)
        {
            out.u8_(flags);
            out.u16_(number);
            out.u8_(from);
            out.u8_(whom);
            out.u16_(static_cast<u16>(m.size));
            out.bytes(m.data, m.size);
        };
        Writer out;
        begin(out);
        bool any = false;
        if (quick)
        {
            add(out, *quick, 0, 0, origin, to);
            any = true;
        }
        u32 packets = 0;
        for (u32 i = 0; link.sure && i < link.sure->count() && packets < 4; i++)
        {
            Out& o = (*link.sure)[i];
            const double wait = link.ping * 1.5 > kResend ? link.ping * 1.5 : kResend;
            if (m_now - o.sent < wait || i >= 32)
            {
                continue;
            }
            if (out.size + 7 + o.message->size > kPacket)
            {
                m_socket.send(link.where, out.data, out.size);
                packets++;
                out.size = 0;
                begin(out);
            }
            add(out, *o.message, static_cast<u8>(1 | (o.internal ? 2 : 0)), o.number, o.origin, o.to);
            o.sent = m_now;
            any = true;
        }
        if (any || m_now - link.sent > kBeat)
        {
            m_socket.send(link.where, out.data, out.size);
            link.sent = m_now;
        }
    }

    void Room::sendPlayers()
    {
        Message m;
        m.writeInt(static_cast<i32>(m_most));
        for (const Player& p : m_players)
        {
            m.writeInt(p.present);
            m.writeText(p.name);
        }
        for (Link& link : m_links)
        {
            enqueue(link, m, 0, kEveryone, true);
        }
    }

    void Room::setPlayers(Message& m)
    {
        m.at = 0;
        const i32 most = m.readInt();
        m_most = most < 1 ? 1 : most > static_cast<i32>(kRoomPlayers) ? kRoomPlayers : static_cast<u32>(most);
        for (u32 i = 0; i < kRoomPlayers; i++)
        {
            const bool present = m.readInt() != 0;
            const std::string name = m.readText();
            if (m.short_)
            {
                return;
            }
            Player& p = m_players[i];
            if (present != p.present && static_cast<int>(i) != m_me)
            {
                (present ? m_arrived : m_departed) |= static_cast<u8>(1 << i);
            }
            p.present = present;
            cleanName(p.name, name.c_str(), name.size());
        }
    }

    // One whose turn it is.
    void Room::accept(Link& link, u32 index, Message* message, u8 origin, u8 to, bool internal)
    {
        (void)link;
        if (internal)
        {
            if (!m_hosting)
            {
                setPlayers(*message);
            }
            message->release();
            return;
        }
        // The host knows who it came from, whatever it says.
        const u8 from = m_hosting ? static_cast<u8>(index) : origin;
        if (from >= kRoomPlayers)
        {
            message->release();
            return;
        }
        if (m_hosting)
        {
            for (u32 i = 1; i < kRoomPlayers; i++)
            {
                if (i != index && m_links[i].used && (to == kEveryone || to == i))
                {
                    enqueue(m_links[i], *message, from, to, false);
                }
            }
        }
        if (to == kEveryone || to == m_me)
        {
            deliver(message, from);
        }
        else
        {
            message->release();
        }
    }

    void Room::handleData(Link& link, u32 index, Reader& in)
    {
        const u16 theyHave = in.u16_();
        const u32 theirTime = in.u32_();
        const u32 mine = in.u32_();
        const u16 kept = in.u16_();
        if (!in.ok)
        {
            return;
        }
        link.heard = m_now;
        link.theirTime = theirTime;
        link.theirTimeAt = m_now;
        if (mine)
        {
            const float there = (static_cast<float>(static_cast<u32>(m_now * 1000) - mine) - static_cast<float>(kept)) / 1000;
            if (there >= 0 && there < 5)
            {
                link.ping = link.ping > 0 ? link.ping * 0.9f + there * 0.1f : there;
            }
        }
        if (link.sure)
        {
            // What they have needn't be sent again.
            GrowArray<Out, MemTag::General>& sure = *link.sure;
            u32 done = 0;
            while (done < sure.count() && static_cast<i16>(static_cast<u16>(theyHave - sure[done].number)) > 0)
            {
                sure[done++].message->release();
            }
            for (u32 i = done; done && i < sure.count(); i++)
            {
                sure[i - done] = sure[i];
            }
            sure.resize(sure.count() - done);
        }
        while (in.ok && in.at < in.size)
        {
            const u8 flags = in.u8_();
            const u16 number = in.u16_();
            const u8 origin = in.u8_(), to = in.u8_();
            const u16 length = in.u16_();
            const u8* bytes = in.bytes(length);
            if (bytes == nullptr || length > Message::kMost)
            {
                return;
            }
            const i16 ahead = static_cast<i16>(static_cast<u16>(number - link.nextIn));
            if ((flags & 1) && (ahead < 0 || ahead >= 32 || (ahead > 0 && link.early[number % 32])))
            {
                continue;  // it came before, or far too early
            }
            Message* m = create<Message>(MemTag::General);
            if (m == nullptr)
            {
                return;
            }
            memcpy(m->data, bytes, length);
            m->size = length;
            const bool internal = (flags & 2) != 0;
            if (!(flags & 1))
            {
                // Sent once: passed on at once.
                const u8 from = m_hosting ? static_cast<u8>(index) : origin;
                if (m_hosting)
                {
                    for (u32 i = 1; i < kRoomPlayers; i++)
                    {
                        if (i != index && m_links[i].used && (to == kEveryone || to == i))
                        {
                            flush(m_links[i], m, from, to);
                        }
                    }
                }
                if (from < kRoomPlayers && (to == kEveryone || to == m_me))
                {
                    deliver(m, from);
                }
                else
                {
                    m->release();
                }
                continue;
            }
            if (ahead > 0)
            {
                link.early[number % 32] = m;
                link.earlyOrigin[number % 32] = origin;
                link.earlyTo[number % 32] = to;
                link.earlyInternal[number % 32] = internal;
                continue;
            }
            link.nextIn++;
            accept(link, index, m, origin, to, internal);
            while (Message* next = link.early[link.nextIn % 32])
            {
                const u32 slot = link.nextIn % 32;
                link.early[slot] = nullptr;
                link.nextIn++;
                accept(link, index, next, link.earlyOrigin[slot], link.earlyTo[slot], link.earlyInternal[slot]);
            }
        }
    }

    void Room::drop(u32 index)
    {
        closeLink(m_links[index]);
        if (m_hosting)
        {
            m_players[index] = {};
            m_departed |= static_cast<u8>(1 << index);
            sendPlayers();
        }
        else
        {
            m_state = RoomState::Lost;
            m_socket.close();
        }
    }

    void Room::handle(const Endpoint& from, const u8* data, size_t size)
    {
        Reader in{ data, size };
        if (in.u32_() != kMagic || in.u32_() != m_game || !in.ok)
        {
            return;
        }
        const u8 kind = in.u8_();
        if (kind == kFind && m_hosting)
        {
            Writer out;
            out.start(m_game, kHere);
            out.name(m_name);
            out.u8_(static_cast<u8>(players()));
            out.u8_(static_cast<u8>(m_most));
            m_socket.send(from, out.data, out.size);
        }
        else if (kind == kHere && m_state == RoomState::Finding)
        {
            Found f = {};
            f.where = from;
            in.name(f.name);
            f.players = in.u8_();
            f.most = in.u8_();
            f.heard = m_now;
            if (!in.ok || f.players > kRoomPlayers || f.most > kRoomPlayers)
            {
                return;
            }
            for (Found& old : m_found)
            {
                // The same one may be heard from more than one address.
                if (old.where == f.where || (old.where.port == f.where.port && strcmp(old.name, f.name) == 0 && (old.where.ip == kLoopback || f.where.ip == kLoopback)))
                {
                    const Endpoint where = old.where.ip == kLoopback ? f.where : old.where;
                    old = f;
                    old.where = where;
                    return;
                }
            }
            if (m_found.count() < 32)
            {
                m_found.push(f);
            }
        }
        else if (kind == kJoin && m_hosting)
        {
            const u32 nonce = in.u32_();
            char name[24];
            in.name(name);
            if (!in.ok)
            {
                return;
            }
            u32 slot = kRoomPlayers;
            for (u32 i = 1; i < m_most; i++)
            {
                // Asking again, if the answer was lost, is the same player.
                if (m_links[i].used && m_links[i].where == from)
                {
                    slot = i;
                    break;
                }
                slot = slot == kRoomPlayers && !m_links[i].used ? i : slot;
            }
            Writer out;
            if (slot == kRoomPlayers)
            {
                out.start(m_game, kFull);
                out.u32_(nonce);
                m_socket.send(from, out.data, out.size);
                return;
            }
            Link& link = m_links[slot];
            const bool fresh = !link.used;
            if (fresh)
            {
                link.used = true;
                link.where = from;
                link.token = random();
                link.heard = link.sent = m_now;
                link.nextIn = link.nextOut = 1;
                m_players[slot].present = true;
                memcpy(m_players[slot].name, name, sizeof(name));
                m_arrived |= static_cast<u8>(1 << slot);
            }
            out.start(m_game, kWelcome);
            out.u32_(nonce);
            out.u32_(link.token);
            out.u8_(static_cast<u8>(slot));
            m_socket.send(from, out.data, out.size);
            if (fresh)
            {
                sendPlayers();
            }
        }
        else if ((kind == kWelcome || kind == kFull) && m_state == RoomState::Joining && from == m_server)
        {
            if (in.u32_() != m_nonce)
            {
                return;
            }
            const u32 token = in.u32_();
            const u8 slot = in.u8_();
            if (kind == kFull || !in.ok || slot == 0 || slot >= kRoomPlayers)
            {
                m_state = RoomState::Lost;
                m_socket.close();
                return;
            }
            Link& link = m_links[0];
            link.used = true;
            link.where = from;
            link.token = token;
            link.heard = link.sent = m_now;
            link.nextIn = link.nextOut = 1;
            m_me = slot;
            m_players[slot].present = true;
            memcpy(m_players[slot].name, m_name, sizeof(m_name));
            m_state = RoomState::In;
        }
        else if ((kind == kData || kind == kLeave) && m_state == RoomState::In)
        {
            const u32 token = in.u32_();
            for (u32 i = 0; i < kRoomPlayers; i++)
            {
                Link& link = m_links[i];
                if (link.used && link.where == from && link.token == token && in.ok)
                {
                    if (kind == kLeave)
                    {
                        drop(i);
                    }
                    else
                    {
                        handleData(link, i, in);
                    }
                    return;
                }
            }
        }
    }

    void Room::update(double now)
    {
        m_now = now;
        if (m_state == RoomState::Idle || m_state == RoomState::Lost)
        {
            return;
        }
        u8 data[kPacket + 64];
        for (u32 i = 0; i < 256; i++)
        {
            Endpoint from;
            const size_t size = m_socket.receive(from, data, sizeof(data));
            if (size == 0)
            {
                break;
            }
            if (size <= kPacket)
            {
                handle(from, data, size);
            }
            if (!m_socket.isOpen())
            {
                return;
            }
        }
        if (m_state == RoomState::Finding && now - m_asked > 1)
        {
            m_asked = now;
            Writer out;
            out.start(m_game, kFind);
            for (u16 i = 0; i < 4; i++)
            {
                const u16 port = static_cast<u16>(kRoomPort + i);
                if (!m_net.onlyHere())
                {
                    m_socket.send({ kBroadcast, port }, out.data, out.size);
                }
                m_socket.send({ kLoopback, port }, out.data, out.size);
            }
            for (u32 i = 0; i < m_found.count();)
            {
                if (now - m_found[i].heard > 4)
                {
                    m_found.swapRemove(i);
                }
                else
                {
                    i++;
                }
            }
        }
        else if (m_state == RoomState::Joining)
        {
            m_began = m_began < 0 ? now : m_began;
            if (now - m_began > kTimeout)
            {
                m_state = RoomState::Lost;
                m_socket.close();
                return;
            }
            if (now - m_asked > 0.4)
            {
                m_asked = now;
                Writer out;
                out.start(m_game, kJoin);
                out.u32_(m_nonce);
                out.name(m_name);
                m_socket.send(m_server, out.data, out.size);
            }
        }
        else if (m_state == RoomState::In)
        {
            for (u32 i = 0; i < kRoomPlayers; i++)
            {
                Link& link = m_links[i];
                if (!link.used)
                {
                    continue;
                }
                if (now - link.heard > kTimeout || (link.sure && link.sure->count() > kQueue))
                {
                    drop(i);
                    if (!m_hosting)
                    {
                        return;
                    }
                    continue;
                }
                flush(link, nullptr, 0, 0);
            }
        }
    }
}
