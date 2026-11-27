#pragma once

#include "base/memory.h"
#include "base/thread.h"
#include "base/types.h"

#include <atomic>
#include <string>

namespace nx
{
    // A WebSocket: messages both ways with a web server, over ws:// or wss:// (TLS, its certificate checked), kept
    // open. It connects, reads and writes on a thread of its own; what comes waits until it is asked for.
    class WebSocket : NonCopyable
    {
    public:
        enum class State : int { Connecting, Open, Closed };

        explicit WebSocket(const char* url);
        ~WebSocket();

        void addRef() { m_refs++; }
        void release()
        {
            if (--m_refs == 0)
            {
                destroy(this);
            }
        }

        State state() const { return static_cast<State>(m_shared->state.load(std::memory_order_acquire)); }
        // Why it closed, if it wasn't asked to.
        std::string error() const;
        // Waits to be sent, in order. False once it has closed.
        bool send(const std::string& text);
        // The next message that has come, oldest first. False if there is none.
        bool receive(std::string& out);
        void close();

        // What the thread and the game share: freed by whichever lets go last.
        struct Shared
        {
            std::string url;
            std::atomic<int> state{ 0 };
            std::atomic<bool> closing{ false };
            Mutex lock;
            std::string error;
            std::string outgoing;       // frames, ready to go
            std::string incoming;       // messages, each after its length
            size_t read = 0;            // how far through incoming the game has read
        };

    private:
        static void run(void* shared);

        Shared* m_shared = nullptr;
        ThreadHandle m_thread{};
        bool m_net = false;   // the network started for it
        int m_refs = 1;
    };
}
