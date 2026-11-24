#pragma once

#include "base/log.h"
#include "base/memory.h"
#include "base/types.h"

namespace nx
{
    // A reference that can tell when its object is gone: slot index plus the slot's generation.
    // The Tag type only stops handles of different kinds being mixed up.
    template <typename Tag>
    struct Handle
    {
        u32 value = 0;

        explicit operator bool() const { return value != 0; }
        bool operator==(const Handle& other) const { return value == other.value; }
        bool operator!=(const Handle& other) const { return value != other.value; }
    };

    // Fixed-capacity pool addressed by handle. Adding and removing never allocates.
    template <typename T, typename Tag>
    class HandlePool : NonCopyable
    {
    public:
        static constexpr u32 kIndexBits = 18;
        static constexpr u32 kMaxCapacity = 1u << kIndexBits;

        ~HandlePool() { shutdown(); }

        bool init(u32 capacity, MemTag tag)
        {
            NX_ASSERT(m_slots == nullptr && capacity > 0 && capacity <= kMaxCapacity);
            m_slots = static_cast<Slot*>(memAlloc(sizeof(Slot) * capacity, tag));
            if (m_slots == nullptr)
            {
                return false;
            }
            m_capacity = capacity;
            for (u32 i = 0; i < capacity; i++)
            {
                m_slots[i].generation = 1;
                m_slots[i].live = false;
                m_slots[i].next = i + 1;
            }
            m_free = 0;
            return true;
        }

        void shutdown()
        {
            for (u32 i = 0; i < m_capacity; i++)
            {
                if (m_slots[i].live)
                {
                    m_slots[i].object()->~T();
                }
            }
            memFree(m_slots);
            m_slots = nullptr;
            m_capacity = m_count = 0;
        }

        // A null handle when the pool is full.
        Handle<Tag> add(T value)
        {
            if (m_count == m_capacity)
            {
                return {};
            }
            const u32 index = m_free;
            Slot& slot = m_slots[index];
            m_free = slot.next;
            slot.live = true;
            new (slot.storage) T(std::move(value));
            m_count++;
            return { (slot.generation << kIndexBits) | index };
        }

        T* get(Handle<Tag> handle)
        {
            const u32 index = handle.value & (kMaxCapacity - 1);
            if (index >= m_capacity)
            {
                return nullptr;
            }
            Slot& slot = m_slots[index];
            return slot.live && slot.generation == (handle.value >> kIndexBits) ? slot.object() : nullptr;
        }

        bool remove(Handle<Tag> handle)
        {
            T* object = get(handle);
            if (object == nullptr)
            {
                return false;
            }
            object->~T();
            const u32 index = handle.value & (kMaxCapacity - 1);
            Slot& slot = m_slots[index];
            slot.live = false;
            slot.generation = slot.generation == kMaxGeneration ? 1 : slot.generation + 1;  // 0 is never valid
            slot.next = m_free;
            m_free = index;
            m_count--;
            return true;
        }

        template <typename Fn>
        void forEach(Fn fn)
        {
            for (u32 i = 0; i < m_capacity; i++)
            {
                if (m_slots[i].live)
                {
                    fn(*m_slots[i].object());
                }
            }
        }

        u32 count() const { return m_count; }
        u32 capacity() const { return m_capacity; }

    private:
        static constexpr u32 kMaxGeneration = (1u << (32 - kIndexBits)) - 1;

        struct Slot
        {
            alignas(T) u8 storage[sizeof(T)];
            u32 generation;
            u32 next;
            bool live;

            T* object() { return std::launder(reinterpret_cast<T*>(storage)); }
        };

        static_assert(alignof(T) <= 16);

        Slot* m_slots = nullptr;
        u32 m_capacity = 0;
        u32 m_count = 0;
        u32 m_free = 0;
    };
}
