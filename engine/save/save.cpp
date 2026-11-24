#include "save/save.h"
#include "base/json.h"

#include <string.h>

namespace nx
{
    namespace
    {
        void append(std::string& out, const std::string& text)
        {
            static const char digits[] = "0123456789abcdef";
            out += '"';
            for (const char c : text)
            {
                const u8 byte = static_cast<u8>(c);
                if (c == '"' || c == '\\')
                {
                    out += '\\';
                    out += c;
                }
                else if (byte < 0x20)
                {
                    out += "\\u00";
                    out += digits[byte >> 4];
                    out += digits[byte & 15];
                }
                else
                {
                    out += c;
                }
            }
            out += '"';
        }
    }

    void Save::init(Platform& platform, const char* name)
    {
        m_platform = &platform;
        m_name = name;
        FileData file;
        if (platform.loadSave(name, file))
        {
            // Damaged or out of date: keep whatever can be read.
            char* value = static_cast<char*>(memAlloc(kMaxValue, MemTag::General));
            jsonEachString(reinterpret_cast<const char*>(file.data), value, kMaxValue, [](const char* key, const char* text, void* self) { static_cast<Save*>(self)->set(key, text); }, this);
            memFree(value);
        }
        m_changed = false;
    }

    const char* Save::get(const char* key, const char* otherwise) const
    {
        for (u32 i = 0; i < m_count; i++)
        {
            if (m_entries[i].key == key)
            {
                return m_entries[i].value.c_str();
            }
        }
        return otherwise;
    }

    bool Save::set(const char* key, const char* value)
    {
        if (strlen(key) >= 64 || strlen(value) >= kMaxValue)
        {
            return false;
        }
        u32 i = 0;
        while (i < m_count && m_entries[i].key != key)
        {
            i++;
        }
        if (i == kMaxEntries)
        {
            return false;
        }
        if (i == m_count)
        {
            m_entries[m_count++].key = key;
            m_entries[i].value.clear();
            m_changed = true;
        }
        if (m_entries[i].value != value)
        {
            m_entries[i].value = value;
            m_changed = true;
        }
        return true;
    }

    void Save::remove(const char* key)
    {
        for (u32 i = 0; i < m_count; i++)
        {
            if (m_entries[i].key == key)
            {
                m_entries[i] = m_entries[--m_count];
                m_entries[m_count] = {};
                m_changed = true;
                return;
            }
        }
    }

    bool Save::flush()
    {
        if (!m_changed || m_readOnly || m_platform == nullptr)
        {
            return true;
        }
        std::string text = "{";
        for (u32 i = 0; i < m_count; i++)
        {
            text += i ? ",\n" : "\n";
            append(text, m_entries[i].key);
            text += ": ";
            append(text, m_entries[i].value);
        }
        text += "\n}\n";
        m_changed = !m_platform->writeSave(m_name.c_str(), text.data(), text.size());
        return !m_changed;
    }
}
