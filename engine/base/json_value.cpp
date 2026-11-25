#include "base/json_value.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        void quote(std::string& out, const std::string& text)
        {
            out += '"';
            for (const char c : text)
            {
                switch (c)
                {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        char code[8];
                        snprintf(code, sizeof(code), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                        out += code;
                    }
                    else
                    {
                        out += c;
                    }
                }
            }
            out += '"';
        }

        void putNumber(std::string& out, double number)
        {
            char text[40];
            if (!isfinite(number))
            {
                number = 0;
            }
            if (number == floor(number) && fabs(number) < 1e15)
            {
                snprintf(text, sizeof(text), "%.0f", number == 0 ? 0.0 : number);
            }
            else
            {
                // As many figures as a float has, so that 0.1f is 0.1.
                snprintf(text, sizeof(text), "%.7g", number);
            }
            out += text;
        }
    }

    JsonValue::~JsonValue()
    {
        clear();
    }

    void JsonValue::release()
    {
        if (--m_refs == 0)
        {
            destroy(this);
        }
    }

    void JsonValue::clear()
    {
        // Out of the way first: nothing is told of a part being let go, but it costs nothing to be careful.
        std::vector<Item> items;
        items.swap(m_items);
        for (const Item& i : items)
        {
            i.value->release();
        }
        m_type = Type::Null;
        m_number = 0;
        m_text.clear();
    }

    bool JsonValue::parse(const char* text, size_t size, std::string& error)
    {
        clear();
        JsonTree tree;
        if (!tree.parse(text, size))
        {
            error = "line " + std::to_string(tree.errorLine()) + ": " + tree.error();
            return false;
        }
        error.clear();
        read(tree.root());
        return true;
    }

    void JsonValue::read(JsonTree::Value from)
    {
        clear();
        m_type = from.type();
        switch (m_type)
        {
        case Type::Bool: m_number = from.flag() ? 1 : 0; break;
        case Type::Number: m_number = from.exact(); break;
        case Type::String: m_text = from.text(); break;
        case Type::Array:
        case Type::Object:
            m_items.reserve(from.count());
            for (JsonTree::Value v = from.first(); v.exists(); v = v.next())
            {
                JsonValue* child = append(v.key());
                if (child == nullptr)
                {
                    break;
                }
                child->read(v);
            }
            break;
        case Type::Null: break;
        }
    }

    JsonValue* JsonValue::append(const std::string& key)
    {
        if (m_depth >= kMaxDepth)
        {
            return nullptr;
        }
        JsonValue* child = create<JsonValue>(MemTag::Script);
        if (child)
        {
            child->m_depth = m_depth + 1;
            m_items.push_back({ key, child });
        }
        return child;
    }

    JsonValue* JsonValue::find(const char* key) const
    {
        if (m_type == Type::Object)
        {
            for (const Item& i : m_items)
            {
                if (i.key == key)
                {
                    return i.value;
                }
            }
        }
        return nullptr;
    }

    void JsonValue::setNumber(double number)
    {
        clear();
        m_type = Type::Number;
        m_number = number;
    }

    void JsonValue::setText(const std::string& text)
    {
        // A copy, as it may be this one's own.
        std::string copy = text;
        clear();
        m_type = Type::String;
        m_text.swap(copy);
    }

    void JsonValue::setFlag(bool flag)
    {
        clear();
        m_type = Type::Bool;
        m_number = flag ? 1 : 0;
    }

    void JsonValue::setArray()
    {
        clear();
        m_type = Type::Array;
    }

    u32 JsonValue::height() const
    {
        u32 most = 0;
        for (const Item& i : m_items)
        {
            const u32 h = i.value->height() + 1;
            most = h > most ? h : most;
        }
        return most;
    }

    bool JsonValue::setCopy(const JsonValue& other)
    {
        if (&other == this)
        {
            return true;
        }
        if (m_depth + other.height() > kMaxDepth)
        {
            return false;
        }
        // Made apart first, as one may be a part of the other.
        JsonValue* made = create<JsonValue>(MemTag::Script);
        made->m_depth = m_depth;
        made->copy(other);
        clear();
        m_type = made->m_type;
        m_number = made->m_number;
        m_text.swap(made->m_text);
        m_items.swap(made->m_items);
        made->release();
        return true;
    }

    void JsonValue::copy(const JsonValue& other)
    {
        m_type = other.m_type;
        m_number = other.m_number;
        m_text = other.m_text;
        m_items.reserve(other.m_items.size());
        for (const Item& i : other.m_items)
        {
            if (JsonValue* child = append(i.key))
            {
                child->copy(*i.value);
            }
        }
    }

    JsonValue* JsonValue::put(const std::string& key)
    {
        if (m_type != Type::Object)
        {
            clear();
            m_type = Type::Object;
        }
        JsonValue* there = find(key.c_str());
        return there ? there : append(key);
    }

    JsonValue* JsonValue::add()
    {
        if (m_type != Type::Array)
        {
            setArray();
        }
        return append("");
    }

    void JsonValue::remove(const char* key)
    {
        for (u32 i = 0; m_type == Type::Object && i < m_items.size(); i++)
        {
            if (m_items[i].key == key)
            {
                removeAt(i);
                return;
            }
        }
    }

    void JsonValue::removeAt(u32 index)
    {
        if (index < m_items.size())
        {
            JsonValue* gone = m_items[index].value;
            m_items.erase(m_items.begin() + index);
            gone->release();
        }
    }

    bool JsonValue::holdsObjects() const
    {
        for (const Item& i : m_items)
        {
            if (i.value->m_type == Type::Object || i.value->holdsObjects())
            {
                return true;
            }
        }
        return false;
    }

    std::string JsonValue::write(bool oneLine) const
    {
        std::string out;
        writeTo(out, 0, oneLine);
        if (!oneLine)
        {
            out += '\n';
        }
        return out;
    }

    // Each thing on a line, and lists of things with a line for each, as people write them.
    void JsonValue::writeTo(std::string& out, u32 indent, bool oneLine) const
    {
        switch (m_type)
        {
        case Type::Null: out += "null"; return;
        case Type::Bool: out += m_number != 0 ? "true" : "false"; return;
        case Type::Number: putNumber(out, m_number); return;
        case Type::String: quote(out, m_text); return;
        default: break;
        }
        const bool object = m_type == Type::Object;
        const bool lines = !oneLine && !m_items.empty() && holdsObjects();
        out += object ? '{' : '[';
        for (size_t i = 0; i < m_items.size(); i++)
        {
            if (i > 0)
            {
                out += lines ? "," : ", ";
            }
            if (lines)
            {
                out += '\n';
                out.append((indent + 1) * 2, ' ');
            }
            if (object)
            {
                quote(out, m_items[i].key);
                out += ": ";
            }
            m_items[i].value->writeTo(out, indent + 1, oneLine);
        }
        if (lines)
        {
            out += '\n';
            out.append(indent * 2, ' ');
        }
        out += object ? '}' : ']';
    }
}
