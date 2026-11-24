#pragma once

#include "base/memory.h"
#include "base/types.h"

#include <string.h>
#include <type_traits>

namespace nx
{
    // Plain data, packed. Grows in steps and never shrinks, so once it has been as large as it needs
    // to be nothing is allocated. Adding may move everything: keep indices, not pointers.
    template <typename T, MemTag Tag = MemTag::Scene>
    class GrowArray : NonCopyable
    {
        static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>);

    public:
        ~GrowArray() { free(); }

        bool reserve(u32 capacity)
        {
            if (capacity <= m_capacity)
            {
                return true;
            }
            T* data = static_cast<T*>(memAlloc(sizeof(T) * capacity, Tag));
            if (data == nullptr)
            {
                return false;
            }
            if (m_count)
            {
                memcpy(static_cast<void*>(data), m_data, sizeof(T) * m_count);
            }
            memFree(m_data);
            m_data = data;
            m_capacity = capacity;
            return true;
        }

        // False if there is no memory.
        bool push(const T& value)
        {
            if (m_count == m_capacity)
            {
                const T copy = value;  // it may be inside what is about to move
                if (!reserve(m_capacity ? m_capacity + (m_capacity < 1024 ? m_capacity : m_capacity / 2) : 32))
                {
                    return false;
                }
                m_data[m_count++] = copy;
                return true;
            }
            m_data[m_count++] = value;
            return true;
        }

        // The last one takes its place.
        void swapRemove(u32 index) { m_data[index] = m_data[--m_count]; }
        void clear() { m_count = 0; }
        void resize(u32 count) { m_count = count; }  // within capacity

        void free()
        {
            memFree(m_data);
            m_data = nullptr;
            m_count = m_capacity = 0;
        }

        void swap(GrowArray& other)
        {
            T* data = m_data;
            const u32 count = m_count, capacity = m_capacity;
            m_data = other.m_data;
            m_count = other.m_count;
            m_capacity = other.m_capacity;
            other.m_data = data;
            other.m_count = count;
            other.m_capacity = capacity;
        }

        T& operator[](u32 index) { return m_data[index]; }
        const T& operator[](u32 index) const { return m_data[index]; }
        T* begin() { return m_data; }
        T* end() { return m_data + m_count; }
        const T* begin() const { return m_data; }
        const T* end() const { return m_data + m_count; }
        u32 count() const { return m_count; }

    private:
        T* m_data = nullptr;
        u32 m_count = 0;
        u32 m_capacity = 0;
    };
}
