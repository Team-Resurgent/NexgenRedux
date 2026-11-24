#pragma once

#include "base/types.h"

#include <stddef.h>

// The sockets that every platform has. Nothing here waits, but for netResolve.
namespace nx
{
    // An IPv4 address. The first number is the highest byte.
    struct Endpoint
    {
        u32 ip = 0;
        u16 port = 0;

        bool operator==(const Endpoint& o) const { return ip == o.ip && port == o.port; }
    };

    constexpr u32 kLoopback = 0x7F000001;
    constexpr u32 kBroadcast = 0xFFFFFFFF;

    // False where there is no network. Counted: stop as often as it was started.
    bool netStart();
    void netStop();
    // This machine's own address, or 0 while it has none.
    u32 netLocalAddress();
    // "a.b.c.d", with no name to look up.
    bool netParseAddress(const char* text, u32& ip);
    void netFormatAddress(u32 ip, char (&out)[16]);
    // Looks a name up. Waits, so not on the game's thread.
    bool netResolve(const char* host, u32& ip);

    class UdpSocket : NonCopyable
    {
    public:
        ~UdpSocket() { close(); }

        // Port 0 is any that is free. Closed to other machines if it is only for this one.
        bool open(u16 port, bool onlyHere);
        void close();
        bool isOpen() const { return m_socket != kNone; }
        u16 port() const { return m_port; }
        bool send(const Endpoint& to, const void* data, size_t size);
        // 0 when there is nothing.
        size_t receive(Endpoint& from, void* data, size_t size);

    private:
        static constexpr uintptr_t kNone = ~static_cast<uintptr_t>(0);
        uintptr_t m_socket = kNone;
        u16 m_port = 0;
    };

    enum class TcpState : u8 { Closed, Connecting, Open, Listening };

    class TcpSocket : NonCopyable
    {
    public:
        ~TcpSocket() { close(); }

        // Begins to connect; update() says when it has.
        bool connect(const Endpoint& to);
        bool listen(u16 port, bool onlyHere);
        // False when nobody is waiting.
        bool accept(TcpSocket& out, Endpoint& from);
        void close();
        TcpState update();
        TcpState state() const { return m_state; }
        u16 port() const { return m_port; }
        // How much was taken, which may be none of it. Closes itself if the other end has gone.
        size_t send(const void* data, size_t size);
        size_t receive(void* data, size_t size);

    private:
        static constexpr uintptr_t kNone = ~static_cast<uintptr_t>(0);
        uintptr_t m_socket = kNone;
        u16 m_port = 0;
        TcpState m_state = TcpState::Closed;
    };
}
