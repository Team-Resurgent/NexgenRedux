#include "base/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

namespace nx
{
    namespace
    {
        void defaultSink(LogLevel level, const char* message)
        {
            static const char* const names[] = { "info", "warn", "error" };
            printf("[%s] %s\n", names[static_cast<int>(level)], message);
            fflush(stdout);
        }

        LogSink g_sink = defaultSink;
    }

    void setLogSink(LogSink sink)
    {
        g_sink = sink ? sink : defaultSink;
    }

    LogSink logSink()
    {
        return g_sink;
    }

    void log(LogLevel level, const char* format, ...)
    {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        g_sink(level, buffer);
    }

    void fail(const char* expression, const char* file, int line)
    {
        log(LogLevel::Error, "assert failed: %s (%s:%d)", expression, file, line);
        abort();
    }
}
