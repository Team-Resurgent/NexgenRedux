#include "base/json_tree.h"

#include <stdlib.h>
#include <string.h>

namespace nx
{
    namespace
    {
        char* putUtf8(char* out, u32 code)
        {
            if (code < 0x80)
            {
                *out++ = static_cast<char>(code);
            }
            else if (code < 0x800)
            {
                *out++ = static_cast<char>(0xC0 | (code >> 6));
                *out++ = static_cast<char>(0x80 | (code & 0x3F));
            }
            else
            {
                *out++ = static_cast<char>(0xE0 | (code >> 12));
                *out++ = static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                *out++ = static_cast<char>(0x80 | (code & 0x3F));
            }
            return out;
        }
    }

    JsonTree::Value JsonTree::Value::operator[](const char* key) const
    {
        if (isObject())
        {
            for (Value v = first(); v.exists(); v = v.next())
            {
                if (strcmp(v.key(), key) == 0)
                {
                    return v;
                }
            }
        }
        return {};
    }

    JsonTree::Value JsonTree::Value::operator[](u32 index) const
    {
        if (isArray())
        {
            for (Value v = first(); v.exists(); v = v.next())
            {
                if (index-- == 0)
                {
                    return v;
                }
            }
        }
        return {};
    }

    bool JsonTree::parse(const char* text, size_t size)
    {
        memFree(m_text);
        m_nodes.clear();
        m_line = 1;
        m_error = "";
        // Offset 0 is an empty string, so a node without a key or text has one.
        m_text = static_cast<char*>(memAlloc(size + 2, MemTag::General));
        if (m_text == nullptr)
        {
            return fail("no memory");
        }
        m_text[0] = 0;
        memcpy(m_text + 1, text, size);
        m_text[size + 1] = 0;
        m_at = m_text + 1;
        u32 index = 0;
        space();
        if (!value(0, index))
        {
            return false;
        }
        space();
        return *m_at == 0 || fail("something after the end");
    }

    bool JsonTree::fail(const char* what)
    {
        m_error = m_error[0] ? m_error : what;
        return false;
    }

    void JsonTree::space()
    {
        for (;;)
        {
            if (*m_at == '\n')
            {
                m_line++;
                m_at++;
            }
            else if (*m_at == ' ' || *m_at == '\t' || *m_at == '\r')
            {
                m_at++;
            }
            else if (m_at[0] == '/' && m_at[1] == '/')
            {
                while (*m_at && *m_at != '\n')
                {
                    m_at++;
                }
            }
            else
            {
                return;
            }
        }
    }

    // Decoded where it lies: it never gets longer.
    bool JsonTree::string(u32& offset)
    {
        char* out = ++m_at;
        offset = static_cast<u32>(out - m_text);
        while (*m_at != '"')
        {
            if (*m_at == 0 || *m_at == '\n')
            {
                return fail("a string isn't closed");
            }
            if (*m_at != '\\')
            {
                *out++ = *m_at++;
                continue;
            }
            m_at++;
            switch (*m_at)
            {
            case 'n': *out++ = '\n'; break;
            case 't': *out++ = '\t'; break;
            case 'r': *out++ = '\r'; break;
            case 'b': *out++ = '\b'; break;
            case 'f': *out++ = '\f'; break;
            case '"':
            case '\\':
            case '/': *out++ = *m_at; break;
            case 'u':
            {
                u32 code = 0;
                for (u32 i = 1; i <= 4; i++)
                {
                    const char c = m_at[i];
                    const u32 digit = c >= '0' && c <= '9' ? static_cast<u32>(c - '0') : c >= 'a' && c <= 'f' ? static_cast<u32>(c - 'a' + 10) : c >= 'A' && c <= 'F' ? static_cast<u32>(c - 'A' + 10) : 16;
                    if (digit == 16)
                    {
                        return fail("a bad \\u in a string");
                    }
                    code = code * 16 + digit;
                }
                m_at += 4;
                out = putUtf8(out, code ? code : '?');
                break;
            }
            default: return fail("a bad \\ in a string");
            }
            m_at++;
        }
        m_at++;
        *out = 0;
        return true;
    }

    bool JsonTree::value(u32 depth, u32& index)
    {
        if (depth > kMaxDepth)
        {
            return fail("nested too deeply");
        }
        index = m_nodes.count();
        Node node;
        node.line = m_line;
        if (!m_nodes.push(node))
        {
            return fail("no memory");
        }
        const char c = *m_at;
        if (c == '{' || c == '[')
        {
            const char close = c == '{' ? '}' : ']';
            m_nodes[index].type = c == '{' ? Type::Object : Type::Array;
            m_at++;
            space();
            u32 last = 0;
            while (*m_at != close)
            {
                u32 key = 0;
                if (close == '}')
                {
                    if (*m_at != '"')
                    {
                        return fail("expected a name in quotes");
                    }
                    if (!string(key))
                    {
                        return false;
                    }
                    space();
                    if (*m_at != ':')
                    {
                        return fail("expected : after a name");
                    }
                    m_at++;
                    space();
                }
                u32 child = 0;
                if (!value(depth + 1, child))
                {
                    return false;
                }
                m_nodes[child].key = key;
                if (last)
                {
                    m_nodes[last].next = child;
                }
                else
                {
                    m_nodes[index].first = child;
                }
                last = child;
                m_nodes[index].count++;
                space();
                if (*m_at == ',')
                {
                    m_at++;
                    space();
                }
                else if (*m_at != close)
                {
                    return fail(close == '}' ? "expected , or }" : "expected , or ]");
                }
            }
            m_at++;
            return true;
        }
        if (c == '"')
        {
            m_nodes[index].type = Type::String;
            u32 text = 0;
            const bool ok = string(text);
            m_nodes[index].text = text;
            return ok;
        }
        if (strncmp(m_at, "true", 4) == 0 || strncmp(m_at, "false", 5) == 0)
        {
            m_nodes[index].type = Type::Bool;
            m_nodes[index].flag = c == 't';
            m_at += c == 't' ? 4 : 5;
            return true;
        }
        if (strncmp(m_at, "null", 4) == 0)
        {
            m_at += 4;
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            char* end = nullptr;
            m_nodes[index].type = Type::Number;
            m_nodes[index].number = strtod(m_at, &end);
            if (end == m_at)
            {
                return fail("a bad number");
            }
            m_at = end;
            return true;
        }
        return fail(c ? "expected a value" : "the file ends too soon");
    }
}
