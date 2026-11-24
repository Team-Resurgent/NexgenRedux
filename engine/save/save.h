#pragma once

#include "platform/platform.h"

#include <string>

namespace nx
{
    // Settings and scores: text by name, kept between runs. Written when something has changed, at
    // moments the app chooses.
    class Save : NonCopyable
    {
    public:
        static constexpr u32 kMaxEntries = 256;
        static constexpr size_t kMaxValue = 4096;

        void init(Platform& platform, const char* name);

        const char* get(const char* key, const char* otherwise) const;
        // False if there is no room or a value is too long.
        bool set(const char* key, const char* value);
        void remove(const char* key);

        // Nothing is written, as for unattended runs.
        void setReadOnly() { m_readOnly = true; }

        // Writes if anything has changed.
        bool flush();

    private:
        struct Entry
        {
            std::string key;
            std::string value;
        };

        Platform* m_platform = nullptr;
        std::string m_name;
        Entry m_entries[kMaxEntries];
        u32 m_count = 0;
        bool m_changed = false;
        bool m_readOnly = false;
    };
}
