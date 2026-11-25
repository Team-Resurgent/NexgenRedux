#pragma once

#include "assets/assets.h"
#include "bindings/bindings.h"
#include "canvas/canvas.h"
#include "input/input.h"
#include "net/net.h"
#include "net/secure.h"
#include "screens/screens.h"

#include <atomic>
#include <stdio.h>
#include <string.h>
#include <map>
#include <string>

namespace nx::test
{
    // Real media files, plus one made up by the test, and no window.
    class TestPlatform final : public Platform
    {
    public:
        std::atomic<int> reads{ 0 };  // files are read by the loading thread too
        u64 time = 1;
        std::string saved, nothing;
        std::map<std::string, std::string> kept;  // other saves, by name
        std::string written;                       // the last file
        double now = 0;
        std::string extraPath, extraText;
        std::map<std::string, std::string> files;   // more, by path
        InputState state;

        bool openWindow(const WindowDesc&) override { return true; }
        void* nativeWindow() override { return nullptr; }
        bool pumpEvents() override { return true; }
        const InputState& input() const override { return state; }
        double seconds() override { return now; }
        void sleep(double) override {}

        bool readFile(const char* path, FileData& out, MemTag tag) override
        {
            reads++;
            const auto other = files.find(path);
            if (other != files.end())
            {
                extraPath = path;
                extraText = other->second;
            }
            if (extraPath == path)
            {
                u8* data = static_cast<u8*>(memAlloc(extraText.size() + 1, tag));
                memcpy(data, extraText.data(), extraText.size());
                out.take(data, extraText.size());
                return true;
            }
            char full[512];
            snprintf(full, sizeof(full), "%s/%s", NX_MEDIA, path);
            FILE* file = fopen(full, "rb");
            if (file == nullptr)
            {
                return false;
            }
            fseek(file, 0, SEEK_END);
            const size_t size = static_cast<size_t>(ftell(file));
            fseek(file, 0, SEEK_SET);
            u8* data = static_cast<u8*>(memAlloc(size + 1, tag));
            const bool ok = fread(data, 1, size, file) == size;
            fclose(file);
            out.take(data, size);
            return ok;
        }

        bool takeDropped(std::string& name) override
        {
            name = dropped;
            dropped.clear();
            return !name.empty();
        }

        void list(const char* path, bool folders, FolderCallback callback, void* user) override
        {
            if (!folders && strcmp(path, "own") == 0)
            {
                for (const char* name : { "b.stl", "a.obj" })
                {
                    callback(name, user);
                }
            }
            if (!folders)
            {
                return;
            }
            if (strcmp(path, "showcase") == 0)
            {
                for (const char* name : { "main", "shapes", "blend", "faulty", "test" })
                {
                    callback(name, user);
                }
            }
        }

        std::string dropped;

        u64 fileTime(const char*) override { return time; }

        bool loadSave(const char* name, FileData& out) override
        {
            const auto other = kept.find(name);
            const std::string& from = strchr(name, '.') == nullptr ? saved : other != kept.end() ? other->second : nothing;
            if (from.empty())
            {
                return false;
            }
            u8* data = static_cast<u8*>(memAlloc(from.size() + 1, MemTag::General));
            memcpy(data, from.data(), from.size());
            out.take(data, from.size());
            return true;
        }

        bool writeSave(const char* name, const void* data, size_t size) override
        {
            (strchr(name, '.') == nullptr ? saved : kept[name]).assign(static_cast<const char*>(data), size);
            return true;
        }

        void removeSave(const char* name) override { kept.erase(name); }

        bool writeFile(const char* path, const void*, size_t) override
        {
            written = path;
            return true;
        }

        u64 totalMemory() override { return 0; }
        u64 freeMemory() override { return 0; }
        const char* name() const override { return "test"; }
    };

    inline const char kShip[] = "showcase/parallax/asset/ship.png";
    inline const char kStars[] = "showcase/parallax/asset/stars1.png";

    // Everything a script needs, on the null GPU and with no sound.
    struct Rig
    {
        TestPlatform platform;
        gpu::Device* device = gpu::createNullDevice(1280, 720);
        Assets assets;
        Canvas canvas;
        SolidMeshes solids;
        Input input;
        Audio audio;
        Save save;
        Net net;
        ScriptHost host;
        Coroutines coroutines;
        Services services;
        Screens screens;
        u32 documented = 0;
        bool ok = false;

        explicit Rig(size_t budget = 64 * 1024 * 1024)
        {
            services.platform = &platform;
            services.device = device;
            services.assets = &assets;
            services.canvas = &canvas;
            services.solids = &solids;
            services.net = &net;
            net.setOnlyHere(true);
            secureSetPlatform(&platform);
            solids.init(*device);
            services.input = &input;
            services.host = &host;
            services.audio = &audio;
            services.save = &save;
            services.coroutines = &coroutines;
            ok = audio.init(createNullAudioOutput(false));
            audio.onFinished([](void* asset, void* cache) { static_cast<Assets*>(cache)->release(static_cast<Asset*>(asset)); }, &assets);
            coroutines.init(host);
            save.init(platform, "test");
            ok = ok && assets.init(*device, platform, budget, 8 * 1024 * 1024) && canvas.init(*device, assets) && host.init();
            host.setStatementLimit(500000);
            Binder binder(host);
            ok = ok && bindEngine(binder, services);
            documented = binder.count();
        }

        ~Rig()
        {
            screens.shutdown();
            coroutines.shutdown();
            host.shutdown();
            audio.shutdown();
            net.shutdown();
            secureShutdown();
            solids.shutdown();
            canvas.shutdown();
            assets.shutdown();
            destroy(device);
        }

        void step(int count = 1)
        {
            for (int i = 0; i < count; i++)
            {
                assets.finish();
                audio.update();
                net.update(services.time);
                services.time += 1.0 / 60;
                input.update(platform.state, 1.0f / 60);
                screens.update(1.0f / 60);
                device->beginFrame({});
                canvas.begin();
                screens.draw();
                canvas.end();
                device->endFrame();
                i16 heard[800 * 2];
                audio.mix(heard, 800);  // as the output would
            }
        }

        void press(Action action)
        {
            input.simulate(action);
            step();
            step();
        }
    };
}
