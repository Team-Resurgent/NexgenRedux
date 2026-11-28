#include "base/log.h"
#include "base/thread.h"
#include "bindings/script_refs.h"
#include "script/binder.h"
#include "script/script_host.h"

#include <angelscript.h>
#include <atomic>
#include <string.h>
#include <string>
#include <vector>

// Scripts of their own on threads of their own, and channels between them. A thread's script has an engine of its own,
// with strings, arrays, maths, Json, Data and Random, and nothing that draws, plays or loads: it shares nothing with
// the screen's script but what goes through a channel, as text.
namespace nx
{
    namespace
    {
        // --- Channels ---

        struct Channel
        {
            std::atomic<int> refs{ 1 };
            std::string name;
            Mutex lock;
            std::vector<std::string> items;   // from the front
            size_t first = 0;

            void addRef() { refs++; }
            void release();

            size_t count()
            {
                Lock held(lock);
                return items.size() - first;
            }
        };

        constexpr size_t kMostWaiting = 100000;

        Mutex& registryLock()
        {
            static Mutex lock;
            return lock;
        }

        std::vector<Channel*>& registry()
        {
            static std::vector<Channel*> channels;
            return channels;
        }

        void Channel::release()
        {
            // Taken out of the names under the same lock that finds it, so nobody finds one that is going.
            Lock held(registryLock());
            if (--refs > 0)
            {
                return;
            }
            std::vector<Channel*>& all = registry();
            for (size_t i = 0; i < all.size(); i++)
            {
                if (all[i] == this)
                {
                    all.erase(all.begin() + static_cast<long>(i));
                    break;
                }
            }
            destroy(this);
        }

        Channel* namedChannel(const std::string& name)
        {
            Lock held(registryLock());
            for (Channel* c : registry())
            {
                if (c->name == name)
                {
                    c->refs++;
                    return c;
                }
            }
            Channel* c = create<Channel>(MemTag::Script);
            if (c)
            {
                c->name = name;
                registry().push_back(c);
            }
            return c;
        }

        void channelPush(const std::string& value, Channel* c)
        {
            Lock held(c->lock);
            if (c->items.size() - c->first >= kMostWaiting)
            {
                scriptError("a channel holds up to 100000 waiting: are they being taken?");
                return;
            }
            c->items.push_back(value);
        }

        bool takeFirst(Channel* c, std::string& out)
        {
            Lock held(c->lock);
            if (c->first >= c->items.size())
            {
                return false;
            }
            out.swap(c->items[c->first++]);
            // Taken from the front without moving the rest each time: tidied once half is taken.
            if (c->first > 64 && c->first * 2 > c->items.size())
            {
                c->items.erase(c->items.begin(), c->items.begin() + static_cast<long>(c->first));
                c->first = 0;
            }
            return true;
        }

        bool channelPop(std::string& out, Channel* c)
        {
            out.clear();
            return takeFirst(c, out);
        }

        std::string channelPopText(Channel* c)
        {
            std::string out;
            takeFirst(c, out);
            return out;
        }

        std::string channelPeek(Channel* c)
        {
            Lock held(c->lock);
            return c->first < c->items.size() ? c->items[c->first] : std::string();
        }

        u32 channelCount(Channel* c) { return static_cast<u32>(c->count()); }
        std::string channelName(const Channel* c) { return c->name; }

        void channelClear(Channel* c)
        {
            Lock held(c->lock);
            c->items.clear();
            c->first = 0;
        }

        // --- Threads ---

        struct ScriptThread
        {
            std::atomic<int> refs{ 1 };
            ScriptHost host;
            asIScriptFunction* main = nullptr;
            ThreadHandle handle;
            std::atomic<bool> stop{ false };
            std::atomic<bool> running{ false };
            std::atomic<bool> started{ false };
            Mutex errorLock;
            std::string error;
            std::string file;
            double began = 0;

            void addRef() { refs++; }
            void release();
            void finish()
            {
                stop.store(true);
                join();
            }
            void join()
            {
                if (handle)
                {
                    joinThread(handle);
                    handle = {};
                }
            }
        };

        std::atomic<int> g_threads{ 0 };
        constexpr int kMostThreads = 8;

        void ScriptThread::release()
        {
            if (--refs == 0)
            {
                // Asked to stop, and waited for: what it is doing ends at its next statement, or its next wait.
                finish();
                host.shutdown();
                g_threads--;
                destroy(this);
            }
        }

        // Whose thread the running script is, or nullptr for the screen's.
        ScriptThread* currentThread()
        {
            asIScriptContext* context = asGetActiveContext();
            return context ? static_cast<ScriptThread*>(context->GetEngine()->GetUserData(1)) : nullptr;
        }

        void runThread(void* user)
        {
            ScriptThread* t = static_cast<ScriptThread*>(user);
            const bool fine = t->host.call(t->main);
            if (!fine && !t->stop.load())
            {
                Lock held(t->errorLock);
                t->error = t->host.lastError();
                NX_ERROR("thread %s: %s", t->file.c_str(), t->error.c_str());
            }
            t->running.store(false);
            asThreadCleanup();
        }

        bool bindForThread(ScriptHost& host);

        ScriptThread* makeThread(const std::string& name)
        {
            Services& s = services();
            if (g_threads.load() >= kMostThreads)
            {
                scriptError("up to 8 threads at once");
                return nullptr;
            }
            FileData file;
            if (!readOwn(s, name.c_str(), file))
            {
                scriptError("there is no script " + name);
                return nullptr;
            }
            ScriptThread* t = create<ScriptThread>(MemTag::Script);
            if (t == nullptr)
            {
                scriptError("there isn't the memory for another thread");
                return nullptr;
            }
            g_threads++;
            t->file = name;
            if (!t->host.init() || !bindForThread(t->host))
            {
                t->release();
                scriptError("a thread's script engine couldn't be made");
                return nullptr;
            }
            t->host.engine()->SetUserData(t, 1);
            t->host.engine()->SetEngineProperty(asEP_AUTO_GARBAGE_COLLECT, true);
            t->host.setStop(&t->stop);
            if (!t->host.compile("thread", name.c_str(), reinterpret_cast<const char*>(file.data), file.size))
            {
                const std::string why = std::string(t->host.lastError()[0] ? t->host.lastError() : "it doesn't compile: the log says where");
                t->release();
                scriptError(name + ": " + why);
                return nullptr;
            }
            t->main = t->host.find("thread", "void main()");
            if (t->main == nullptr)
            {
                t->release();
                scriptError(name + " has no void main()");
                return nullptr;
            }
            return t;
        }

        bool threadStart(ScriptThread* t)
        {
            if (t->started.load())
            {
                return t->running.load();
            }
            t->started.store(true);
            t->running.store(true);
            t->began = services().time;
            t->handle = startThread(runThread, t, ThreadPriority::Normal);
            if (!t->handle)
            {
                // Where there are no threads, it runs here, now, to its end.
                runThread(t);
            }
            return true;
        }

        bool threadRunning(const ScriptThread* t) { return t->running.load(); }
        bool threadStarted(const ScriptThread* t) { return t->started.load(); }
        std::string threadError(ScriptThread* t)
        {
            Lock held(t->errorLock);
            return t->error;
        }
        void threadStop(ScriptThread* t) { t->stop.store(true); }
        void threadWait(ScriptThread* t)
        {
            if (currentThread() == t)
            {
                scriptError("a thread can't wait for itself");
                return;
            }
            t->join();
        }

        // --- What only a thread's script has ---

        void workerSleep(float seconds)
        {
            ScriptThread* t = currentThread();
            if (t == nullptr)
            {
                scriptError("only a thread may sleep: the screen would stop");
                return;
            }
            // In short naps, so that being asked to stop is heard.
            const double wake = seconds > 0 ? seconds : 0;
            for (double slept = 0; slept < wake && !t->stop.load(); slept += 0.005)
            {
                sleepMilliseconds(5);
            }
            if (wake == 0)
            {
                sleepMilliseconds(0);
            }
        }

        bool workerStopping()
        {
            const ScriptThread* t = currentThread();
            return t && t->stop.load();
        }

        bool workerInThread() { return currentThread() != nullptr; }

        // Waits for something on a channel, for so many seconds, or for ever below 0. A thread only.
        std::string channelDemand(float seconds, Channel* c)
        {
            ScriptThread* t = currentThread();
            std::string out;
            if (takeFirst(c, out))
            {
                return out;
            }
            if (t == nullptr)
            {
                scriptError("only a thread may wait on a channel: the screen would stop; use pop()");
                return out;
            }
            for (double waited = 0; !t->stop.load() && (seconds < 0 || waited < seconds); waited += 0.001)
            {
                sleepMilliseconds(1);
                if (takeFirst(c, out))
                {
                    return out;
                }
            }
            return out;
        }

        void bindChannel(Binder& b)
        {
            b.refType("Channel", 0, "Text passed from one thread to another, first in, first out, and safe to use from any of them. Channels of the same name are the same channel, in every thread. Numbers go as text, and anything with parts as Json's text.");
            b.behaviour(asBEHAVE_FACTORY, "Channel@ f(const string &in name)", NX_FN(namedChannel));
            b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Channel, addRef));
            b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Channel, release));
            b.method("string get_name() const property", NX_OBJ_LAST(channelName), "What it is called.");
            b.method("void push(const string &in value)", NX_OBJ_LAST(channelPush), "Puts one at the end.");
            b.method("bool pop(string &out value)", NX_OBJ_LAST(channelPop), "Takes the first, if there is one: false if there isn't. Never waits.");
            b.method("string pop()", NX_OBJ_LAST(channelPopText), "Takes the first, or gives empty text if there is none.");
            b.method("string peek()", NX_OBJ_LAST(channelPeek), "The first, left where it is.");
            b.method("string demand(float seconds = -1)", NX_OBJ_LAST(channelDemand), "In a thread: waits for one, for so many seconds or for ever, and takes it; empty if none came, or the thread is asked to stop.");
            b.method("uint get_count() property", NX_OBJ_LAST(channelCount), "How many are waiting.");
            b.method("void clear()", NX_OBJ_LAST(channelClear), "Takes them all away.");
        }

        void bindWorker(Binder& b)
        {
            b.names("Worker");
            b.function("void sleep(float seconds)", NX_FN(workerSleep), "In a thread: waits so long, waking at once if it is asked to stop. 0 lets others run.");
            b.function("bool get_stopping() property", NX_FN(workerStopping), "In a thread: whether it has been asked to stop, as its screen closes. Long loops should look, and return.");
            b.function("bool get_inThread() property", NX_FN(workerInThread), "Whether the script running is a thread's.");
            b.names("");
        }

        // What a thread's own engine has, beside strings, arrays and maths.
        bool bindForThread(ScriptHost& host)
        {
            Binder b(host);
            bindColorType(b);
            bindJsonValue(b);
            bindBytes(b, true);
            bindChannel(b);
            bindWorker(b);
            return b.ok();
        }
    }

    void bindThreads(Binder& b)
    {
        b.section("Threads", "Scripts of their own, each on a thread of its own, for work that would hold up the screen: making a world, finding a way, squeezing a file. A thread's script has strings, arrays, maths, Json, Data and Random, and nothing that draws, plays or loads; it begins at its `void main()`, and shares nothing with the screen but what goes through channels. On the original Xbox, with one processor, a thread runs in the time the screen leaves.");
        bindChannel(b);
        b.refType("Thread", 0, "A script file run on a thread of its own. It is asked to stop, and waited for, when nothing holds it any more: as its screen closes.");
        b.behaviour(asBEHAVE_FACTORY, "Thread@ f(const string &in script)", NX_FN(makeThread));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(ScriptThread, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(ScriptThread, release));
        b.method("bool start()", NX_OBJ_LAST(threadStart), "Runs its main(), once.");
        b.method("bool get_started() const property", NX_OBJ_LAST(threadStarted), "Whether it has been started.");
        b.method("bool get_running() const property", NX_OBJ_LAST(threadRunning), "Whether its main() is still going.");
        b.method("string get_error() property", NX_OBJ_LAST(threadError), "What went wrong in it, with its line, or empty.");
        b.method("void stop()", NX_OBJ_LAST(threadStop), "Asks it to stop: its waits end, and Worker::stopping is true.");
        b.method("void wait()", NX_OBJ_LAST(threadWait), "Waits until its main() has returned: for ever, if it never does. stop() first to end it sooner.");
        bindWorker(b);
    }
}
