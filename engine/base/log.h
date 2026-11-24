#pragma once

namespace nx
{
    enum class LogLevel { Info, Warning, Error };

    using LogSink = void (*)(LogLevel level, const char* message);

    // Replaces the sink (default: stdout). Set it before starting any thread.
    void setLogSink(LogSink sink);

#if defined(__clang__) || defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    void log(LogLevel level, const char* format, ...);

    [[noreturn]] void fail(const char* expression, const char* file, int line);
}

#define NX_INFO(...) ::nx::log(::nx::LogLevel::Info, __VA_ARGS__)
#define NX_WARN(...) ::nx::log(::nx::LogLevel::Warning, __VA_ARGS__)
#define NX_ERROR(...) ::nx::log(::nx::LogLevel::Error, __VA_ARGS__)

// Always on: a broken invariant stops the program rather than corrupting memory.
#define NX_ASSERT(expr) ((expr) ? (void)0 : ::nx::fail(#expr, __FILE__, __LINE__))
