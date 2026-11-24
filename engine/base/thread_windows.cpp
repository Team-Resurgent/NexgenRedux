#include "base/log.h"
#include "base/memory.h"
#include "base/thread.h"

// Windows, Xbox One / Series and the original Xbox, whose SDK has the same calls.
#if defined(_XBOX)
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace nx
{
    namespace
    {
        struct Start
        {
            ThreadFunction function;
            void* user;
            HANDLE handle;
        };

        DWORD WINAPI threadMain(LPVOID data)
        {
            Start* start = static_cast<Start*>(data);
            start->function(start->user);
            return 0;
        }
    }

    Mutex::Mutex()
    {
        static_assert(sizeof(CRITICAL_SECTION) <= sizeof(m_storage));
        InitializeCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_storage));
    }

    Mutex::~Mutex()
    {
        DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_storage));
    }

    void Mutex::lock()
    {
        EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_storage));
    }

    void Mutex::unlock()
    {
        LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(m_storage));
    }

    Semaphore::Semaphore()
    {
        *reinterpret_cast<HANDLE*>(m_storage) = CreateSemaphoreA(nullptr, 0, 0x7FFFFFFF, nullptr);
    }

    Semaphore::~Semaphore()
    {
        CloseHandle(*reinterpret_cast<HANDLE*>(m_storage));
    }

    void Semaphore::post()
    {
        ReleaseSemaphore(*reinterpret_cast<HANDLE*>(m_storage), 1, nullptr);
    }

    void Semaphore::wait()
    {
        WaitForSingleObject(*reinterpret_cast<HANDLE*>(m_storage), INFINITE);
    }

    ThreadHandle startThread(ThreadFunction function, void* user, ThreadPriority priority)
    {
        Start* start = create<Start>(MemTag::General);
        if (start == nullptr)
        {
            return {};
        }
        start->function = function;
        start->user = user;
        // Image and sound decoders use a fair amount of stack.
        start->handle = CreateThread(nullptr, 256 * 1024, threadMain, start, 0, nullptr);
        if (start->handle == nullptr)
        {
            NX_ERROR("cannot start a thread");
            destroy(start);
            return {};
        }
        if (priority == ThreadPriority::Audio)
        {
            SetThreadPriority(start->handle, THREAD_PRIORITY_HIGHEST);
        }
        return { start };
    }

    void joinThread(ThreadHandle thread)
    {
        Start* start = static_cast<Start*>(thread.value);
        WaitForSingleObject(start->handle, INFINITE);
        CloseHandle(start->handle);
        destroy(start);
    }

    void sleepMilliseconds(u32 milliseconds)
    {
        Sleep(milliseconds);
    }
}
