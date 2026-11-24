#include "script/binder.h"
#include "base/log.h"

#include <string.h>

namespace nx
{
    bool Binder::described(const char* declaration, const char* doc)
    {
        // Passed differently by AngelScript and by clang on 64-bit Unix. int is never slower.
        static const char* const kSmall[] = { "int8 ", "int16 " };
        for (const char* small : kSmall)
        {
            if (strstr(declaration, small))
            {
                NX_ERROR("%s: use int or uint", declaration);
                m_ok = false;
            }
        }
        if (doc == nullptr || doc[0] == 0)
        {
            NX_ERROR("no description for %s", declaration);
            m_ok = false;
            return false;
        }
        return true;
    }

    void Binder::cased(const char* name, size_t length, bool capital)
    {
        if (length == 0)
        {
            return;
        }
        const bool upper = name[0] >= 'A' && name[0] <= 'Z';
        if (upper != capital || memchr(name, '_', length))
        {
            NX_ERROR("%.*s should begin with a %s letter and have no underscores", static_cast<int>(length), name, capital ? "capital" : "small");
            m_ok = false;
        }
    }

    // "int get_x() const property" is about x, and "Vec2 opAdd(...)" is AngelScript's own name.
    void Binder::casedName(const char* declaration, bool capital)
    {
        const std::string text = declaration;
        const size_t bracket = text.find('(');
        const size_t end = bracket == std::string::npos ? text.size() : bracket;
        size_t start = text.rfind(' ', end - 1);
        start = start == std::string::npos ? 0 : start + 1;
        if (text.compare(start, 4, "get_") == 0 || text.compare(start, 4, "set_") == 0)
        {
            start += 4;
        }
        cased(text.c_str() + start, end - start, capital);
    }

    void Binder::entry(const char* declaration, const char* doc)
    {
        if (described(declaration, doc))
        {
            m_text += "| `";
            if (!m_namespace.empty())
            {
                // The namespace goes before the name, which follows the return type.
                const std::string text = declaration;
                const size_t bracket = text.find('(');
                const size_t space = text.rfind(' ', bracket == std::string::npos ? bracket : bracket - 1);
                m_text += text.substr(0, space + 1) + m_namespace + "::" + text.substr(space + 1);
            }
            else
            {
                m_text += declaration;
            }
            m_text += "` | ";
            m_text += doc;
            m_text += " |\n";
            m_count++;
        }
    }

    void Binder::section(const char* title, const char* doc)
    {
        m_text += "\n## ";
        m_text += title;
        m_text += "\n\n";
        m_text += doc;
        m_text += "\n";
        m_text += "\n| | |\n|---|---|\n";
    }

    void Binder::valueType(const char* name, size_t size, asQWORD flags, const char* doc)
    {
        m_type = name;
        m_namespace.clear();
        cased(name, strlen(name), true);
        m_ok = m_host.check(m_engine->RegisterObjectType(name, static_cast<int>(size), asOBJ_VALUE | flags), name) && m_ok;
        section(name, doc);
    }

    void Binder::refType(const char* name, asQWORD flags, const char* doc)
    {
        m_type = name;
        m_namespace.clear();
        cased(name, strlen(name), true);
        m_ok = m_host.check(m_engine->RegisterObjectType(name, 0, asOBJ_REF | flags), name) && m_ok;
        section(name, doc);
    }

    void Binder::enumType(const char* name, const char* doc)
    {
        m_type = name;
        m_namespace.clear();
        cased(name, strlen(name), true);
        m_ok = m_host.check(m_engine->RegisterEnum(name), name) && m_ok;
        section(name, doc);
    }

    void Binder::enumValue(const char* value, int number, const char* doc)
    {
        cased(value, strlen(value), true);
        m_ok = m_host.check(m_engine->RegisterEnumValue(m_type.c_str(), value, number), value) && m_ok;
        entry(value, doc);
    }

    void Binder::interfaceType(const char* name, const char* doc)
    {
        m_type = name;
        m_namespace.clear();
        cased(name, strlen(name), true);
        m_ok = m_host.check(m_engine->RegisterInterface(name), name) && m_ok;
        section(name, doc);
    }

    void Binder::funcdef(const char* declaration, const char* doc)
    {
        m_namespace.clear();
        m_ok = m_host.check(m_engine->RegisterFuncdef(declaration), declaration) && m_ok;
        described(declaration, doc);
        section(declaration, doc);
    }

    void Binder::reopen(const char* name)
    {
        m_type = name;
        m_namespace.clear();
        m_text += "\n### ";
        m_text += name;
        m_text += "\n\n| | |\n|---|---|\n";
    }

    void Binder::method(const char* declaration, const asSFuncPtr& fn, asDWORD convention, const char* doc)
    {
        casedName(declaration, false);
        m_ok = m_host.check(m_engine->RegisterObjectMethod(m_type.c_str(), declaration, fn, convention), declaration) && m_ok;
        entry(declaration, doc);
    }

    void Binder::property(const char* declaration, size_t offset, const char* doc)
    {
        casedName(declaration, false);
        m_ok = m_host.check(m_engine->RegisterObjectProperty(m_type.c_str(), declaration, static_cast<int>(offset)), declaration) && m_ok;
        entry(declaration, doc);
    }

    void Binder::constructor(const char* declaration, const asSFuncPtr& fn, asDWORD convention, const char* doc)
    {
        m_ok = m_host.check(m_engine->RegisterObjectBehaviour(m_type.c_str(), asBEHAVE_CONSTRUCT, declaration, fn, convention), declaration) && m_ok;
        // Registered as "void f(...)", written as "Type(...)".
        const std::string text = declaration;
        entry((m_type + text.substr(text.find('('))).c_str(), doc);
    }

    void Binder::behaviour(asEBehaviours kind, const char* declaration, const asSFuncPtr& fn, asDWORD convention)
    {
        m_ok = m_host.check(m_engine->RegisterObjectBehaviour(m_type.c_str(), kind, declaration, fn, convention), declaration) && m_ok;
    }

    void Binder::callback(const char* declaration, const char* doc)
    {
        entry(declaration, doc);
    }

    void Binder::function(const char* declaration, const asSFuncPtr& fn, asDWORD convention, const char* doc)
    {
        m_engine->SetDefaultNamespace(m_namespace.c_str());
        casedName(declaration, false);
        m_ok = m_host.check(m_engine->RegisterGlobalFunction(declaration, fn, convention), declaration) && m_ok;
        m_engine->SetDefaultNamespace("");
        entry(declaration, doc);
    }

    void Binder::constant(const char* declaration, void* value, const char* doc)
    {
        m_engine->SetDefaultNamespace(m_namespace.c_str());
        casedName(declaration, true);
        m_ok = m_host.check(m_engine->RegisterGlobalProperty(declaration, value), declaration) && m_ok;
        m_engine->SetDefaultNamespace("");
        entry(declaration, doc);
    }
}
