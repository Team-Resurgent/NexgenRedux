#include "script/debugger.h"
#include "base/json_value.h"
#include "base/log.h"

#include "scriptarray/scriptarray.h"

#include <angelscript.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace nx
{
    namespace
    {
        ScriptDebugger* g_debugger = nullptr;
        LogSink g_previousSink = nullptr;

        constexpr u32 kMostChildren = 500;

        // `string get_text() const property` and the like, of the engine's own types: what can be asked
        // without running any script.
        bool isGetter(asIScriptFunction* f)
        {
            asDWORD flags = 0;
            return f && f->GetFuncType() == asFUNC_SYSTEM && strncmp(f->GetName(), "get_", 4) == 0 && f->GetParamCount() == 0 &&
                   f->GetReturnTypeId(&flags) != asTYPEID_VOID && f->IsProperty();
        }
        constexpr size_t kLongestText = 400;

        void putNumber(JsonValue& to, const char* key, double number)
        {
            to.put(key)->setNumber(number);
        }

        void putText(JsonValue& to, const char* key, const std::string& text)
        {
            to.put(key)->setText(text);
        }

        // Paths as the engine has them and as a debugger sends them may differ in slashes and case.
        bool samePath(const std::string& a, const char* b)
        {
            const size_t n = strlen(b);
            if (a.size() != n)
            {
                return false;
            }
            for (size_t i = 0; i < n; i++)
            {
                char x = a[i] == '\\' ? '/' : a[i];
                char y = b[i] == '\\' ? '/' : b[i];
                x = (x >= 'A' && x <= 'Z') ? static_cast<char>(x + 32) : x;
                y = (y >= 'A' && y <= 'Z') ? static_cast<char>(y + 32) : y;
                if (x != y)
                {
                    return false;
                }
            }
            return true;
        }

        std::string formatNumber(double value)
        {
            char text[48];
            snprintf(text, sizeof(text), "%.9g", value);
            return text;
        }
    }

    bool ScriptDebugger::start(ScriptHost& host, u16 port, bool onlyHere)
    {
        if (!netStart())
        {
            NX_ERROR("debugger: no network");
            return false;
        }
        if (!m_listener.listen(port, onlyHere))
        {
            NX_ERROR("debugger: cannot listen on port %u", static_cast<unsigned>(port));
            netStop();
            return false;
        }
        m_host = &host;
        m_stringType = host.engine()->GetTypeIdByDecl("string");
        host.setWatcher(this);
        g_debugger = this;
        g_previousSink = logSink();
        setLogSink([](LogLevel level, const char* message) {
            g_previousSink(level, message);
            if (g_debugger)
            {
                static const char* const levels[] = { "info", "warn", "error" };
                JsonValue line;
                putText(line, "event", "output");
                putText(line, "level", levels[static_cast<int>(level)]);
                putText(line, "text", message);
                Lock lock(g_debugger->m_logLock);
                g_debugger->m_log += line.write(true);
                g_debugger->m_log += '\n';
            }
        });
        NX_INFO("debugger: waiting on port %u", static_cast<unsigned>(port));
        return true;
    }

    void ScriptDebugger::stop()
    {
        if (m_host == nullptr)
        {
            return;
        }
        setLogSink(g_previousSink);
        g_debugger = nullptr;
        m_host->setWatcher(nullptr);
        m_host = nullptr;
        m_client.close();
        m_listener.close();
        netStop();
    }

    void ScriptDebugger::waitForReady()
    {
        while (m_host && !m_ready)
        {
            update();
            if (m_idle)
            {
                m_idle(m_idleUser);
            }
            sleepMilliseconds(10);
        }
    }

    void ScriptDebugger::update()
    {
        if (m_host == nullptr)
        {
            return;
        }
        if (m_client.state() != TcpState::Open)
        {
            m_client.close();
            Endpoint from;
            if (m_listener.accept(m_client, from))
            {
                // A new one starts with nothing set.
                m_breakpoints.clear();
                m_files.clear();
                m_mode = Mode::Run;
                m_in.clear();
                m_out.clear();
                {
                    Lock lock(m_logLock);
                    m_log.clear();
                }
                NX_INFO("debugger: connected");
            }
        }
        pump();
    }

    void ScriptDebugger::pump()
    {
        if (m_client.state() != TcpState::Open)
        {
            return;
        }
        {
            Lock lock(m_logLock);
            m_out += m_log;
            m_log.clear();
        }
        char buffer[4096];
        size_t got;
        while ((got = m_client.receive(buffer, sizeof(buffer))) > 0)
        {
            m_in.append(buffer, got);
        }
        size_t end;
        while ((end = m_in.find('\n')) != std::string::npos)
        {
            const std::string line = m_in.substr(0, end);
            m_in.erase(0, end + 1);
            handle(line.data(), line.size());
        }
        flush();
        if (m_client.state() != TcpState::Open)
        {
            // Gone: nothing is to stop the scripts any more.
            m_breakpoints.clear();
            m_files.clear();
            m_mode = Mode::Run;
            m_resume = true;
            m_ready = true;
        }
    }

    void ScriptDebugger::send(const std::string& line)
    {
        m_out += line;
        m_out += '\n';
    }

    void ScriptDebugger::flush()
    {
        while (!m_out.empty() && m_client.state() == TcpState::Open)
        {
            const size_t sent = m_client.send(m_out.data(), m_out.size());
            if (sent == 0)
            {
                break;
            }
            m_out.erase(0, sent);
        }
    }

    void ScriptDebugger::handle(const char* text, size_t size)
    {
        JsonValue ask;
        std::string error;
        if (!ask.parse(text, size, error))
        {
            return;
        }
        const JsonValue* cmd = ask.find("cmd");
        const JsonValue* idValue = ask.find("id");
        const int id = idValue ? static_cast<int>(idValue->number()) : 0;
        const std::string name = cmd ? cmd->text() : std::string();
        if (name == "breakpoints")
        {
            const JsonValue* file = ask.find("file");
            const JsonValue* lines = ask.find("lines");
            if (file == nullptr)
            {
                return;
            }
            Breakpoints* these = nullptr;
            for (Breakpoints& b : m_breakpoints)
            {
                these = samePath(b.file, file->text().c_str()) ? &b : these;
            }
            if (these == nullptr)
            {
                m_breakpoints.push_back({ file->text(), {} });
                these = &m_breakpoints.back();
            }
            these->lines.clear();
            for (u32 i = 0; lines && i < lines->count(); i++)
            {
                these->lines.push_back(static_cast<int>(lines->at(i)->number()));
            }
            m_files.clear();
            JsonValue reply;
            putNumber(reply, "id", id);
            send(reply.write(true));
        }
        else if (name == "ready")
        {
            m_ready = true;
            JsonValue reply;
            putNumber(reply, "id", id);
            send(reply.write(true));
        }
        else if (name == "hello")
        {
            JsonValue reply;
            putText(reply, "event", "hello");
            putNumber(reply, "version", 1);
            putText(reply, "angelscript", ScriptHost::version());
            putNumber(reply, "id", id);
            send(reply.write(true));
        }
        else if (name == "pause")
        {
            // Also just after being told to go on, in the same breath.
            if (!m_paused || m_resume)
            {
                m_mode = Mode::Pause;
            }
        }
        else if (name == "continue" || name == "next" || name == "stepIn" || name == "stepOut")
        {
            if (m_paused)
            {
                resume(name == "continue" ? Mode::Run : name == "next" ? Mode::StepOver : name == "stepIn" ? Mode::StepIn : Mode::StepOut);
            }
        }
        else if (name == "stack")
        {
            stack(id);
        }
        else if (name == "variables")
        {
            variables(id, ask);
        }
        else if (name == "evaluate")
        {
            evaluate(id, ask);
        }
    }

    int ScriptDebugger::fileOf(const char* section)
    {
        for (const auto& known : m_files)
        {
            if (known.first == section)
            {
                return known.second;
            }
        }
        const char* path = m_host->sourceOf(section);
        int found = -1;
        for (size_t i = 0; i < m_breakpoints.size(); i++)
        {
            found = samePath(m_breakpoints[i].file, path) ? static_cast<int>(i) : found;
        }
        m_files.emplace_back(section, found);
        return found;
    }

    void ScriptDebugger::line(asIScriptContext* context)
    {
        if (m_paused || m_client.state() != TcpState::Open)
        {
            return;
        }
        // Now and then, so that "pause" reaches a script that never ends its frame.
        if ((++m_tick & 4095) == 0)
        {
            pump();
        }
        if (m_mode == Mode::Run && m_breakpoints.empty())
        {
            return;
        }
        Where here;
        here.context = context;
        here.depth = context->GetCallstackSize();
        here.line = context->GetLineNumber(0, nullptr, &here.section);
        here.run = m_host->runs();
        // A line with several statements is stopped at once; so is one that is come back to in a loop.
        const bool moved = here.line != m_last.line || here.section != m_last.section || here.depth != m_last.depth || here.context != m_last.context;
        m_last = here;
        const bool fresh = here.line != m_from.line || here.section != m_from.section || here.depth != m_from.depth || here.context != m_from.context || here.run != m_from.run;
        // Past the end of what was stopped in, into the next call the engine makes.
        const bool later = here.context != m_from.context || here.run != m_from.run;
        switch (m_mode)
        {
        case Mode::Pause:
            stopHere(context, "pause", nullptr);
            return;
        case Mode::StepIn:
            if (fresh)
            {
                stopHere(context, "step", nullptr);
                return;
            }
            break;
        case Mode::StepOver:
            if (fresh && (later || here.depth <= m_from.depth))
            {
                stopHere(context, "step", nullptr);
                return;
            }
            break;
        case Mode::StepOut:
            if (here.depth < m_from.depth || (later && here.depth <= m_from.depth))
            {
                stopHere(context, "step", nullptr);
                return;
            }
            break;
        case Mode::Run:
            break;
        }
        if (moved && !m_breakpoints.empty())
        {
            const int file = fileOf(here.section);
            if (file >= 0)
            {
                for (int at : m_breakpoints[file].lines)
                {
                    if (at == here.line)
                    {
                        stopHere(context, "breakpoint", nullptr);
                        return;
                    }
                }
            }
        }
    }

    void ScriptDebugger::exception(asIScriptContext* context)
    {
        if (!m_paused && m_client.state() == TcpState::Open)
        {
            stopHere(context, "exception", context->GetExceptionString());
        }
    }

    void ScriptDebugger::stopHere(asIScriptContext* context, const char* reason, const char* text)
    {
        m_paused = true;
        m_resume = false;
        m_stopped = context;
        forget();
        m_from.context = context;
        m_from.depth = context->GetCallstackSize();
        m_from.line = context->GetLineNumber(0, nullptr, &m_from.section);
        m_from.run = m_host->runs();
        JsonValue event;
        putText(event, "event", "stopped");
        putText(event, "reason", reason);
        if (text)
        {
            putText(event, "text", text);
        }
        send(event.write(true));
        while (!m_resume && m_host)
        {
            pump();
            if (m_idle)
            {
                m_idle(m_idleUser);
            }
            sleepMilliseconds(5);
        }
        m_paused = false;
        m_stopped = nullptr;
        forget();
    }

    void ScriptDebugger::forget()
    {
        m_refs.clear();
        for (asIScriptContext* context : m_temporaries)
        {
            context->Release();
        }
        m_temporaries.clear();
    }

    bool ScriptDebugger::callGetter(void* object, asIScriptFunction* getter, Ref& out)
    {
        asIScriptContext* context = m_host->engine()->CreateContext();
        if (context == nullptr)
        {
            return false;
        }
        if (context->Prepare(getter) < 0 || context->SetObject(object) < 0 || context->Execute() != asEXECUTION_FINISHED)
        {
            context->Release();
            return false;
        }
        asDWORD flags = 0;
        const int typeId = getter->GetReturnTypeId(&flags);
        void* at = (flags & asTM_INOUTREF) ? context->GetReturnAddress() : context->GetAddressOfReturnValue();
        m_temporaries.push_back(context);
        out = { at, typeId };
        return true;
    }

    void ScriptDebugger::resume(Mode mode)
    {
        m_mode = mode;
        m_resume = true;
        m_last = Where();
        JsonValue event;
        putText(event, "event", "continued");
        send(event.write(true));
    }

    void ScriptDebugger::stack(int id)
    {
        JsonValue reply;
        putNumber(reply, "id", id);
        JsonValue* frames = reply.put("frames");
        frames->setArray();
        for (u32 i = 0; m_stopped && i < m_stopped->GetCallstackSize(); i++)
        {
            asIScriptFunction* function = m_stopped->GetFunction(i);
            if (function == nullptr)
            {
                continue;  // where one call waits for another the engine made
            }
            const char* section = nullptr;
            int column = 0;
            const int line = m_stopped->GetLineNumber(i, &column, &section);
            JsonValue* frame = frames->add();
            putNumber(*frame, "frame", i);
            putText(*frame, "name", function->GetDeclaration(true, true, false));
            putText(*frame, "file", m_host->sourceOf(section));
            putNumber(*frame, "line", line);
            putNumber(*frame, "column", column);
        }
        send(reply.write(true));
    }

    bool ScriptDebugger::describe(JsonValue& out, const char* name, int typeId, void* address)
    {
        asIScriptEngine* engine = m_host->engine();
        putText(out, "name", name);
        asITypeInfo* type = engine->GetTypeInfoById(typeId);
        const char* declaration = engine->GetTypeDeclaration(typeId, true);
        putText(out, "type", declaration ? declaration : "?");
        std::string value;
        bool expandable = false;
        if (address == nullptr)
        {
            value = "(not made yet)";
        }
        else if (typeId & asTYPEID_OBJHANDLE)
        {
            void* object = *static_cast<void**>(address);
            if (object == nullptr)
            {
                value = "null";
            }
            else
            {
                return describe(out, name, typeId & ~asTYPEID_OBJHANDLE, object);
            }
        }
        else
        {
            switch (typeId)
            {
            case asTYPEID_BOOL: value = *static_cast<bool*>(address) ? "true" : "false"; break;
            case asTYPEID_INT8: value = formatNumber(*static_cast<int8_t*>(address)); break;
            case asTYPEID_INT16: value = formatNumber(*static_cast<i16*>(address)); break;
            case asTYPEID_INT32: value = formatNumber(*static_cast<i32*>(address)); break;
            case asTYPEID_INT64: value = formatNumber(static_cast<double>(*static_cast<i64*>(address))); break;
            case asTYPEID_UINT8: value = formatNumber(*static_cast<u8*>(address)); break;
            case asTYPEID_UINT16: value = formatNumber(*static_cast<u16*>(address)); break;
            case asTYPEID_UINT32: value = formatNumber(*static_cast<u32*>(address)); break;
            case asTYPEID_UINT64: value = formatNumber(static_cast<double>(*static_cast<u64*>(address))); break;
            case asTYPEID_FLOAT: value = formatNumber(*static_cast<float*>(address)); break;
            case asTYPEID_DOUBLE: value = formatNumber(*static_cast<double*>(address)); break;
            default:
                if (typeId == m_stringType)
                {
                    const std::string& text = *static_cast<std::string*>(address);
                    value = "\"" + (text.size() > kLongestText ? text.substr(0, kLongestText) + "..." : text) + "\"";
                }
                else if (type && (type->GetFlags() & asOBJ_ENUM))
                {
                    const i32 number = *static_cast<i32*>(address);
                    value = formatNumber(number);
                    for (asUINT i = 0; i < type->GetEnumValueCount(); i++)
                    {
                        asINT64 v = 0;
                        const char* label = type->GetEnumValueByIndex(i, &v);
                        if (v == number)
                        {
                            value = std::string(type->GetName()) + "::" + label;
                            break;
                        }
                    }
                }
                else if (type && strcmp(type->GetName(), "array") == 0)
                {
                    const CScriptArray* array = static_cast<const CScriptArray*>(address);
                    value = std::string(declaration ? declaration : "array") + " (" + formatNumber(array->GetSize()) + ")";
                    expandable = array->GetSize() > 0;
                }
                else if (type && m_summaries > 0)
                {
                    // Inside a summary, only what it is: objects may hold each other.
                    value = type->GetName();
                    expandable = true;
                }
                else if (type)
                {
                    // A few of what it holds, as a summary: { x: 1, y: 2 }. Nothing in it is kept to be opened.
                    Ref self { address, typeId };
                    JsonValue parts;
                    const size_t refs = m_refs.size();
                    m_summaries++;
                    children(parts, self);
                    m_summaries--;
                    m_refs.resize(refs);
                    value = "{";
                    for (u32 i = 0; i < parts.count() && i < 4; i++)
                    {
                        const JsonValue* part = parts.at(i);
                        value += (i ? ", " : " ") + part->find("name")->text() + ": " + part->find("value")->text();
                    }
                    value += parts.count() > 4 ? ", ... }" : parts.count() ? " }" : "}";
                    if (parts.count() == 0)
                    {
                        value = type->GetName();
                    }
                    expandable = parts.count() > 0;
                }
                else
                {
                    value = "?";
                }
            }
        }
        putText(out, "value", value);
        if (expandable)
        {
            m_refs.push_back({ address, typeId });
            putNumber(out, "ref", static_cast<double>(m_refs.size()));
        }
        return true;
    }

    void ScriptDebugger::children(JsonValue& out, const Ref& of)
    {
        out.setArray();
        asIScriptEngine* engine = m_host->engine();
        asITypeInfo* type = engine->GetTypeInfoById(of.typeId);
        if (type == nullptr || of.address == nullptr)
        {
            return;
        }
        if (strcmp(type->GetName(), "array") == 0)
        {
            CScriptArray* array = static_cast<CScriptArray*>(of.address);
            for (asUINT i = 0; i < array->GetSize() && i < kMostChildren; i++)
            {
                char label[16];
                snprintf(label, sizeof(label), "[%u]", static_cast<unsigned>(i));
                describe(*out.add(), label, array->GetElementTypeId(), array->At(i));
            }
        }
        else if (of.typeId & asTYPEID_SCRIPTOBJECT)
        {
            asIScriptObject* object = static_cast<asIScriptObject*>(of.address);
            for (asUINT i = 0; i < object->GetPropertyCount() && i < kMostChildren; i++)
            {
                describe(*out.add(), object->GetPropertyName(i), object->GetPropertyTypeId(i), object->GetAddressOfProperty(i));
            }
        }
        else
        {
            for (asUINT i = 0; i < type->GetPropertyCount() && i < kMostChildren; i++)
            {
                const char* label = nullptr;
                int typeId = 0, offset = 0;
                bool isReference = false;
                type->GetProperty(i, &label, &typeId, nullptr, nullptr, &offset, &isReference);
                void* at = static_cast<char*>(of.address) + offset;
                describe(*out.add(), label, typeId, isReference ? *static_cast<void**>(at) : at);
            }
            for (asUINT i = 0; i < type->GetMethodCount() && out.count() < kMostChildren; i++)
            {
                asIScriptFunction* method = type->GetMethodByIndex(i);
                Ref got {};
                if (isGetter(method) && callGetter(of.address, method, got))
                {
                    describe(*out.add(), method->GetName() + 4, got.typeId, got.address);
                }
            }
        }
    }

    void ScriptDebugger::variables(int id, const JsonValue& ask)
    {
        JsonValue reply;
        putNumber(reply, "id", id);
        JsonValue* list = reply.put("variables");
        list->setArray();
        const JsonValue* ref = ask.find("ref");
        const JsonValue* scope = ask.find("scope");
        const JsonValue* frameValue = ask.find("frame");
        const u32 frame = frameValue ? static_cast<u32>(frameValue->number()) : 0;
        if (m_stopped == nullptr)
        {
        }
        else if (ref)
        {
            const size_t at = static_cast<size_t>(ref->number());
            if (at >= 1 && at <= m_refs.size())
            {
                const Ref of = m_refs[at - 1];
                children(*list, of);
            }
        }
        else if (frame < m_stopped->GetCallstackSize() && scope && scope->text() == "globals")
        {
            asIScriptFunction* function = m_stopped->GetFunction(frame);
            asIScriptModule* module = function ? function->GetModule() : nullptr;
            for (asUINT i = 0; module && i < module->GetGlobalVarCount(); i++)
            {
                const char* name = nullptr;
                const char* space = nullptr;
                int typeId = 0;
                module->GetGlobalVar(i, &name, &space, &typeId);
                const std::string full = space && *space ? std::string(space) + "::" + name : std::string(name);
                describe(*list->add(), full.c_str(), typeId, module->GetAddressOfGlobalVar(i));
            }
        }
        else if (frame < m_stopped->GetCallstackSize())
        {
            if (void* self = m_stopped->GetThisPointer(frame))
            {
                describe(*list->add(), "this", m_stopped->GetThisTypeId(frame), self);
            }
            const int count = m_stopped->GetVarCount(frame);
            for (int i = 0; i < count; i++)
            {
                const char* name = nullptr;
                int typeId = 0;
                if (!m_stopped->IsVarInScope(i, frame) || m_stopped->GetVar(i, frame, &name, &typeId) < 0 || name == nullptr || !*name)
                {
                    continue;
                }
                describe(*list->add(), name, typeId, m_stopped->GetAddressOfVar(i, frame));
            }
        }
        send(reply.write(true));
    }

    bool ScriptDebugger::child(const Ref& of, const std::string& name, Ref& out)
    {
        asIScriptEngine* engine = m_host->engine();
        Ref from = of;
        if (from.typeId & asTYPEID_OBJHANDLE)
        {
            from.address = from.address ? *static_cast<void**>(from.address) : nullptr;
            from.typeId &= ~asTYPEID_OBJHANDLE;
        }
        asITypeInfo* type = engine->GetTypeInfoById(from.typeId);
        if (type == nullptr || from.address == nullptr)
        {
            return false;
        }
        if (strcmp(type->GetName(), "array") == 0)
        {
            CScriptArray* array = static_cast<CScriptArray*>(from.address);
            char* end = nullptr;
            const unsigned long index = strtoul(name.c_str(), &end, 10);
            if (end == name.c_str() || *end || index >= array->GetSize())
            {
                return false;
            }
            out = { array->At(static_cast<asUINT>(index)), array->GetElementTypeId() };
            return true;
        }
        if (from.typeId & asTYPEID_SCRIPTOBJECT)
        {
            asIScriptObject* object = static_cast<asIScriptObject*>(from.address);
            for (asUINT i = 0; i < object->GetPropertyCount(); i++)
            {
                if (name == object->GetPropertyName(i))
                {
                    out = { object->GetAddressOfProperty(i), object->GetPropertyTypeId(i) };
                    return true;
                }
            }
            return false;
        }
        for (asUINT i = 0; i < type->GetPropertyCount(); i++)
        {
            const char* label = nullptr;
            int typeId = 0, offset = 0;
            bool isReference = false;
            type->GetProperty(i, &label, &typeId, nullptr, nullptr, &offset, &isReference);
            if (name == label)
            {
                void* at = static_cast<char*>(from.address) + offset;
                out = { isReference ? *static_cast<void**>(at) : at, typeId };
                return true;
            }
        }
        const std::string getter = "get_" + name;
        for (asUINT i = 0; i < type->GetMethodCount(); i++)
        {
            asIScriptFunction* method = type->GetMethodByIndex(i);
            if (isGetter(method) && getter == method->GetName())
            {
                return callGetter(from.address, method, out);
            }
        }
        return false;
    }

    bool ScriptDebugger::lookUp(u32 frame, const std::string& name, Ref& out)
    {
        // What is nearest first: a local, then the object's own, then the module's.
        const int count = m_stopped->GetVarCount(frame);
        for (int i = count - 1; i >= 0; i--)
        {
            const char* label = nullptr;
            int typeId = 0;
            if (m_stopped->IsVarInScope(i, frame) && m_stopped->GetVar(i, frame, &label, &typeId) >= 0 && label && name == label)
            {
                out = { m_stopped->GetAddressOfVar(i, frame), typeId };
                return true;
            }
        }
        if (void* self = m_stopped->GetThisPointer(frame))
        {
            const Ref me { self, m_stopped->GetThisTypeId(frame) };
            if (name == "this")
            {
                out = me;
                return true;
            }
            if (child(me, name, out))
            {
                return true;
            }
        }
        asIScriptFunction* function = m_stopped->GetFunction(frame);
        asIScriptModule* module = function ? function->GetModule() : nullptr;
        for (asUINT i = 0; module && i < module->GetGlobalVarCount(); i++)
        {
            const char* label = nullptr;
            int typeId = 0;
            module->GetGlobalVar(i, &label, nullptr, &typeId);
            if (label && name == label)
            {
                out = { module->GetAddressOfGlobalVar(i), typeId };
                return true;
            }
        }
        return false;
    }

    void ScriptDebugger::evaluate(int id, const JsonValue& ask)
    {
        JsonValue reply;
        putNumber(reply, "id", id);
        const JsonValue* expr = ask.find("expr");
        const JsonValue* frameValue = ask.find("frame");
        const u32 frame = frameValue ? static_cast<u32>(frameValue->number()) : 0;
        // Names joined by dots, and [n] into arrays: no more than that is worked out.
        std::string text = expr ? expr->text() : std::string();
        std::vector<std::string> names;
        std::string part;
        for (char c : text)
        {
            if (c == '.' || c == '[' || c == ']')
            {
                if (!part.empty())
                {
                    names.push_back(part);
                }
                part.clear();
            }
            else if (c != ' ')
            {
                part += c;
            }
        }
        if (!part.empty())
        {
            names.push_back(part);
        }
        Ref at {};
        bool found = m_stopped && frame < m_stopped->GetCallstackSize() && !names.empty() && lookUp(frame, names[0], at);
        for (size_t i = 1; found && i < names.size(); i++)
        {
            Ref next {};
            found = child(at, names[i], next);
            at = next;
        }
        if (found)
        {
            describe(*reply.put("result"), text.c_str(), at.typeId, at.address);
        }
        else
        {
            putText(reply, "error", "not known here");
        }
        send(reply.write(true));
    }
}
