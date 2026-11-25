#pragma once

#include "base/types.h"

class asIScriptEngine;
class asIScriptContext;
class asIScriptFunction;
class asIScriptObject;
class asITypeInfo;

#include <string>
#include <utility>
#include <vector>

namespace nx
{
    // Owns the AngelScript engine. All its memory is tagged MemTag::Script.
    class ScriptHost : NonCopyable
    {
    public:
        ~ScriptHost();

        // Registers strings, arrays, maths and the engine's value types.
        bool init();
        void shutdown();

        asIScriptEngine* engine() { return m_engine; }

        // Registration helper: logs the declaration if a binding is refused.
        bool check(int result, const char* declaration);

        // Code that every module is given. Must stay valid.
        void setPrelude(const char* code) { m_prelude = code; }
        // Keeps a type for as long as the host, so that it isn't made again for each module.
        asITypeInfo* hold(const char* declaration);

        // Compile errors are logged with file, line and column.
        bool compile(const char* module, const char* file, const char* code, size_t length);
        // Other files, put in with the next to be compiled: those that it includes.
        void addSection(const char* file, const char* code, size_t length);
        void discard(const char* module);

        // Looks up a function by declaration; nullptr if the module doesn't have it.
        asIScriptFunction* find(const char* module, const char* declaration);
        // A class in a module by name, or nullptr.
        asITypeInfo* findType(const char* module, const char* name);
        // The first class in a module that implements an interface.
        asITypeInfo* findClass(const char* module, const char* interfaceName);

        // Makes a script object, or nullptr. Release it with release().
        asIScriptObject* create(asITypeInfo* type);
        void release(asIScriptObject* object);

        // Runs a function. A script error or a runaway loop stops that call, is logged with its
        // line, and returns false; the engine carries on. Not for use inside another call.
        bool call(asIScriptFunction* function);
        bool call(asIScriptFunction* function, float argument);
        bool callInt(asIScriptFunction* function, int& result);
        bool callMethod(asIScriptObject* object, asIScriptFunction* method);
        bool callMethod(asIScriptObject* object, asIScriptFunction* method, float argument);
        bool callMethod(asIScriptObject* object, asIScriptFunction* method, void* argument);
        // A function, or a method bound to its object, with up to two objects and then a number. May be
        // used inside another call, which waits.
        bool callEvent(asIScriptFunction* function, void* first, void* second, const int* number);

        // For coroutines: a context of their own, with the same limits.
        asIScriptContext* createContext();
        bool resume(asIScriptContext* context, bool& suspended);

        const char* lastError() const { return m_error; }

        // Statements a single call may run before it is treated as stuck.
        void setStatementLimit(u32 statements) { m_statementLimit = statements; }

        // Some garbage collection, for calling every frame: more when there is more to look at.
        void collectStep();
        // Everything at once, for screen changes.
        void collectAll();

        static const char* version();
        static bool usesNativeCalls();

    private:
        bool prepare(asIScriptFunction* function);
        bool prepareMethod(asIScriptObject* object, asIScriptFunction* method);
        bool run();
        bool finished(asIScriptContext* context, int result);
        static void lineCallback(asIScriptContext* context, void* host);
        static void messageCallback(const void* message, void* host);

        asIScriptEngine* m_engine = nullptr;
        asIScriptContext* m_context = nullptr;
        static constexpr u32 kHeldTypes = 12;
        u32 m_heldCount = 0;
        const char* m_prelude = nullptr;
        std::vector<std::pair<std::string, std::string>> m_sections;
        asITypeInfo* m_held[kHeldTypes] = {};
        u32 m_statementLimit = 20000000;
        u32 m_statements = 0;
        char m_error[256] = {};
    };

    // Vec2 and Vec3, with every operator. Called by ScriptHost::init.
    bool bindMath(ScriptHost& host);

    // Calls native functions of many shapes from script and checks what comes back. This is how a
    // platform's calling convention is proved. Returns the number of failures, or -1 if it can't run.
    int runScriptAbiTests(ScriptHost& host, int& checks);
}
