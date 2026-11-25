#pragma once

#include "base/json_tree.h"

#include <string>
#include <vector>

namespace nx
{
    // JSON that can be changed and written out. Shared by count: what holds a part of one keeps that part.
    // Values are copied as they are put in, so nothing is ever inside itself.
    class JsonValue : NonCopyable
    {
    public:
        using Type = JsonTree::Type;
        static constexpr u32 kMaxDepth = 20;

        JsonValue() = default;
        ~JsonValue();

        void addRef() { m_refs++; }
        void release();

        // Replaces all of it. False, and empty, if it isn't JSON.
        bool parse(const char* text, size_t size, std::string& error);
        void read(JsonTree::Value from);
        // On several lines if it has things in it, unless it is to be on one.
        std::string write(bool oneLine) const;

        Type type() const { return m_type; }
        u32 count() const { return static_cast<u32>(m_items.size()); }
        const char* key(u32 index) const { return index < m_items.size() ? m_items[index].key.c_str() : ""; }
        // nullptr if it isn't there.
        JsonValue* find(const char* key) const;
        JsonValue* at(u32 index) const { return index < m_items.size() ? m_items[index].value : nullptr; }
        double number() const { return m_number; }
        const std::string& text() const { return m_text; }
        bool flag() const { return m_number != 0; }

        // What is there already becomes something else.
        void setNull() { clear(); }
        void setNumber(double number);
        void setText(const std::string& text);
        void setFlag(bool flag);
        void setArray();
        // False if it would be too deep.
        bool setCopy(const JsonValue& other);

        // A part by name, made if need be; what wasn't an object becomes one. nullptr if it would be too deep.
        JsonValue* put(const std::string& key);
        // Another at the end; what wasn't an array becomes one.
        JsonValue* add();
        void remove(const char* key);
        void removeAt(u32 index);

    private:
        struct Item
        {
            std::string key;
            JsonValue* value;
        };

        void clear();
        u32 height() const;
        void copy(const JsonValue& other);
        void writeTo(std::string& out, u32 indent, bool oneLine) const;
        bool holdsObjects() const;
        JsonValue* append(const std::string& key);

        int m_refs = 1;
        u32 m_depth = 0;
        Type m_type = Type::Null;
        double m_number = 0;
        std::string m_text;
        std::vector<Item> m_items;
    };
}
