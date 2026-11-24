#pragma once

#include "base/types.h"

// Implemented once per platform family: thread_windows.cpp, thread_posix.cpp or thread_none.cpp.
namespace nx
{
    class Mutex : NonCopyable
    {
    public:
        Mutex();
        ~Mutex();
        void lock();
        void unlock();

    private:
        [[maybe_unused]] alignas(16) u8 m_storage[80];
    };

    class Lock : NonCopyable
    {
    public:
        explicit Lock(Mutex& mutex) : m_mutex(mutex) { m_mutex.lock(); }
        ~Lock() { m_mutex.unlock(); }

    private:
        Mutex& m_mutex;
    };

    class Semaphore : NonCopyable
    {
    public:
        Semaphore();
        ~Semaphore();
        void post();
        void wait();

    private:
        [[maybe_unused]] alignas(16) u8 m_storage[160];
    };

    using ThreadFunction = void (*)(void* user);

    struct ThreadHandle
    {
        void* value = nullptr;
        explicit operator bool() const { return value != nullptr; }
    };

    enum class ThreadPriority : u8 { Normal, Audio };

    // Null if the platform has no threads (yet): the caller then does the work itself.
    ThreadHandle startThread(ThreadFunction function, void* user, ThreadPriority priority);
    void joinThread(ThreadHandle thread);
    void sleepMilliseconds(u32 milliseconds);

    // One worker that runs jobs in order, away from the frame. Jobs are plain functions: the caller
    // keeps the data alive until the job says it is done.
    class JobQueue : NonCopyable
    {
    public:
        ~JobQueue() { stop(); }

        bool start();
        // Finishes what is queued, then stops.
        void stop();

        // False when the queue is full.
        bool add(ThreadFunction function, void* user);

    private:
        static constexpr u32 kCapacity = 256;
        static void run(void* self);

        struct Job
        {
            ThreadFunction function;
            void* user;
        };

        Job m_jobs[kCapacity];
        u32 m_first = 0;
        u32 m_count = 0;
        bool m_stopping = false;
        bool m_started = false;
        Mutex m_mutex;
        Semaphore m_ready;
        ThreadHandle m_thread;
    };
}
