#pragma once

#include "base/log.h"

// A small test runner with no dependencies, so the same tests run on a console.
namespace nx::test
{
    using Function = void (*)();

    struct Case
    {
        Case(const char* name, Function function);

        const char* name;
        Function function;
        Case* next;
    };

    void expect(bool ok, const char* expression, const char* file, int line);

    // Returns the number of failed checks.
    int runAll();
}

#define NX_TEST(id) \
    static void id(); \
    static ::nx::test::Case id##Case(#id, id); \
    static void id()

#define NX_EXPECT(expr) ::nx::test::expect((expr), #expr, __FILE__, __LINE__)
