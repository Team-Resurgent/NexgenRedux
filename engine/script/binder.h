#pragma once

#include "script/bind.h"
#include "script/script_host.h"

#include <string>

namespace nx
{
    // Registers the script API and writes its reference from the same descriptions. Something with no
    // description is refused, so the reference can't fall behind.
    class Binder : NonCopyable
    {
    public:
        explicit Binder(ScriptHost& host) : m_host(host), m_engine(host.engine()) {}

        // Starts a chapter of the reference.
        void section(const char* title, const char* doc);

        void valueType(const char* name, size_t size, asQWORD flags, const char* doc);
        // Reference counted unless flags say otherwise.
        void refType(const char* name, asQWORD flags, const char* doc);
        void enumType(const char* name, const char* doc);
        void enumValue(const char* value, int number, const char* doc);
        void interfaceType(const char* name, const char* doc);

        void funcdef(const char* declaration, const char* doc);
        // More for a type that was named before.
        void reopen(const char* name);

        // These belong to the type named last.
        void method(const char* declaration, const asSFuncPtr& fn, asDWORD convention, const char* doc);
        void property(const char* declaration, size_t offset, const char* doc);
        void constructor(const char* declaration, const asSFuncPtr& fn, asDWORD convention, const char* doc);
        // Plumbing that scripts never name, so not in the reference.
        void behaviour(asEBehaviours kind, const char* declaration, const asSFuncPtr& fn, asDWORD convention);
        // Documents something a script provides and the engine calls.
        void callback(const char* declaration, const char* doc);

        // Functions and constants in a namespace ("" for none).
        void names(const char* name) { m_namespace = name; }
        void function(const char* declaration, const asSFuncPtr& fn, asDWORD convention, const char* doc);
        void constant(const char* declaration, void* value, const char* doc);

        bool ok() const { return m_ok; }
        u32 count() const { return m_count; }
        const std::string& reference() const { return m_text; }

    private:
        bool described(const char* declaration, const char* doc);
        void entry(const char* declaration, const char* doc);

        ScriptHost& m_host;
        asIScriptEngine* m_engine;
        std::string m_text;
        std::string m_type;
        std::string m_namespace;
        u32 m_count = 0;
        bool m_ok = true;
    };
}
