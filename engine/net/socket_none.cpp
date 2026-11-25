#include "net/socket.h"

// For a platform whose network isn't written yet: there is nobody there.
namespace nx
{
    bool netStart() { return false; }
    void netStop() {}
    u32 netLocalAddress() { return 0; }
    bool netResolve(const char*, u32&) { return false; }

    bool UdpSocket::open(u16, bool) { return false; }
    void UdpSocket::close() { m_socket = kNone; }
    bool UdpSocket::send(const Endpoint&, const void*, size_t) { return false; }
    size_t UdpSocket::receive(Endpoint&, void*, size_t) { return 0; }

    bool TcpSocket::connect(const Endpoint&) { return false; }
    bool TcpSocket::listen(u16, bool) { return false; }
    bool TcpSocket::accept(TcpSocket&, Endpoint&) { return false; }
    void TcpSocket::close()
    {
        m_socket = kNone;
        m_state = TcpState::Closed;
    }
    TcpState TcpSocket::update() { return m_state; }
    size_t TcpSocket::send(const void*, size_t) { return 0; }
    size_t TcpSocket::receive(void*, size_t) { return 0; }
}
