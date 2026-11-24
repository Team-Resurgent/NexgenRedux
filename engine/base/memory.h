#pragma once

#include "base/types.h"

#include <new>
#include <utility>

namespace nx
{
    // Who an allocation belongs to. Every engine allocation carries one, so each system can be
    // given a budget and reported on.
    enum class MemTag : u8
    {
        General,
        Frame,
        Scene,
        Texture,
        Buffer,
        Audio,
        Script,
        UI,
        Count
    };

    struct MemStats
    {
        size_t bytes;       // in use now
        size_t peak;        // highest so far
        size_t count;       // allocations in use now
        size_t totalCount;  // allocations ever made
    };

    const char* memTagName(MemTag tag);
    MemStats memStats(MemTag tag);
    MemStats memStatsTotal();

    // 0 means no limit. An allocation that would go over its tag's budget returns nullptr.
    void memSetBudget(MemTag tag, size_t bytes);

    // Returns nullptr when out of memory or over budget.
    void* memAlloc(size_t size, MemTag tag, size_t align = 16);
    void memFree(void* ptr);

    template <typename T, typename... Args>
    T* create(MemTag tag, Args&&... args)
    {
        void* memory = memAlloc(sizeof(T), tag, alignof(T) > 16 ? alignof(T) : 16);
        return memory ? new (memory) T(std::forward<Args>(args)...) : nullptr;
    }

    template <typename T>
    void destroy(T* object)
    {
        if (object)
        {
            object->~T();
            memFree(object);
        }
    }

    // Linear memory that is handed out in order and reclaimed all at once.
    class Arena : NonCopyable
    {
    public:
        Arena() = default;
        ~Arena();

        bool init(size_t capacity, MemTag tag);

        // nullptr when full: callers treat that as "over budget", never as a reason to use the heap.
        void* alloc(size_t size, size_t align = 16);
        void reset() { m_used = 0; }

        size_t used() const { return m_used; }
        size_t peak() const { return m_peak; }
        size_t capacity() const { return m_capacity; }

    private:
        u8* m_memory = nullptr;
        size_t m_capacity = 0;
        size_t m_used = 0;
        size_t m_peak = 0;
    };
}
