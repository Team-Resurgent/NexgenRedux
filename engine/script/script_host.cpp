#include "script/script_host.h"
#include "base/log.h"
#include "base/memory.h"
#include "script/bind.h"

#include "scriptarray/scriptarray.h"
#include "scriptmath/scriptmath.h"
#include "scriptstdstring/scriptstdstring.h"

#include <stdio.h>
#include <string.h>
#include <string>

namespace nx
{
    namespace
    {
        void* scriptAlloc(size_t size)
        {
            return memAlloc(size, MemTag::Script);
        }

        void scriptFree(void* ptr)
        {
            memFree(ptr);
        }

        void scriptPrint(const std::string& text)
        {
            NX_INFO("script: %s", text.c_str());
        }
    }

    ScriptHost::~ScriptHost()
    {
        shutdown();
    }

    const char* ScriptHost::version()
    {
        return asGetLibraryVersion();
    }

    bool ScriptHost::usesNativeCalls()
    {
#if defined(NX_SCRIPT_GENERIC)
        return false;
#else
        return strstr(asGetLibraryOptions(), "AS_MAX_PORTABILITY") == nullptr;
#endif
    }

    bool ScriptHost::check(int result, const char* declaration)
    {
        if (result < 0)
        {
            NX_ERROR("script binding refused (%d): %s", result, declaration);
        }
        return result >= 0;
    }

    void ScriptHost::messageCallback(const void* data, void*)
    {
        const asSMessageInfo* message = static_cast<const asSMessageInfo*>(data);
        const LogLevel level = message->type == asMSGTYPE_ERROR ? LogLevel::Error : message->type == asMSGTYPE_WARNING ? LogLevel::Warning : LogLevel::Info;
        log(level, "%s (%d, %d): %s", message->section, message->row, message->col, message->message);
    }

    bool ScriptHost::init()
    {
        NX_ASSERT(m_engine == nullptr);
        asSetGlobalMemoryFunctions(scriptAlloc, scriptFree);

        m_engine = asCreateScriptEngine();
        if (m_engine == nullptr)
        {
            NX_ERROR("asCreateScriptEngine failed");
            return false;
        }
        m_engine->SetMessageCallback(asFUNCTION(messageCallback), this, asCALL_CDECL);
        // Collection is stepped by collectStep, so a frame never pays for a full pass.
        m_engine->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, false);
        // compile() runs them, so they have the same time limit as everything else.
        m_engine->SetEngineProperty(asEP_INIT_GLOBAL_VARS_AFTER_BUILD, false);

        RegisterStdString(m_engine);
        RegisterScriptArray(m_engine, true);
        RegisterStdStringUtils(m_engine);
        RegisterScriptMath(m_engine);

        bool ok = check(m_engine->RegisterGlobalFunction("void print(const string &in)", NX_FN(scriptPrint)), "print");
        ok = bindMath(*this) && ok;

        m_context = m_engine->CreateContext();
        if (m_context == nullptr)
        {
            return false;
        }
        m_context->SetLineCallback(asFUNCTION(lineCallback), this, asCALL_CDECL);

        // AngelScript makes these again for each module that uses them, and keeps a little of every one
        // that was filled from a list. Held here, they are made once.
        static const char* const arrays[] = { "array<int>", "array<uint>", "array<float>", "array<bool>", "array<string>", "array<Vec2>", "array<Color>" };
        for (const char* declaration : arrays)
        {
            hold(declaration);
        }
        return ok;
    }

    void ScriptHost::shutdown()
    {
        if (m_context)
        {
            m_context->Release();
            m_context = nullptr;
        }
        for (asITypeInfo*& type : m_held)
        {
            if (type)
            {
                type->Release();
                type = nullptr;
            }
        }
        m_heldCount = 0;
        if (m_engine)
        {
            m_engine->ShutDownAndRelease();
            m_engine = nullptr;
        }
    }

    asITypeInfo* ScriptHost::hold(const char* declaration)
    {
        asITypeInfo* type = m_heldCount < kHeldTypes ? m_engine->GetTypeInfoByDecl(declaration) : nullptr;
        if (type)
        {
            type->AddRef();
            m_held[m_heldCount++] = type;
        }
        return type;
    }

    asITypeInfo* ScriptHost::findType(const char* module, const char* name)
    {
        asIScriptModule* m = m_engine->GetModule(module, asGM_ONLY_IF_EXISTS);
        return m ? m->GetTypeInfoByName(name) : nullptr;
    }

    void ScriptHost::addSection(const char* file, const char* code, size_t length)
    {
        m_sections.emplace_back(file, std::string(code, length));
    }

    bool ScriptHost::compile(const char* module, const char* file, const char* code, size_t length)
    {
        asIScriptModule* m = m_engine->GetModule(module, asGM_ALWAYS_CREATE);
        bool added = m != nullptr;
        for (const auto& section : m_sections)
        {
            added = added && m->AddScriptSection(section.first.c_str(), section.second.data(), section.second.size()) >= 0;
        }
        m_sections.clear();
        if (!added || (m_prelude && m->AddScriptSection("engine", m_prelude) < 0) || m->AddScriptSection(file, code, length) < 0 || m->Build() < 0)
        {
            snprintf(m_error, sizeof(m_error), "%s did not compile", file);
            discard(module);
            return false;
        }
        m_statements = 0;
        if (m->ResetGlobalVars(m_context) < 0)
        {
            const char* section = nullptr;
            const int line = m_context->GetState() == asEXECUTION_EXCEPTION ? m_context->GetExceptionLineNumber(nullptr, &section) : 0;
            snprintf(m_error, sizeof(m_error), "%s:%d: %s", section ? section : file, line, m_context->GetState() == asEXECUTION_EXCEPTION ? m_context->GetExceptionString() : "a global variable could not be set up");
            NX_ERROR("%s", m_error);
            discard(module);
            return false;
        }
        return true;
    }

    asITypeInfo* ScriptHost::findClass(const char* module, const char* interfaceName)
    {
        asIScriptModule* m = m_engine->GetModule(module, asGM_ONLY_IF_EXISTS);
        const asITypeInfo* wanted = m_engine->GetTypeInfoByName(interfaceName);
        for (asUINT i = 0; m && wanted && i < m->GetObjectTypeCount(); i++)
        {
            asITypeInfo* type = m->GetObjectTypeByIndex(i);
            if (type->Implements(wanted) && !(type->GetFlags() & asOBJ_ABSTRACT))
            {
                return type;
            }
        }
        return nullptr;
    }

    asIScriptObject* ScriptHost::create(asITypeInfo* type)
    {
        if (asGetActiveContext())
        {
            // From inside a script, whose context is busy: this nests in it, limits and all.
            return static_cast<asIScriptObject*>(m_engine->CreateScriptObject(type));
        }
        asIScriptFunction* factory = nullptr;
        for (asUINT i = 0; type && i < type->GetFactoryCount(); i++)
        {
            asIScriptFunction* f = type->GetFactoryByIndex(i);
            factory = f->GetParamCount() == 0 ? f : factory;
        }
        if (!prepare(factory) || !run())
        {
            return nullptr;
        }
        asIScriptObject* object = *static_cast<asIScriptObject**>(m_context->GetAddressOfReturnValue());
        if (object)
        {
            object->AddRef();
        }
        m_context->Unprepare();
        return object;
    }

    void ScriptHost::release(asIScriptObject* object)
    {
        if (object)
        {
            m_context->Unprepare();
            object->Release();
        }
    }

    bool ScriptHost::prepareMethod(asIScriptObject* object, asIScriptFunction* method)
    {
        return prepare(method) && m_context->SetObject(object) >= 0;
    }

    bool ScriptHost::callMethod(asIScriptObject* object, asIScriptFunction* method)
    {
        return prepareMethod(object, method) && run();
    }

    bool ScriptHost::callMethod(asIScriptObject* object, asIScriptFunction* method, float argument)
    {
        return prepareMethod(object, method) && m_context->SetArgFloat(0, argument) >= 0 && run();
    }

    bool ScriptHost::callMethod(asIScriptObject* object, asIScriptFunction* method, void* argument)
    {
        return prepareMethod(object, method) && m_context->SetArgObject(0, argument) >= 0 && run();
    }

    bool ScriptHost::callEvent(asIScriptFunction* function, void* first, void* second, const int* number)
    {
        const bool within = m_context->GetState() == asEXECUTION_ACTIVE;
        const u32 statements = m_statements;
        if (within && m_context->PushState() < 0)
        {
            snprintf(m_error, sizeof(m_error), "too many calls inside one another");
            return false;
        }
        bool ok = prepare(function);
        asUINT at = 0;
        for (void* object : { first, second })
        {
            ok = ok && (object == nullptr || m_context->SetArgObject(at++, object) >= 0);
        }
        ok = ok && (number == nullptr || m_context->SetArgDWord(at, static_cast<asDWORD>(*number)) >= 0);
        ok = ok && run();
        if (within)
        {
            m_context->PopState();
            m_statements = statements;
        }
        return ok;
    }

    void ScriptHost::discard(const char* module)
    {
        m_context->Unprepare();
        m_engine->DiscardModule(module);
    }

    asIScriptFunction* ScriptHost::find(const char* module, const char* declaration)
    {
        asIScriptModule* m = m_engine->GetModule(module, asGM_ONLY_IF_EXISTS);
        return m ? m->GetFunctionByDecl(declaration) : nullptr;
    }

    void ScriptHost::lineCallback(asIScriptContext* context, void* data)
    {
        ScriptHost* host = static_cast<ScriptHost*>(data);
        if (++host->m_statements > host->m_statementLimit)
        {
            context->Abort();
        }
    }

    bool ScriptHost::prepare(asIScriptFunction* function)
    {
        m_statements = 0;
        m_error[0] = 0;
        if (function == nullptr)
        {
            snprintf(m_error, sizeof(m_error), "function not found");
            return false;
        }
        return m_context->Prepare(function) >= 0;
    }

    bool ScriptHost::run()
    {
        return finished(m_context, m_context->Execute());
    }

    asIScriptContext* ScriptHost::createContext()
    {
        asIScriptContext* context = m_engine->CreateContext();
        if (context)
        {
            context->SetLineCallback(asFUNCTION(lineCallback), this, asCALL_CDECL);
        }
        return context;
    }

    bool ScriptHost::resume(asIScriptContext* context, bool& suspended)
    {
        m_statements = 0;
        m_error[0] = 0;
        const int result = context->Execute();
        suspended = result == asEXECUTION_SUSPENDED;
        return suspended || finished(context, result);
    }

    bool ScriptHost::finished(asIScriptContext* context, int result)
    {
        if (result == asEXECUTION_FINISHED)
        {
            return true;
        }

        const char* section = nullptr;
        if (result == asEXECUTION_EXCEPTION)
        {
            const int line = context->GetExceptionLineNumber(nullptr, &section);
            snprintf(m_error, sizeof(m_error), "%s:%d: %s", section ? section : "?", line, context->GetExceptionString());
        }
        else if (result == asEXECUTION_ABORTED)
        {
            const int line = context->GetLineNumber(0, nullptr, &section);
            snprintf(m_error, sizeof(m_error), "%s:%d: stopped after %u statements (endless loop?)", section ? section : "?", line, static_cast<unsigned>(m_statementLimit));
        }
        else
        {
            snprintf(m_error, sizeof(m_error), "script ended with code %d", result);
        }
        NX_ERROR("%s", m_error);
        return false;
    }

    bool ScriptHost::call(asIScriptFunction* function)
    {
        return prepare(function) && run();
    }

    bool ScriptHost::call(asIScriptFunction* function, float argument)
    {
        return prepare(function) && m_context->SetArgFloat(0, argument) >= 0 && run();
    }

    bool ScriptHost::callInt(asIScriptFunction* function, int& result)
    {
        if (!prepare(function) || !run())
        {
            return false;
        }
        result = static_cast<int>(m_context->GetReturnDWord());
        return true;
    }

    void ScriptHost::collectStep()
    {
        // A step looks at about one object, and finding a loop takes several passes over all of them.
        // Doing a share of them each frame keeps pace however many are made.
        asUINT objects = 0;
        m_engine->GetGCStatistics(&objects);
        const asUINT share = objects / 8 + 1;
        m_engine->GarbageCollect(asGC_ONE_STEP | asGC_DESTROY_GARBAGE | asGC_DETECT_GARBAGE, share < 1000 ? share : 1000);
    }

    void ScriptHost::collectAll()
    {
        m_context->Unprepare();
        m_engine->GarbageCollect(asGC_FULL_CYCLE);
    }
}
