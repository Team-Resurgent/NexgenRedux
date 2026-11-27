#pragma once

#include "net/socket.h"

#include <atomic>

namespace nx
{
    class Platform;

    // Where the certificates that are trusted are read from, when they are first needed.
    void secureSetPlatform(Platform* platform);
    // Frees them. Nothing secure may be going on.
    void secureShutdown();

    // A stream over TLS. Waits, so not on the game's thread.
    class SecureStream : NonCopyable
    {
    public:
        ~SecureStream() { close(); }

        // Greets the other end and checks that it is who it says. On failure error() says why.
        bool start(TcpSocket& socket, const char* host, const std::atomic<bool>& cancelled, double patience);
        void close();
        bool send(const void* data, size_t size);
        // 0 at the end, or on failure.
        size_t receive(void* data, size_t size);
        // What has come already, without waiting: 0 if nothing has, and then ended says whether that is because it is over.
        size_t receiveNow(void* data, size_t size, bool& ended);
        const char* error() const { return m_error; }

    private:
        struct Parts;

        static int sendSome(void* self, const unsigned char* data, size_t size);
        static int receiveSome(void* self, unsigned char* data, size_t size);
        bool waited();

        Parts* m_parts = nullptr;
        TcpSocket* m_socket = nullptr;
        const std::atomic<bool>* m_cancelled = nullptr;
        double m_patience = 0;   // seconds left, while waiting for anything
        double m_waited = 0;
        const char* m_error = "";
    };
}
