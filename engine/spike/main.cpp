#include "base/endian.h"
#include "base/log.h"
#include "base/math.h"
#include "base/memory.h"
#include "gpu/gpu.h"
#include "platform/platform.h"
#include "script/bind.h"
#include "script/script_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Phase 0: proves one script-driven triangle through the GPU interface on every target, checks the
// script calling convention, and measures memory.
//
//   --frames N      stop after N frames (fixed time step, so captures are repeatable)
//   --capture FILE  save the last frame as a BMP
//   --hidden        don't show the window
//   --null          use the null GPU
namespace
{
    using namespace nx;

    struct App
    {
        gpu::Color clear;
        Vec2 offset;
        float angle = 0;
        float scale = 1;
    };

    App g_app;

    void setClear(float r, float g, float b) { g_app.clear = { r, g, b, 1 }; }
    void setTriangle(const Vec2& offset, float angle, float scale)
    {
        g_app.offset = offset;
        g_app.angle = angle;
        g_app.scale = scale;
    }

    bool bindApp(ScriptHost& host)
    {
        asIScriptEngine* e = host.engine();
        e->SetDefaultNamespace("app");
        bool ok = host.check(e->RegisterGlobalFunction("void setClear(float, float, float)", NX_FN(setClear)), "setClear");
        ok = host.check(e->RegisterGlobalFunction("void setTriangle(const Vec2 &in, float, float)", NX_FN(setTriangle)), "setTriangle") && ok;
        e->SetDefaultNamespace("");
        return ok;
    }

    void logMemory(Platform& platform, const char* when)
    {
        const MemStats total = memStatsTotal();
        NX_INFO("memory %s: engine %u KB in %u allocations (script %u KB, peak %u KB); machine free %u of %u KB", when,
            static_cast<unsigned>(total.bytes / 1024), static_cast<unsigned>(total.count),
            static_cast<unsigned>(memStats(MemTag::Script).bytes / 1024), static_cast<unsigned>(memStats(MemTag::Script).peak / 1024),
            static_cast<unsigned>(platform.freeMemory() / 1024), static_cast<unsigned>(platform.totalMemory() / 1024));
    }

    // A 32-bit BMP, stored top row first.
    bool writeBmp(const char* path, const u8* pixels, u32 width, u32 height)
    {
        u8 header[54] = { 'B', 'M' };
        const u32 bytes = width * height * 4;
        writeU32(header + 2, static_cast<u32>(sizeof(header)) + bytes);
        writeU32(header + 10, sizeof(header));
        writeU32(header + 14, 40);
        writeU32(header + 18, width);
        writeU32(header + 22, 0u - height);
        writeU16(header + 26, 1);
        writeU16(header + 28, 32);
        writeU32(header + 34, bytes);
        FILE* file = fopen(path, "wb");
        if (file == nullptr)
        {
            return false;
        }
        bool ok = fwrite(header, sizeof(header), 1, file) == 1;
        for (u32 i = 0; ok && i < width * height; i++)
        {
            const u8* p = pixels + static_cast<size_t>(i) * 4;
            const u8 bgra[4] = { p[2], p[1], p[0], p[3] };
            ok = fwrite(bgra, 4, 1, file) == 1;
        }
        fclose(file);
        return ok;
    }

    bool saveCapture(gpu::Device& device, const char* path)
    {
        u32 w, h;
        device.size(w, h);
        const size_t bytes = static_cast<size_t>(w) * h * 4;
        u8* pixels = static_cast<u8*>(memAlloc(bytes, MemTag::General));
        bool ok = pixels && device.capture(pixels, bytes);
        if (ok && path)
        {
            ok = writeBmp(path, pixels, w, h);
        }
        if (ok)
        {
            // The centre is inside the triangle and the corner is the clear colour.
            const u8* centre = pixels + (static_cast<size_t>(h / 2) * w + w / 2) * 4;
            NX_INFO("capture %ux%u: corner %u,%u,%u centre %u,%u,%u", static_cast<unsigned>(w), static_cast<unsigned>(h), pixels[0], pixels[1], pixels[2], centre[0], centre[1], centre[2]);
        }
        memFree(pixels);
        return ok;
    }

    struct Options
    {
        const char* capture = nullptr;
        int frames = 0;
        bool hidden = false;
        bool null = false;
    };

    bool runApp(Platform& platform, const Options& options)
    {
        logMemory(platform, "at start");

        WindowDesc window;
        window.title = "Nexgen phase 0";
        window.hidden = options.hidden;
        if (!options.null && !platform.openWindow(window))
        {
            return false;
        }

        gpu::Device* device = options.null ? gpu::createNullDevice(640, 480) : gpu::createDevice(platform.nativeWindow());
        if (device == nullptr)
        {
            return false;
        }
        u32 width, height;
        device->size(width, height);
        NX_INFO("platform %s, gpu %s %ux%u", platform.name(), device->name(), static_cast<unsigned>(width), static_cast<unsigned>(height));
        logMemory(platform, "with window and gpu");

        bool ok = false;
        {
            ScriptHost host;
            FileData source;
            int checks = 0;
            if (host.init() && bindApp(host) && platform.readFile("spike.as", source, MemTag::Script))
            {
                NX_INFO("AngelScript %s, %s calls", ScriptHost::version(), ScriptHost::usesNativeCalls() ? "native" : "generic");
                logMemory(platform, "with script engine");

                const int failures = runScriptAbiTests(host, checks);
                NX_INFO("script ABI: %d checks, %d failures", checks, failures);

                host.setStatementLimit(100000);
                const bool compiled = host.compile("spike", "spike.as", reinterpret_cast<const char*>(source.data), source.size);
                const bool loopStopped = compiled && !host.call(host.find("spike", "void endless()"));
                const bool errorCaught = compiled && !host.call(host.find("spike", "void divideByZero()"));
                NX_INFO("endless loop stopped: %s; script error caught: %s", loopStopped ? "yes" : "NO", errorCaught ? "yes" : "NO");

                asIScriptFunction* update = host.find("spike", "void update(float)");
                ok = failures == 0 && loopStopped && errorCaught && update && host.call(host.find("spike", "void init()"));

                const gpu::Vertex vertices[] = {
                    { -0.6f, -0.5f, 0.5f, 255, 0, 0, 255, 0, 0 },
                    { 0.6f, -0.5f, 0.5f, 0, 255, 0, 255, 1, 0 },
                    { 0.0f, 0.7f, 0.5f, 0, 0, 255, 255, 0.5f, 1 },
                };
                const gpu::Buffer buffer = device->createVertexBuffer(vertices, 3);
                gpu::PipelineDesc desc;
                desc.blend = gpu::Blend::Alpha;
                desc.depthTest = true;
                desc.depthWrite = true;
                const gpu::Pipeline pipeline = device->createPipeline(desc);

                double last = platform.seconds();
                const double start = last;
                int frame = 0;
                size_t allocations = 0;
                while (ok && platform.pumpEvents() && (options.frames == 0 || frame < options.frames))
                {
                    const double now = platform.seconds();
                    const float dt = options.frames ? 1.0f / 60.0f : static_cast<float>(now - last);
                    last = now;

                    ok = host.call(update, dt);
                    host.collectStep();
                    if (frame == 10)
                    {
                        allocations = memStatsTotal().totalCount;
                    }
                    if (device->beginFrame(g_app.clear))
                    {
                        device->size(width, height);
                        const float aspect = static_cast<float>(width) / static_cast<float>(height);
                        gpu::Draw draw;
                        draw.pipeline = pipeline;
                        draw.vertices = buffer;
                        draw.count = 3;
                        draw.transform = Mat4::ortho(-aspect, aspect, -1, 1, 0, 1) * Mat4::translation({ g_app.offset.x, g_app.offset.y, 0 }) * Mat4::rotationZ(g_app.angle) * Mat4::scale({ g_app.scale, g_app.scale, 1 });
                        device->draw(draw);
                        device->endFrame();
                    }
                    frame++;

                    if (frame == 120)
                    {
                        const gpu::Stats stats = device->stats();
                        NX_INFO("after %d frames: %u draws, %u rejected, %.1f fps, %u engine allocations since frame 10", frame,
                            static_cast<unsigned>(stats.draws), static_cast<unsigned>(stats.rejected), frame / (platform.seconds() - start),
                            static_cast<unsigned>(memStatsTotal().totalCount - allocations));
                        logMemory(platform, "running");
#if defined(NX_GPU_D3D8)
                        ok = saveCapture(*device, nullptr) && ok;
#endif
                        NX_INFO("%s", ok ? "PHASE0 PASS" : "PHASE0 FAIL");
                    }
                }
                if (options.capture)
                {
                    ok = saveCapture(*device, options.capture) && ok;
                }
                device->destroyBuffer(buffer);
                device->destroyPipeline(pipeline);
            }
        }
        destroy(device);
        return ok;
    }
}

#if defined(NX_GPU_D3D8)
int main()
{
    const int argc = 0;  // a title has no command line
    char** argv = nullptr;
#else
int main(int argc, char** argv)
{
#endif
    Options options;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
        {
            options.frames = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc)
        {
            options.capture = argv[++i];
        }
        else if (strcmp(argv[i], "--hidden") == 0)
        {
            options.hidden = true;
        }
        else if (strcmp(argv[i], "--null") == 0)
        {
            options.null = true;
        }
    }

    nx::Platform* platform = nx::createPlatform();
    bool ok = platform && runApp(*platform, options);
    nx::destroy(platform);

    const nx::MemStats left = nx::memStatsTotal();
    NX_INFO("at exit: %u bytes in %u allocations still held", static_cast<unsigned>(left.bytes), static_cast<unsigned>(left.count));
    ok = ok && left.count == 0;
    NX_INFO("%s", ok ? "EXIT OK" : "EXIT FAILED");
    return ok ? 0 : 1;
}
