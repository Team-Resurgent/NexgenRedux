#include "test.h"

namespace nx::test
{
    namespace
    {
        Case* g_first = nullptr;
        int g_checks = 0;
        int g_failures = 0;
    }

    Case::Case(const char* name_, Function function_) : name(name_), function(function_), next(g_first)
    {
        g_first = this;
    }

    void expect(bool ok, const char* expression, const char* file, int line)
    {
        g_checks++;
        if (!ok)
        {
            g_failures++;
            NX_ERROR("failed: %s (%s:%d)", expression, file, line);
        }
    }

    int runAll()
    {
        int cases = 0;
        for (Case* c = g_first; c; c = c->next)
        {
            const int before = g_failures;
            c->function();
            cases++;
            NX_INFO("%s %s", g_failures == before ? "pass" : "FAIL", c->name);
        }
        NX_INFO("%d tests, %d checks, %d failures", cases, g_checks, g_failures);
        return g_failures;
    }
}

int main()
{
    return nx::test::runAll() == 0 ? 0 : 1;
}
