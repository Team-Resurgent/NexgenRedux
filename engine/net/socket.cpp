#include "net/socket.h"
#include "base/log.h"
#include "base/thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// One set of calls, with small differences in what they are named.
#if defined(_XBOX)
#include <xtl.h>
#include <winsockx.h>
#define NX_WINSOCK 1
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define NX_WINSOCK 1
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define NX_WINSOCK 0
#endif

namespace nx
{
    namespace
    {
        int g_started = 0;
        Mutex g_lock;   // requests are made from more than one thread

#if NX_WINSOCK
        using Handle32 = SOCKET;
        using Length = int;
        void closeSocket(Handle32 s) { closesocket(s); }
        bool wouldWait() { return WSAGetLastError() == WSAEWOULDBLOCK; }
        bool connecting() { return WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINPROGRESS; }
        bool dontWait(Handle32 s)
        {
            u_long on = 1;
            return ioctlsocket(s, FIONBIO, &on) == 0;
        }
        constexpr int kSendFlags = 0;
#else
        using Handle32 = int;
        using Length = socklen_t;
        void closeSocket(Handle32 s) { ::close(s); }
        bool wouldWait() { return errno == EWOULDBLOCK || errno == EAGAIN; }
        bool connecting() { return errno == EINPROGRESS; }
        bool dontWait(Handle32 s)
        {
            const int flags = fcntl(s, F_GETFL, 0);
            return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
        }
#if defined(MSG_NOSIGNAL)
        constexpr int kSendFlags = MSG_NOSIGNAL;
#else
        constexpr int kSendFlags = 0;
#endif
#endif

        // Whether one that can be written to got there.
        bool connected(Handle32 s)
        {
#if defined(_XBOX)
            (void)s;  // it would have failed instead
            return true;
#else
            int error = 0;
            Length length = sizeof(error);
            return getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && error == 0;
#endif
        }

        Handle32 handle(uintptr_t s) { return static_cast<Handle32>(s); }

        uintptr_t make(int type, int protocol)
        {
            const Handle32 s = ::socket(AF_INET, type, protocol);
#if NX_WINSOCK
            if (s == INVALID_SOCKET)
#else
            if (s < 0)
#endif
            {
                return ~static_cast<uintptr_t>(0);
            }
            if (!dontWait(s))
            {
                closeSocket(s);
                return ~static_cast<uintptr_t>(0);
            }
#if defined(SO_NOSIGPIPE)
            const int one = 1;
            setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
            return static_cast<uintptr_t>(s);
        }

        sockaddr_in address(const Endpoint& e)
        {
            sockaddr_in a;
            memset(&a, 0, sizeof(a));
            a.sin_family = AF_INET;
            a.sin_addr.s_addr = htonl(e.ip);
            a.sin_port = htons(e.port);
            return a;
        }

        Endpoint endpoint(const sockaddr_in& a)
        {
            return { static_cast<u32>(ntohl(a.sin_addr.s_addr)), static_cast<u16>(ntohs(a.sin_port)) };
        }

        u16 bindTo(uintptr_t s, u16 port, bool onlyHere)
        {
            sockaddr_in a = address({ onlyHere ? kLoopback : 0, port });
#if defined(_XBOX)
            a.sin_addr.s_addr = INADDR_ANY;  // there is no loopback to bind to
#endif
            if (::bind(handle(s), reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0)
            {
                return 0;
            }
            Length length = sizeof(a);
            return getsockname(handle(s), reinterpret_cast<sockaddr*>(&a), &length) == 0 ? ntohs(a.sin_port) : port;
        }
    }

    bool netStart()
    {
        Lock lock(g_lock);
        if (g_started++ > 0)
        {
            return true;
        }
#if defined(_XBOX)
        // Plain sockets, as everything else on the network has. A console that checks titles refuses it,
        // and then there is no network.
        XNetStartupParams params;
        memset(&params, 0, sizeof(params));
        params.cfgSizeOfStruct = sizeof(params);
        params.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
        if (XNetStartup(&params) != 0)
        {
            g_started--;
            return false;
        }
#endif
#if NX_WINSOCK
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            g_started--;
            return false;
        }
#endif
        return true;
    }

    void netStop()
    {
        Lock lock(g_lock);
        if (g_started == 0 || --g_started > 0)
        {
            return;
        }
#if NX_WINSOCK
        WSACleanup();
#endif
#if defined(_XBOX)
        XNetCleanup();
#endif
    }

    u32 netLocalAddress()
    {
#if defined(_XBOX)
        XNADDR a;
        memset(&a, 0, sizeof(a));
        const DWORD state = XNetGetTitleXnAddr(&a);
        return state == XNET_GET_XNADDR_PENDING || (state & XNET_GET_XNADDR_NONE) ? 0 : ntohl(a.ina.s_addr);
#elif defined(_WIN32)
        // Whichever the way out goes through. Nothing is sent.
        const SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET)
        {
            return 0;
        }
        sockaddr_in to = address({ 0x0A000001, 9 });
        sockaddr_in mine;
        int length = sizeof(mine);
        u32 ip = 0;
        if (::connect(s, reinterpret_cast<sockaddr*>(&to), sizeof(to)) == 0 && getsockname(s, reinterpret_cast<sockaddr*>(&mine), &length) == 0)
        {
            ip = ntohl(mine.sin_addr.s_addr);
        }
        closesocket(s);
        return ip;
#else
        u32 ip = 0;
        ifaddrs* list = nullptr;
        if (getifaddrs(&list) == 0)
        {
            for (const ifaddrs* i = list; i; i = i->ifa_next)
            {
                if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && (i->ifa_flags & IFF_UP) && !(i->ifa_flags & IFF_LOOPBACK) && ip == 0)
                {
                    ip = ntohl(reinterpret_cast<const sockaddr_in*>(i->ifa_addr)->sin_addr.s_addr);
                }
            }
            freeifaddrs(list);
        }
        return ip;
#endif
    }

    bool netResolve(const char* host, u32& ip)
    {
        if (netParseAddress(host, ip))
        {
            return true;
        }
#if defined(_XBOX)
        XNDNS* answer = nullptr;
        if (XNetDnsLookup(host, nullptr, &answer) != 0 || answer == nullptr)
        {
            return false;
        }
        for (u32 waited = 0; answer->iStatus == WSAEINPROGRESS && waited < 8000; waited += 20)
        {
            Sleep(20);
        }
        const bool ok = answer->iStatus == 0 && answer->cina > 0;
        if (ok)
        {
            ip = ntohl(answer->aina[0].s_addr);
        }
        XNetDnsRelease(answer);
        return ok;
#else
        addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* found = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &found) != 0 || found == nullptr)
        {
            return false;
        }
        ip = ntohl(reinterpret_cast<const sockaddr_in*>(found->ai_addr)->sin_addr.s_addr);
        freeaddrinfo(found);
        return true;
#endif
    }

    // --- Datagrams ---

    bool UdpSocket::open(u16 port, bool onlyHere)
    {
        close();
        m_socket = make(SOCK_DGRAM, IPPROTO_UDP);
        if (m_socket == kNone)
        {
            return false;
        }
        const int on = 1;
        setsockopt(handle(m_socket), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof(on));
#if defined(_WIN32) && !defined(_XBOX)
        // One machine that isn't listening mustn't make receiving from the others fail.
        DWORD off = 0, bytes = 0;
        WSAIoctl(handle(m_socket), _WSAIOW(IOC_VENDOR, 12), &off, sizeof(off), nullptr, 0, &bytes, nullptr, nullptr);
#endif
        m_port = bindTo(m_socket, port, onlyHere);
        if (m_port == 0)
        {
            close();
            return false;
        }
        return true;
    }

    void UdpSocket::close()
    {
        if (m_socket != kNone)
        {
            closeSocket(handle(m_socket));
            m_socket = kNone;
        }
        m_port = 0;
    }

    bool UdpSocket::send(const Endpoint& to, const void* data, size_t size)
    {
        if (m_socket == kNone)
        {
            return false;
        }
        const sockaddr_in a = address(to);
        return ::sendto(handle(m_socket), static_cast<const char*>(data), static_cast<int>(size), kSendFlags, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) == static_cast<int>(size);
    }

    size_t UdpSocket::receive(Endpoint& from, void* data, size_t size)
    {
        if (m_socket == kNone)
        {
            return 0;
        }
        sockaddr_in a;
        Length length = sizeof(a);
        const auto got = ::recvfrom(handle(m_socket), static_cast<char*>(data), static_cast<int>(size), 0, reinterpret_cast<sockaddr*>(&a), &length);
        if (got <= 0)
        {
            return 0;
        }
        from = endpoint(a);
        return static_cast<size_t>(got);
    }

    // --- Streams ---

    bool TcpSocket::connect(const Endpoint& to)
    {
        close();
        m_socket = make(SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == kNone)
        {
            return false;
        }
        const int on = 1;
        setsockopt(handle(m_socket), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
        const sockaddr_in a = address(to);
        if (::connect(handle(m_socket), reinterpret_cast<const sockaddr*>(&a), sizeof(a)) == 0)
        {
            m_state = TcpState::Open;
            return true;
        }
        if (!connecting())
        {
            close();
            return false;
        }
        m_state = TcpState::Connecting;
        return true;
    }

    bool TcpSocket::listen(u16 port, bool onlyHere)
    {
        close();
        m_socket = make(SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == kNone)
        {
            return false;
        }
#if !NX_WINSOCK
        const int on = 1;
        setsockopt(handle(m_socket), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
#endif
        m_port = bindTo(m_socket, port, onlyHere);
        if (m_port == 0 || ::listen(handle(m_socket), 8) != 0)
        {
            close();
            return false;
        }
        m_state = TcpState::Listening;
        return true;
    }

    bool TcpSocket::accept(TcpSocket& out, Endpoint& from)
    {
        if (m_state != TcpState::Listening)
        {
            return false;
        }
        sockaddr_in a;
        Length length = sizeof(a);
        const Handle32 s = ::accept(handle(m_socket), reinterpret_cast<sockaddr*>(&a), &length);
#if NX_WINSOCK
        if (s == INVALID_SOCKET)
#else
        if (s < 0)
#endif
        {
            return false;
        }
        if (!dontWait(s))
        {
            closeSocket(s);
            return false;
        }
        out.close();
        out.m_socket = static_cast<uintptr_t>(s);
        out.m_state = TcpState::Open;
        from = endpoint(a);
        return true;
    }

    void TcpSocket::close()
    {
        if (m_socket != kNone)
        {
            closeSocket(handle(m_socket));
            m_socket = kNone;
        }
        m_state = TcpState::Closed;
        m_port = 0;
    }

    TcpState TcpSocket::update()
    {
        if (m_state == TcpState::Connecting)
        {
            fd_set writable, failed;
            FD_ZERO(&writable);
            FD_ZERO(&failed);
            FD_SET(handle(m_socket), &writable);
            FD_SET(handle(m_socket), &failed);
            timeval none = { 0, 0 };
            const int ready = ::select(static_cast<int>(m_socket) + 1, nullptr, &writable, &failed, &none);
            if (ready < 0 || (ready > 0 && FD_ISSET(handle(m_socket), &failed)))
            {
                close();
            }
            else if (ready > 0 && FD_ISSET(handle(m_socket), &writable))
            {
                if (!connected(handle(m_socket)))
                {
                    close();
                }
                else
                {
                    m_state = TcpState::Open;
                }
            }
        }
        return m_state;
    }

    size_t TcpSocket::send(const void* data, size_t size)
    {
        if (m_state != TcpState::Open || size == 0)
        {
            return 0;
        }
        const auto sent = ::send(handle(m_socket), static_cast<const char*>(data), static_cast<int>(size), kSendFlags);
        if (sent < 0)
        {
            if (!wouldWait())
            {
                close();
            }
            return 0;
        }
        return static_cast<size_t>(sent);
    }

    size_t TcpSocket::receive(void* data, size_t size)
    {
        if (m_state != TcpState::Open)
        {
            return 0;
        }
        const auto got = ::recv(handle(m_socket), static_cast<char*>(data), static_cast<int>(size), 0);
        if (got > 0)
        {
            return static_cast<size_t>(got);
        }
        if (got == 0 || !wouldWait())
        {
            close();
        }
        return 0;
    }
}
