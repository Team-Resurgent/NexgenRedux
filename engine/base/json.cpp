#include "base/json.h"
#include "base/types.h"

#include <stdlib.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr int kMaxDepth = 32;

        const char* space(const char* p)
        {
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            {
                p++;
            }
            return p;
        }

        bool hex(const char* p, u32& out)
        {
            out = 0;
            for (int i = 0; i < 4; i++)
            {
                const char c = p[i];
                const u32 digit = c >= '0' && c <= '9' ? static_cast<u32>(c - '0') : c >= 'a' && c <= 'f' ? static_cast<u32>(c - 'a' + 10) : c >= 'A' && c <= 'F' ? static_cast<u32>(c - 'A' + 10) : 16;
                if (digit == 16)
                {
                    return false;
                }
                out = out * 16 + digit;
            }
            return true;
        }

        // p is at the opening quote. Copies what fits (out may be null). nullptr if malformed.
        const char* readString(const char* p, char* out, size_t size)
        {
            size_t length = 0;
            const auto put = [&](u32 byte)
            {
                if (out && length + 1 < size)
                {
                    out[length++] = static_cast<char>(byte);
                }
            };
            for (p++; *p != '"'; p++)
            {
                if (static_cast<u8>(*p) < 0x20)
                {
                    return nullptr;  // includes the end of the text
                }
                if (*p != '\\')
                {
                    put(static_cast<u8>(*p));
                    continue;
                }
                p++;
                u32 code;
                switch (*p)
                {
                case '"': case '\\': case '/': put(static_cast<u8>(*p)); break;
                case 'n': put('\n'); break;
                case 't': put('\t'); break;
                case 'r': put('\r'); break;
                case 'b': put('\b'); break;
                case 'f': put('\f'); break;
                case 'u':
                    if (!hex(p + 1, code))
                    {
                        return nullptr;
                    }
                    p += 4;
                    if (code < 0x80)
                    {
                        put(code);
                    }
                    else if (code < 0x800)
                    {
                        put(0xC0 | (code >> 6));
                        put(0x80 | (code & 0x3F));
                    }
                    else
                    {
                        put(0xE0 | (code >> 12));
                        put(0x80 | ((code >> 6) & 0x3F));
                        put(0x80 | (code & 0x3F));
                    }
                    break;
                default:
                    return nullptr;
                }
            }
            if (out && size)
            {
                out[length] = 0;
            }
            return p + 1;
        }

        const char* skipValue(const char* p, int depth)
        {
            if (p == nullptr || depth > kMaxDepth)
            {
                return nullptr;
            }
            p = space(p);
            if (*p == '"')
            {
                return readString(p, nullptr, 0);
            }
            if (*p == '{' || *p == '[')
            {
                const bool object = *p == '{';
                const char close = object ? '}' : ']';
                p = space(p + 1);
                if (*p == close)
                {
                    return p + 1;
                }
                for (;;)
                {
                    if (object)
                    {
                        p = space(p);
                        p = *p == '"' ? readString(p, nullptr, 0) : nullptr;
                        if (p == nullptr || *(p = space(p)) != ':')
                        {
                            return nullptr;
                        }
                        p++;
                    }
                    p = skipValue(p, depth + 1);
                    if (p == nullptr)
                    {
                        return nullptr;
                    }
                    p = space(p);
                    if (*p == close)
                    {
                        return p + 1;
                    }
                    if (*p++ != ',')
                    {
                        return nullptr;
                    }
                }
            }
            static const char* const words[] = { "true", "false", "null" };
            for (const char* word : words)
            {
                const size_t length = strlen(word);
                if (strncmp(p, word, length) == 0)
                {
                    return p + length;
                }
            }
            char* end = nullptr;
            if (*p == '-' || (*p >= '0' && *p <= '9'))
            {
                strtod(p, &end);
            }
            return end;
        }

        const char* findValue(const char* p, const char* key)
        {
            p = space(p);
            if (*p != '{')
            {
                return nullptr;
            }
            p = space(p + 1);
            while (*p == '"')
            {
                char name[64];
                p = readString(p, name, sizeof(name));
                if (p == nullptr || *(p = space(p)) != ':')
                {
                    return nullptr;
                }
                p = space(p + 1);
                if (strcmp(name, key) == 0)
                {
                    return p;
                }
                p = skipValue(p, 0);
                if (p == nullptr || *(p = space(p)) != ',')
                {
                    return nullptr;
                }
                p = space(p + 1);
            }
            return nullptr;
        }
    }

    void jsonEachString(const char* text, char* buffer, size_t size, JsonString callback, void* user)
    {
        const char* p = space(text);
        if (*p != '{')
        {
            return;
        }
        p = space(p + 1);
        while (*p == '"')
        {
            char name[64];
            p = readString(p, name, sizeof(name));
            if (p == nullptr || *(p = space(p)) != ':')
            {
                return;
            }
            p = space(p + 1);
            const bool isString = *p == '"';
            p = isString ? readString(p, buffer, size) : skipValue(p, 0);
            if (p == nullptr)
            {
                return;
            }
            if (isString)
            {
                callback(name, buffer, user);
            }
            p = space(p);
            if (*p != ',')
            {
                return;
            }
            p = space(p + 1);
        }
    }

    bool jsonString(const char* text, const char* key, char* out, size_t size)
    {
        const char* p = findValue(text, key);
        return p && *p == '"' && readString(p, out, size) != nullptr;
    }

    bool jsonNumber(const char* text, const char* key, double& out)
    {
        const char* p = findValue(text, key);
        if (p == nullptr || !(*p == '-' || (*p >= '0' && *p <= '9')))
        {
            return false;
        }
        char* end = nullptr;
        const double value = strtod(p, &end);
        if (end == p)
        {
            return false;
        }
        out = value;
        return true;
    }
}
