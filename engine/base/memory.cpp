#include "base/memory.h"
#include "base/log.h"

#include <atomic>
#include <stdlib.h>

namespace nx
{
    namespace
    {
        constexpr u32 kLive = 0x4E584D31;  // "NXM1"
        constexpr u32 kFreed = 0xDEADF00D;

        struct Header
        {
            void* raw;
            size_t size;
            u32 magic;
            MemTag tag;
        };

        struct TagState
        {
            std::atomic<size_t> bytes{ 0 };
            std::atomic<size_t> peak{ 0 };
            std::atomic<size_t> count{ 0 };
            std::atomic<size_t> totalCount{ 0 };
            std::atomic<size_t> budget{ 0 };
        };

        constexpr size_t kTagCount = static_cast<size_t>(MemTag::Count);
        TagState g_tags[kTagCount];

        TagState& state(MemTag tag)
        {
            NX_ASSERT(static_cast<size_t>(tag) < kTagCount);
            return g_tags[static_cast<size_t>(tag)];
        }

        void raisePeak(TagState& s, size_t bytes)
        {
            size_t peak = s.peak.load(std::memory_order_relaxed);
            while (bytes > peak && !s.peak.compare_exchange_weak(peak, bytes, std::memory_order_relaxed))
            {
            }
        }
    }

    const char* memTagName(MemTag tag)
    {
        static const char* const names[kTagCount] = { "general", "frame", "scene", "texture", "buffer", "audio", "script", "ui" };
        return names[static_cast<size_t>(tag)];
    }

    MemStats memStats(MemTag tag)
    {
        const TagState& s = state(tag);
        return { s.bytes.load(), s.peak.load(), s.count.load(), s.totalCount.load() };
    }

    MemStats memStatsTotal()
    {
        MemStats total = {};
        for (size_t i = 0; i < kTagCount; i++)
        {
            const MemStats s = memStats(static_cast<MemTag>(i));
            total.bytes += s.bytes;
            total.peak += s.peak;
            total.count += s.count;
            total.totalCount += s.totalCount;
        }
        return total;
    }

    void memSetBudget(MemTag tag, size_t bytes)
    {
        state(tag).budget.store(bytes);
    }

    void* memAlloc(size_t size, MemTag tag, size_t align)
    {
        NX_ASSERT(align >= alignof(Header) && (align & (align - 1)) == 0);
        if (size > SIZE_MAX - align - sizeof(Header))
        {
            return nullptr;
        }

        TagState& s = state(tag);
        const size_t bytes = s.bytes.fetch_add(size) + size;
        const size_t budget = s.budget.load(std::memory_order_relaxed);
        void* raw = (budget != 0 && bytes > budget) ? nullptr : malloc(size + align + sizeof(Header));
        if (raw == nullptr)
        {
            s.bytes.fetch_sub(size);
            return nullptr;
        }

        uintptr_t address = reinterpret_cast<uintptr_t>(raw) + sizeof(Header);
        address = (address + align - 1) & ~static_cast<uintptr_t>(align - 1);

        Header* header = reinterpret_cast<Header*>(address) - 1;
        header->raw = raw;
        header->size = size;
        header->magic = kLive;
        header->tag = tag;

        raisePeak(s, bytes);
        s.count.fetch_add(1, std::memory_order_relaxed);
        s.totalCount.fetch_add(1, std::memory_order_relaxed);
        return reinterpret_cast<void*>(address);
    }

    void memFree(void* ptr)
    {
        if (ptr == nullptr)
        {
            return;
        }
        Header* header = static_cast<Header*>(ptr) - 1;
        NX_ASSERT(header->magic == kLive);  // freed twice, or not from memAlloc
        header->magic = kFreed;

        TagState& s = state(header->tag);
        s.bytes.fetch_sub(header->size);
        s.count.fetch_sub(1, std::memory_order_relaxed);
        free(header->raw);
    }

    Arena::~Arena()
    {
        memFree(m_memory);
    }

    bool Arena::init(size_t capacity, MemTag tag)
    {
        NX_ASSERT(m_memory == nullptr);
        m_memory = static_cast<u8*>(memAlloc(capacity, tag));
        m_capacity = m_memory ? capacity : 0;
        return m_memory != nullptr;
    }

    void* Arena::alloc(size_t size, size_t align)
    {
        NX_ASSERT(align != 0 && (align & (align - 1)) == 0 && align <= 16);
        const size_t start = (m_used + align - 1) & ~(align - 1);
        if (start > m_capacity || size > m_capacity - start)
        {
            return nullptr;
        }
        m_used = start + size;
        m_peak = m_used > m_peak ? m_used : m_peak;
        return m_memory + start;
    }
}
