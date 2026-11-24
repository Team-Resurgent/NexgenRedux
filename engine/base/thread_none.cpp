#include "base/thread.h"

// For a platform whose threads aren't written yet: there is only the one, and it never waits.
namespace nx
{
    Mutex::Mutex() : m_storage() {}
    Mutex::~Mutex() = default;
    void Mutex::lock() { (void)m_storage; }
    void Mutex::unlock() {}

    Semaphore::Semaphore() : m_storage() {}
    Semaphore::~Semaphore() = default;
    void Semaphore::post() { (void)m_storage; }
    void Semaphore::wait() {}

    ThreadHandle startThread(ThreadFunction, void*, ThreadPriority)
    {
        return {};
    }

    void joinThread(ThreadHandle) {}
    void sleepMilliseconds(u32) {}
}
