#pragma once

#include "base/types.h"
#include "base/thread.h"
#include "net/socket.h"
#include "script/script_host.h"

#include <string>
#include <vector>

namespace nx
{
    class JsonValue;

    // Lets a debugger, such as the one for VS Code in tools/vscode, stop scripts where it is asked to, step
    // through them, and look at what they hold. It listens on a TCP port; what passes is JSON, a line each.
    //
    // Asked:    {"cmd":"breakpoints","file":"showcase/blocks/main.as","lines":[12,40]}   (all of that file's)
    //           {"cmd":"ready"}  {"cmd":"pause"}  {"cmd":"continue"}  {"cmd":"next"}  {"cmd":"stepIn"}  {"cmd":"stepOut"}
    //           {"cmd":"stack","id":1}  {"cmd":"variables","id":2,"frame":0,"scope":"locals"|"globals"}
    //           {"cmd":"variables","id":3,"ref":7}  {"cmd":"evaluate","id":4,"frame":0,"expr":"player.position.x"}
    // Told:     {"event":"hello",...}  {"event":"stopped","reason":"breakpoint"|"step"|"pause"|"exception","text":...}
    //           {"event":"continued"}  {"event":"output","level":"info","text":...}  and {"id":n,...} for each question.
    //
    // Files are named as the engine reads them, from its media folder: "showcase/blocks/main.as", "global/scene.as".
    class ScriptDebugger final : public ScriptWatcher, NonCopyable
    {
    public:
        ~ScriptDebugger() override { stop(); }

        // Open to other machines unless onlyHere, so that a console can be debugged from a computer.
        bool start(ScriptHost& host, u16 port, bool onlyHere);
        void stop();
        // Run while scripts are stopped, so that the window goes on answering.
        void setIdle(void (*idle)(void* user), void* user)
        {
            m_idle = idle;
            m_idleUser = user;
        }
        // Each frame: takes questions and passes the log on.
        void update();
        // Until a debugger has connected and said it is ready, so that the first lines can be stopped at too.
        void waitForReady();
        bool connected() const { return m_client.state() == TcpState::Open; }
        bool listening() const { return m_host != nullptr; }

        void line(asIScriptContext* context) override;
        void exception(asIScriptContext* context) override;
        void compiled() override { m_files.clear(); }

    private:
        enum class Mode : u8 { Run, Pause, StepIn, StepOver, StepOut };

        struct Breakpoints
        {
            std::string file;
            std::vector<int> lines;
        };

        struct Ref
        {
            void* address;
            int typeId;
        };

        struct Where
        {
            const char* section = nullptr;
            int line = 0;
            u32 depth = 0;
            asIScriptContext* context = nullptr;
            u32 run = 0;
        };

        void pump();
        void handle(const char* text, size_t size);
        void send(const std::string& line);
        void flush();
        void stopHere(asIScriptContext* context, const char* reason, const char* text);
        void resume(Mode mode);
        int fileOf(const char* section);

        void stack(int id);
        void variables(int id, const JsonValue& ask);
        void evaluate(int id, const JsonValue& ask);
        bool describe(JsonValue& out, const char* name, int typeId, void* address);
        bool child(const Ref& of, const std::string& name, Ref& out);
        bool lookUp(u32 frame, const std::string& name, Ref& out);
        void children(JsonValue& out, const Ref& of);
        bool callGetter(void* object, asIScriptFunction* getter, Ref& out);
        void forget();

        ScriptHost* m_host = nullptr;
        TcpSocket m_listener;
        TcpSocket m_client;
        std::string m_in;
        std::string m_out;
        Mutex m_logLock;
        std::string m_log;
        void (*m_idle)(void*) = nullptr;
        void* m_idleUser = nullptr;

        std::vector<Breakpoints> m_breakpoints;
        std::vector<std::pair<const char*, int>> m_files;
        Mode m_mode = Mode::Run;
        Where m_last;
        Where m_from;
        bool m_paused = false;
        bool m_ready = false;
        bool m_resume = false;
        asIScriptContext* m_stopped = nullptr;
        std::vector<Ref> m_refs;
        // What getters gave back lives in the context that called them, until the scripts go on.
        std::vector<asIScriptContext*> m_temporaries;
        u32 m_tick = 0;
        int m_stringType = 0;
        u32 m_summaries = 0;
    };
}
