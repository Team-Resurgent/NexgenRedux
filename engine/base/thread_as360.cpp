// The calls that base/as360/windows.h gives AngelScript on the Xbox 360, made with the console's own.
#include <xtl.h>

static_assert(sizeof(CRITICAL_SECTION) <= 64, "base/as360/windows.h has too little room for one");

namespace
{
    // A value for each thread, kept here: as RXDK-360's runtime gives them, TlsGetValue and GetCurrentThreadId don't tell
    // one thread from another, and AngelScript keeps which script is running in one. A thread is known by its stack:
    // each that the engine starts (base/thread_windows.cpp) says where its stack is, and any other is the main thread.
    constexpr unsigned kKeys = 4;
    constexpr unsigned kThreads = 32;
    constexpr uintptr_t kStack = 256 * 1024;   // as startThread asks for
    struct Range
    {
        volatile uintptr_t low, high;   // high is 0 while the slot is free
    };
    Range g_ranges[kThreads];
    void* volatile g_values[kKeys][kThreads + 1];   // [0] is the main thread's
    bool g_used[kKeys] = {};
    CRITICAL_SECTION g_lock;
    volatile LONG g_lockMade = 0;

    void lockOnce()
    {
        // The first call is from the main thread, before any other thread is started.
        if (InterlockedCompareExchange(&g_lockMade, 1, 0) == 0)
        {
            InitializeCriticalSection(&g_lock);
        }
    }

    // Which thread is asking: 0 for the main thread, otherwise its slot and 1.
    unsigned threadIndex()
    {
        volatile char here = 0;
        const uintptr_t at = reinterpret_cast<uintptr_t>(&here);
        for (unsigned i = 0; i < kThreads; i++)
        {
            const uintptr_t high = g_ranges[i].high;
            if (high && at >= g_ranges[i].low && at < high)
            {
                return i + 1;
            }
        }
        return 0;
    }
}

extern "C"
{
    // At the start and end of each thread that base/thread_windows.cpp starts: top is near the top of its stack.
    void nx360ThreadBegan(void* top)
    {
        lockOnce();
        const uintptr_t at = reinterpret_cast<uintptr_t>(top);
        EnterCriticalSection(&g_lock);
        for (unsigned i = 0; i < kThreads; i++)
        {
            if (g_ranges[i].high == 0)
            {
                for (unsigned k = 0; k < kKeys; k++)
                {
                    g_values[k][i + 1] = nullptr;
                }
                g_ranges[i].low = at > kStack ? at - kStack : 0;
                MemoryBarrier();
                g_ranges[i].high = at + 4096;
                break;
            }
        }
        LeaveCriticalSection(&g_lock);
    }

    void nx360ThreadEnded(void)
    {
        const unsigned index = threadIndex();
        if (index)
        {
            EnterCriticalSection(&g_lock);
            g_ranges[index - 1].high = 0;
            LeaveCriticalSection(&g_lock);
        }
    }

    void nx360InitializeCriticalSection(void* section) { InitializeCriticalSection(static_cast<CRITICAL_SECTION*>(section)); }
    void nx360DeleteCriticalSection(void* section) { DeleteCriticalSection(static_cast<CRITICAL_SECTION*>(section)); }
    void nx360EnterCriticalSection(void* section) { EnterCriticalSection(static_cast<CRITICAL_SECTION*>(section)); }
    void nx360LeaveCriticalSection(void* section) { LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(section)); }
    int nx360TryEnterCriticalSection(void* section) { return TryEnterCriticalSection(static_cast<CRITICAL_SECTION*>(section)) ? 1 : 0; }

    unsigned long nx360TlsAlloc(void)
    {
        lockOnce();
        EnterCriticalSection(&g_lock);
        unsigned long key = 0xFFFFFFFFul;
        for (unsigned k = 0; k < kKeys && key == 0xFFFFFFFFul; k++)
        {
            if (!g_used[k])
            {
                g_used[k] = true;
                key = k;
                for (unsigned t = 0; t <= kThreads; t++)
                {
                    g_values[k][t] = nullptr;
                }
            }
        }
        LeaveCriticalSection(&g_lock);
        return key;
    }

    int nx360TlsFree(unsigned long key)
    {
        if (key >= kKeys)
        {
            return 0;
        }
        EnterCriticalSection(&g_lock);
        g_used[key] = false;
        LeaveCriticalSection(&g_lock);
        return 1;
    }

    void* nx360TlsGetValue(unsigned long key) { return key < kKeys ? g_values[key][threadIndex()] : nullptr; }

    int nx360TlsSetValue(unsigned long key, void* value)
    {
        if (key >= kKeys)
        {
            return 0;
        }
        g_values[key][threadIndex()] = value;   // only this thread writes its own
        return 1;
    }

    long nx360InterlockedIncrement(long volatile* value) { return InterlockedIncrement(value); }
    long nx360InterlockedDecrement(long volatile* value) { return InterlockedDecrement(value); }

    void* nx360CreateSemaphore(void*, long initial, long most, const void*) { return CreateSemaphoreA(nullptr, initial, most, nullptr); }
    int nx360ReleaseSemaphore(void* semaphore, long count, long* previous) { return ReleaseSemaphore(semaphore, count, previous) ? 1 : 0; }
    unsigned long nx360WaitForSingleObject(void* handle, unsigned long milliseconds) { return WaitForSingleObject(handle, milliseconds); }
    int nx360CloseHandle(void* handle) { return CloseHandle(handle) ? 1 : 0; }
}
