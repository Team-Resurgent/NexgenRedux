#include "net/net.h"
#include "net/wire.h"
#include "base/log.h"

#include <string.h>

// Somewhere every player can reach: hosts say they are there, finders ask what there is, and two who want to play
// are introduced, so that each one's router lets the other's packets in. When that isn't enough, what they send is
// passed on, but only between two who were introduced, and only so much a second.
namespace nx
{
    using namespace wire;

    namespace
    {
        constexpr double kListingLife = 8, kPairLife = 30;
        constexpr u32 kMostListings = 1024, kMostPairs = 4096;
        constexpr u32 kPacketsASecond = 600;           // from any one place
        constexpr u32 kBytesASecond = 256 * 1024;      // passed on between any two
        constexpr u32 kListedAtOnce = 16;              // rooms in one answer

        u32 slotOf(const Endpoint& e) { return (e.ip * 2654435761u ^ e.port * 40503u) >> 24; }
    }

    bool MeetingPlace::open(u16 port, bool onlyHere)
    {
        if (!m_started)
        {
            m_started = netStart();
        }
        return m_started && m_socket.open(port, onlyHere);
    }

    void MeetingPlace::close()
    {
        m_socket.close();
        m_listings.clear();
        m_pairs.clear();
        if (m_started)
        {
            netStop();
            m_started = false;
        }
    }

    bool MeetingPlace::allowed(const Endpoint& from)
    {
        Sender& s = m_senders[slotOf(from)];
        if (m_now - s.window >= 1)
        {
            s.window = m_now;
            s.packets = 0;
        }
        return ++s.packets <= kPacketsASecond;
    }

    MeetingPlace::Pair* MeetingPlace::pairOf(const Endpoint& a, const Endpoint& b)
    {
        for (Pair& p : m_pairs)
        {
            if ((p.a == a && p.b == b) || (p.a == b && p.b == a))
            {
                return &p;
            }
        }
        return nullptr;
    }

    void MeetingPlace::handle(const Endpoint& from, const u8* data, size_t size)
    {
        if (size < 9 || get32(data) != kMagic || !allowed(from))
        {
            return;
        }
        const u32 game = get32(data + 4);
        const u8 kind = data[8];
        const u8* p = data + 9;
        const size_t left = size - 9;
        if (kind == kList && left >= 4 + 1 + 2 + 4 + 2)
        {
            // key, name, players, most, and where it is on its own network.
            const size_t length = p[4] < 23 ? p[4] : 23;
            if (left < 5 + length + 2 + 6)
            {
                return;
            }
            Listing l = {};
            l.game = game;
            l.where = from;
            memcpy(l.name, p + 5, length);
            l.players = p[5 + length];
            l.most = p[6 + length];
            l.near.ip = get32(p + 7 + length);
            l.near.port = get16(p + 11 + length);
            l.heard = m_now;
            for (Listing& old : m_listings)
            {
                if (old.where == from && old.game == game)
                {
                    old = l;
                    return;
                }
            }
            if (m_listings.count() < kMostListings)
            {
                m_listings.push(l);
                NX_INFO("meeting: room '%s' listed (%u)", l.name, m_listings.count());
            }
        }
        else if (kind == kUnlist)
        {
            for (u32 i = 0; i < m_listings.count(); i++)
            {
                if (m_listings[i].where == from && m_listings[i].game == game)
                {
                    m_listings.swapRemove(i);
                    return;
                }
            }
        }
        else if (kind == kAsk)
        {
            // In packets of a few at a time.
            u8 out[kPacket];
            size_t at = 0;
            u32 count = 0, sent = 0;
            const auto start = [&]()
            {
                put32(out, kMagic);
                put32(out + 4, game);
                out[8] = kListed;
                out[9] = 0;
                at = 10;
                count = 0;
            };
            start();
            for (const Listing& l : m_listings)
            {
                if (l.game != game || l.where == from)
                {
                    continue;
                }
                const size_t length = strlen(l.name);
                put32(out + at, l.where.ip);
                put16(out + at + 4, l.where.port);
                put32(out + at + 6, l.near.ip);
                put16(out + at + 10, l.near.port);
                out[at + 12] = static_cast<u8>(length);
                memcpy(out + at + 13, l.name, length);
                out[at + 13 + length] = l.players;
                out[at + 14 + length] = l.most;
                at += 15 + length;
                out[9] = static_cast<u8>(++count);
                if (count == kListedAtOnce)
                {
                    m_socket.send(from, out, at);
                    if (++sent == 4)
                    {
                        return;
                    }
                    start();
                }
            }
            if (count || sent == 0)
            {
                m_socket.send(from, out, at);
            }
        }
        else if (kind == kMeet && left >= 6)
        {
            // Only to a room that is listed, of the same game.
            Endpoint host;
            host.ip = get32(p);
            host.port = get16(p + 4);
            bool listed = false;
            for (const Listing& l : m_listings)
            {
                listed = listed || (l.where == host && l.game == game);
            }
            if (!listed)
            {
                return;
            }
            Pair* pair = pairOf(from, host);
            if (pair == nullptr && m_pairs.count() < kMostPairs)
            {
                m_pairs.push({ from, host, m_now, m_now, 0 });
            }
            else if (pair)
            {
                pair->heard = m_now;
            }
            u8 out[15];
            put32(out, kMagic);
            put32(out + 4, game);
            out[8] = kMeet;
            put32(out + 9, from.ip);
            put16(out + 13, from.port);
            m_socket.send(host, out, sizeof(out));
        }
        else if (kind == kRelay && left >= 6 && left - 6 <= kPacket)
        {
            Endpoint to;
            to.ip = get32(p);
            to.port = get16(p + 4);
            Pair* pair = pairOf(from, to);
            if (pair == nullptr)
            {
                return;
            }
            if (m_now - pair->window >= 1)
            {
                pair->window = m_now;
                pair->bytes = 0;
            }
            pair->bytes += static_cast<u32>(left);
            if (pair->bytes > kBytesASecond)
            {
                return;
            }
            pair->heard = m_now;
            u8 out[kPacket + kRelayHeader];
            put32(out, kMagic);
            put32(out + 4, game);
            out[8] = kRelayed;
            put32(out + 9, from.ip);
            put16(out + 13, from.port);
            memcpy(out + kRelayHeader, p + 6, left - 6);
            m_socket.send(to, out, kRelayHeader + left - 6);
            m_passed += left - 6;
        }
    }

    void MeetingPlace::update(double now)
    {
        m_now = now;
        if (!m_socket.isOpen())
        {
            return;
        }
        u8 data[kPacket + kRelayHeader + 64];
        for (u32 i = 0; i < 4096; i++)
        {
            Endpoint from;
            const size_t size = m_socket.receive(from, data, sizeof(data));
            if (size == 0)
            {
                break;
            }
            if (size <= kPacket + kRelayHeader)
            {
                handle(from, data, size);
            }
        }
        for (u32 i = 0; i < m_listings.count();)
        {
            if (now - m_listings[i].heard > kListingLife)
            {
                m_listings.swapRemove(i);
            }
            else
            {
                i++;
            }
        }
        for (u32 i = 0; i < m_pairs.count();)
        {
            if (now - m_pairs[i].heard > kPairLife)
            {
                m_pairs.swapRemove(i);
            }
            else
            {
                i++;
            }
        }
    }
}
