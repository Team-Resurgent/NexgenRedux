#include "script/binder.h"
#include "base/log.h"

namespace nx
{
    bool Binder::described(const char* declaration, const char* doc)
    {
        if (doc == nullptr || doc[0] == 0)
        {
            NX_ERROR("no description for %s", declaration);
            m_ok = false;
            return false;
        }
        return true;
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
        m_ok = m_host.check(m_engine->RegisterObjectType(name, static_cast<int>(size), asOBJ_VALUE | flags), name) && m_ok;
        section(name, doc);
    }

    void Binder::refType(const char* name, asQWORD flags, const char* doc)
    {
        m_type = name;
        m_namespace.clear();
        m_ok = m_host.check(m_engine->RegisterObjectType(name, 0, asOBJ_REF | flags), name) && m_ok;
        section(name, doc);
    }

    void Binder::enumType(const char* name, const char* doc)
    {
        m_type = name;
        m_namespace.clear();
        m_ok = m_host.check(m_engine->RegisterEnum(name), name) && m_ok;
        section(name, doc);
    }

    void Binder::enumValue(const char* value, int number, const char* doc)
    {
        m_ok = m_host.check(m_engine->RegisterEnumValue(m_type.c_str(), value, number), value) && m_ok;
        entry(value, doc);
    }

    void Binder::interfaceType(const char* name, const char* doc)
    {
        m_type = name;
        m_namespace.clear();
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
        m_ok = m_host.check(m_engine->RegisterObjectMethod(m_type.c_str(), declaration, fn, convention), declaration) && m_ok;
        entry(declaration, doc);
    }

    void Binder::property(const char* declaration, size_t offset, const char* doc)
    {
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
        m_ok = m_host.check(m_engine->RegisterGlobalFunction(declaration, fn, convention), declaration) && m_ok;
        m_engine->SetDefaultNamespace("");
        entry(declaration, doc);
    }

    void Binder::constant(const char* declaration, void* value, const char* doc)
    {
        m_engine->SetDefaultNamespace(m_namespace.c_str());
        m_ok = m_host.check(m_engine->RegisterGlobalProperty(declaration, value), declaration) && m_ok;
        m_engine->SetDefaultNamespace("");
        entry(declaration, doc);
    }
}
