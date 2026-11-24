#include "test.h"

#include "base/memory.h"
#include "script/script_host.h"

#include <string.h>

using namespace nx;

namespace
{
    bool compile(ScriptHost& host, const char* code)
    {
        return host.compile("test", "test.as", code, strlen(code));
    }
}

NX_TEST(scriptCallingConvention)
{
    ScriptHost host;
    NX_EXPECT(host.init());
    int checks = 0;
    NX_EXPECT(runScriptAbiTests(host, checks) == 0);
    NX_EXPECT(checks > 25);
    NX_INFO("     AngelScript %s, %s calls, %d checks", ScriptHost::version(), ScriptHost::usesNativeCalls() ? "native" : "generic", checks);
}

NX_TEST(scriptErrorsDontStopTheHost)
{
    ScriptHost host;
    NX_EXPECT(host.init());
    host.setStatementLimit(10000);
    NX_EXPECT(!compile(host, "void broken( {"));
    NX_EXPECT(compile(host,
        "int zero = 0;\n"
        "void divide() {\n int x = 1 / zero;\n}\n"
        "void forever() { while (true) {} }\n"
        "void recurse() { recurse(); }\n"
        "int fine() { return 42; }"));
    NX_EXPECT(!host.call(host.find("test", "void divide()")));
    NX_EXPECT(strstr(host.lastError(), "test.as:3") != nullptr);
    NX_EXPECT(!host.call(host.find("test", "void forever()")));
    NX_EXPECT(strstr(host.lastError(), "endless loop") != nullptr);
    NX_EXPECT(!host.call(host.find("test", "void recurse()")));
    NX_EXPECT(!host.call(host.find("test", "void missing()")));
    int value = 0;
    NX_EXPECT(host.callInt(host.find("test", "int fine()"), value) && value == 42);
}

NX_TEST(scriptMemoryIsReturned)
{
    const size_t before = memStats(MemTag::Script).bytes;
    {
        ScriptHost host;
        NX_EXPECT(host.init());
        size_t settled = 0;
        for (int i = 0; i < 100; i++)
        {
            // A reference cycle, which only the garbage collector can free.
            NX_EXPECT(compile(host, "class Node { Node@ other; array<string> text(50); }\n void run() { Node a; Node b; @a.other = b; @b.other = a; }"));
            NX_EXPECT(host.call(host.find("test", "void run()")));
            host.discard("test");
            host.collectAll();
            if (i == 10)
            {
                settled = memStats(MemTag::Script).bytes;
            }
        }
        NX_EXPECT(memStats(MemTag::Script).bytes <= settled);
    }
    NX_EXPECT(memStats(MemTag::Script).bytes == before);
}

NX_TEST(scriptEngineFitsConsoleBudget)
{
    ScriptHost host;
    NX_EXPECT(host.init());
    NX_EXPECT(memStats(MemTag::Script).bytes > 0);
    NX_EXPECT(memStats(MemTag::Script).bytes < 4 * 1024 * 1024);  // the original Xbox script budget
}

NX_TEST(garbageIsCollectedAsItIsMade)
{
    ScriptHost host;
    NX_EXPECT(host.init());
    NX_EXPECT(compile(host, "class A { A@ other; array<int> data(100); } void main() { for (int i = 0; i < 20; i++) { A a, b; @a.other = b; @b.other = a; } }"));
    asIScriptFunction* function = host.find("test", "void main()");
    size_t settled = 0;
    for (int i = 0; i < 2000; i++)
    {
        NX_EXPECT(host.call(function));
        host.collectStep();
        settled = i == 500 ? memStats(MemTag::Script).bytes : settled;
    }
    NX_EXPECT(memStats(MemTag::Script).bytes < settled * 3 / 2);
    host.shutdown();
}
