#include "assets/assets.h"
#include "base/endian.h"
#include "base/log.h"
#include "bindings/bindings.h"
#include "audio/audio.h"
#include "canvas/canvas.h"
#include "net/net.h"
#include "net/secure.h"
#include "gpu/gpu.h"
#include "input/input.h"
#include "platform/platform.h"
#include "screens/screens.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The showcase: a main screen that opens the others.
//
//   --screen ID     start with a screen open
//   --frames N      stop after N frames of a fixed 60th of a second, so runs can be repeated
//   --capture FILE  save the last frame as a BMP
//   --tour N        open every screen N times over, unattended, then stop
//   --bench         with --frames: don't wait for the display, and say how long frames took
//   --demo SECONDS  show every screen, or the one given, for a while, playing it, then stop
//   --record FILE --record-sound FILE   with --demo: write each frame (RGBA), and the sound (16-bit, stereo, 48 kHz)
//   --docs FILE     write the script reference and stop
//   --budget MB     how much images and fonts may take
//   --dev           reload scripts and images when their files change
//   --saves FOLDER  keep saves there, and keep them with --demo too
//   --size W H      window size
//   --hidden (and silent), --null, --menu (open the engine's menu for a capture)
//   --clean         with --demo: no captions over the demos, for footage
//   --meeting PORT  be a meeting place for rooms on the internet, with no window, until stopped
namespace
{
    using namespace nx;

    constexpr float kStep = 1.0f / 60.0f;

    struct Options
    {
        const char* screen = nullptr;
        const char* capture = nullptr;
        const char* docs = nullptr;
        const char* saves = nullptr;
        const char* record = nullptr;
        const char* recordSound = "";
        int meeting = 0;
        int frames = 0;
        int tour = 0;
        float demo = 0;
        bool bench = false;
        size_t budget = 0;
        u32 width = 1280, height = 720;
        bool hidden = false;
        bool clean = false;        // demos without their captions, for footage
        bool null = false;
        bool menu = false;
        bool dev = false;
    };

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
        const bool ok = pixels && device.capture(pixels, bytes) && writeBmp(path, pixels, w, h);
        memFree(pixels);
        return ok;
    }

    // Every frame and all that is heard, whatever the machine can keep up with showing.
    class Recording
    {
    public:
        bool open(const char* path, const char* sound, gpu::Device& device)
        {
            device.size(m_width, m_height);
            m_pixels = static_cast<u8*>(memAlloc(bytes(), MemTag::General));
            m_video = fopen(path, "wb");
            m_sound = fopen(sound, "wb");
            return m_pixels && m_video && m_sound;
        }

        ~Recording()
        {
            for (FILE* file : { m_video, m_sound })
            {
                if (file)
                {
                    fclose(file);
                }
            }
            memFree(m_pixels);
        }

        bool frame(gpu::Device& device, Audio& audio)
        {
            constexpr u32 heard = kMixRate / 60;
            i16 sound[heard * 2];
            audio.mix(sound, heard);
            return device.capture(m_pixels, bytes()) && fwrite(m_pixels, 1, bytes(), m_video) == bytes() && fwrite(sound, 1, sizeof(sound), m_sound) == sizeof(sound);
        }

    private:
        size_t bytes() const { return static_cast<size_t>(m_width) * m_height * 4; }

        u32 m_width = 0, m_height = 0;
        u8* m_pixels = nullptr;
        FILE* m_video = nullptr;
        FILE* m_sound = nullptr;
    };

    bool writeText(const char* path, const std::string& text)
    {
        FILE* file = fopen(path, "wb");
        const bool ok = file && fwrite(text.data(), 1, text.size(), file) == text.size();
        if (file)
        {
            fclose(file);
        }
        return ok;
    }

    // Shows every screen to someone, pressing what its screen.json says over and over: "right:0.5" holds
    // right for half a second, "accept" presses it, "right+fire:1" holds both and "wait:1" does nothing.
    class Demo
    {
    public:
        // Only one screen, if it is named.
        Demo(float seconds, const char* only) : m_seconds(seconds), m_only(only) {}

        // True once finished.
        bool step(Services& s, float dt)
        {
            Screens& screens = *s.screens;
            m_steps++;
            if (screens.loading())
            {
                s.time = screens.depth() > 1 && m_clock > 0 ? m_clock : s.time;
                return false;
            }
            m_left -= dt;
            if (screens.depth() > 1 && m_left <= 0)
            {
                screens.home();
                m_left = kAtHome;
                // Along to the next tile, over any that are skipped.
                m_moves = skip(screens, m_next) - m_next + 1;
                return false;
            }
            if (screens.depth() > 1)
            {
                play(s, dt);
                return false;
            }
            if (m_left > 0)
            {
                // On and off, as each press counts once.
                m_beat++;
                if (m_moves && m_beat % 8 == 0)
                {
                    m_moves--;
                    s.input->simulate(Action::Right);
                }
                return false;
            }
            m_next = skip(screens, m_next);
            while (m_only && m_next < screens.count() && strcmp(screens.info(m_next)->id, m_only) != 0)
            {
                m_next++;
            }
            if (m_next >= screens.count())
            {
                return true;
            }
            const ScreenInfo& info = *screens.info(m_next++);
            NX_INFO("demo: %s frame %u", info.id, static_cast<unsigned>(m_steps));
            snprintf(m_script, sizeof(m_script), "%s", info.demo);
            m_at = m_script;
            m_hold = kBeforePlaying;
            m_pressed = 0;
            m_length = info.seconds > m_seconds ? info.seconds : m_seconds;
            m_left = m_length;
            // Each begins alike, on every machine and however long the others took to load. The time never goes back.
            s.random = 2463534242u;
            m_clock = m_next * 60.0 > s.time ? m_next * 60.0 : s.time;
            s.time = m_clock;
            screens.push(info.id);
            return false;
        }

        // The screen's name, for its first few seconds.
        void caption(Services& s)
        {
            if (s.screens->depth() < 2 || s.screens->loading())
            {
                return;
            }
            const float shown = m_length - m_left;
            const float alpha = shown < 0.4f ? shown / 0.4f : shown > 2.6f ? (3.2f - shown) / 0.6f : 1;
            const ScreenInfo* info = m_next ? s.screens->info(m_next - 1) : nullptr;
            if (alpha <= 0 || info == nullptr)
            {
                return;
            }
            if (m_title == nullptr)
            {
                m_title = s.assets->font("global/freesans.sfn", 44);
                m_text = s.assets->font("global/freesans.sfn", 24);
            }
            Canvas& c = *s.canvas;
            c.designHeight(720);
            c.setAlpha(alpha);
            c.gradient(0, 520, c.width(), 200, { 0, 0, 0, 0 }, { 0, 0, 0, 0.85f });
            c.rect(48, 610, 6, 78, { 0.27f, 0.55f, 0.96f, 1 });
            c.text(*m_title, info->title, 72, 602, { 1, 1, 1, 1 });
            c.text(*m_text, info->description, 74, 658, { 0.8f, 0.85f, 0.95f, 1 });
            c.setAlpha(1);
        }

        void finish(Services& s)
        {
            if (m_title)
            {
                s.assets->release(m_title);
                s.assets->release(m_text);
                m_title = m_text = nullptr;
            }
        }

    private:
        static constexpr float kAtHome = 0.8f;
        static constexpr float kBeforePlaying = 0.8f;

        // Broken on purpose, or nothing to look at.
        static u32 skip(const Screens& screens, u32 next)
        {
            while (next < screens.count() && (strcmp(screens.info(next)->id, "main") == 0 || strcmp(screens.info(next)->id, "faulty") == 0))
            {
                next++;
            }
            return next;
        }

        void play(Services& s, float dt)
        {
            if (m_script[0] == 0)
            {
                return;
            }
            m_hold -= dt;
            if (m_hold <= 0)
            {
                next();
            }
            for (u32 i = 0; i < static_cast<u32>(Action::Count); i++)
            {
                if (m_pressed & (1u << i))
                {
                    s.input->simulate(static_cast<Action>(i));
                }
            }
        }

        void next()
        {
            m_at = *m_at ? m_at : m_script;
            char word[64];
            size_t length = 0;
            while (*m_at == ' ')
            {
                m_at++;
            }
            while (*m_at && *m_at != ' ')
            {
                if (length + 1 < sizeof(word))
                {
                    word[length++] = *m_at;
                }
                m_at++;
            }
            word[length] = 0;
            m_pressed = 0;
            m_hold = 1.5f / 60;
            char* seconds = strchr(word, ':');
            if (seconds)
            {
                *seconds = 0;
                m_hold = static_cast<float>(atof(seconds + 1));
            }
            for (char* name = word; name;)
            {
                char* plus = strchr(name, '+');
                if (plus)
                {
                    *plus = 0;
                }
                Action action;
                if (Input::parse(name, action) && action != Action::Menu)
                {
                    m_pressed |= 1u << static_cast<u32>(action);
                }
                name = plus ? plus + 1 : nullptr;
            }
        }

        float m_seconds;
        const char* m_only;
        float m_left = kAtHome;
        u32 m_next = 0;
        char m_script[400] = {};
        float m_length = 0;
        const char* m_at = m_script;
        float m_hold = 0;
        u32 m_pressed = 0;
        u32 m_moves = 0, m_beat = 0, m_steps = 0;
        double m_clock = 0;
        Asset* m_title = nullptr;
        Asset* m_text = nullptr;
    };

    // Opens every screen in turn, a few times over, and watches what is left behind.
    class Tour
    {
    public:
        explicit Tour(int rounds) : m_rounds(rounds) {}

        // True once finished.
        bool step(Services& s, bool& ok)
        {
            if (m_wait > 0)
            {
                m_wait--;
                return false;
            }
            Screens& screens = *s.screens;
            if (screens.loading())
            {
                return false;
            }
            if (screens.depth() > 1)
            {
                if (screens.failed())
                {
                    NX_ERROR("tour: %s failed: %s", screens.current(), screens.error());
                    ok = false;
                }
                NX_INFO("tour: %-12s %5.1f fps, %3u draws, %2u voices, script %u KB, assets %u KB, free %u KB", screens.current(), static_cast<double>(s.fps), static_cast<unsigned>(s.draws),
                    static_cast<unsigned>(s.audio->voicesInUse()), static_cast<unsigned>(memStats(MemTag::Script).bytes / 1024), static_cast<unsigned>((s.assets->stats().inUseBytes + s.assets->stats().warmBytes) / 1024),
                    static_cast<unsigned>(s.platform->freeMemory() / 1024));
                // The way back has to work from anywhere: through the engine's menu, as a player would.
                screens.home();
                m_wait = 3;
                return false;
            }
            while (m_next < screens.count() && strcmp(screens.info(m_next)->id, screens.current()) == 0)
            {
                m_next++;
            }
            if (m_next < screens.count())
            {
                screens.push(screens.info(m_next++)->id);
                m_wait = m_round == 0 ? 90 : 5;
                return false;
            }
            m_next = 0;
            m_round++;
            const size_t script = memStats(MemTag::Script).bytes;
            const u32 loads = s.assets->stats().loads;
            NX_INFO("tour: round %d done, script %u bytes, %u files read, %u hits, %u evictions", m_round, static_cast<unsigned>(script), static_cast<unsigned>(loads),
                static_cast<unsigned>(s.assets->stats().hits), static_cast<unsigned>(s.assets->stats().evictions));
            if (m_round == 2)
            {
                m_script = script;
            }
            if (m_round > 1 && loads != m_loads && s.assets->stats().evictions == 0 && s.assets->stats().trims == m_trimsBefore)
            {
                NX_ERROR("tour: screens read files again although nothing was evicted");
                ok = false;
            }
            if (m_round == 1)
            {
                // Saving, which screens may not do on an unattended run.
                FileData back;
                static const char text[] = "{ \"self\": \"test\" }";
                if (!s.platform->writeSave("selftest", text, sizeof(text) - 1) || !s.platform->loadSave("selftest", back) || back.size != sizeof(text) - 1 || memcmp(back.data, text, back.size) != 0)
                {
                    NX_ERROR("tour: a save did not come back");
                    ok = false;
                }
                s.platform->removeSave("selftest");
                FileData gone;
                if (s.platform->loadSave("selftest", gone))
                {
                    NX_ERROR("tour: a save did not go");
                    ok = false;
                }
            }
            // What a screen unloads on purpose in one round is read again in the next.
            m_trimsBefore = m_trims;
            m_trims = s.assets->stats().trims;
            m_loads = loads;
            if (m_round >= m_rounds)
            {
                // AngelScript keeps half a kilobyte for each array initialiser of a type that has gone, until it
                // ends or another lands on the same address. With a sanitizer, which never reuses one, that is all of them.
                if (m_round > 2 && script > m_script + 128 * 1024)
                {
                    NX_ERROR("tour: script memory grew from %u to %u", static_cast<unsigned>(m_script), static_cast<unsigned>(script));
                    ok = false;
                }
                return true;
            }
            return false;
        }

    private:
        int m_rounds;
        int m_round = 0;
        int m_wait = 60;
        u32 m_next = 0;
        u32 m_loads = 0;
        u32 m_trims = 0, m_trimsBefore = 0;
        size_t m_script = 0;
    };

    // A meeting place: no window, no screens, only the network, until it is stopped (or for --frames 60ths).
    bool meet(Platform& platform, const Options& options)
    {
        MeetingPlace place;
        if (options.meeting <= 0 || options.meeting > 65535 || !place.open(static_cast<u16>(options.meeting), false))
        {
            NX_ERROR("meeting: can't listen on port %d", options.meeting);
            return false;
        }
        NX_INFO("meeting: listening on port %u", place.port());
        double said = platform.seconds();
        for (int frame = 0; options.frames == 0 || frame < options.frames; frame++)
        {
            const double now = platform.seconds();
            place.update(now);
            if (now - said > 60)
            {
                said = now;
                NX_INFO("meeting: %u rooms, %u pairs, %llu bytes passed on", place.rooms(), place.pairs(), static_cast<unsigned long long>(place.passedOn()));
            }
            platform.sleep(0.002);
        }
        return true;
    }

    bool run(Platform& platform, const Options& options)
    {
        WindowDesc window;
        window.title = "Nexgen";
        window.width = options.width;
        window.height = options.height;
        window.hidden = options.hidden;
        if (!options.null && !platform.openWindow(window))
        {
            return false;
        }
        gpu::Device* device = options.null ? gpu::createNullDevice(options.width, options.height) : gpu::createDevice(platform.nativeWindow());
        if (device == nullptr)
        {
            return false;
        }

        bool ok = true;
        {
            // Destroyed in reverse: the scripts let go of their assets before the cache goes.
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
            services.platform = &platform;
            services.device = device;
            services.assets = &assets;
            services.canvas = &canvas;
            services.solids = &solids;
            services.net = &net;
            secureSetPlatform(&platform);
            assets.setWeb(webRead);
            solids.init(*device);
            services.input = &input;
            services.host = &host;
            services.audio = &audio;
            services.save = &save;
            services.coroutines = &coroutines;

            const bool small = platform.totalMemory() && platform.totalMemory() <= 128u * 1024 * 1024;
            const size_t textures = options.budget ? options.budget : (small ? 20u : 256u) * 1024 * 1024;
            ok = assets.init(*device, platform, textures, (small ? 6u : 64u) * 1024 * 1024) && canvas.init(*device, assets) && host.init();
            ok = ok && audio.init(options.null || options.docs || options.hidden || options.record ? createNullAudioOutput(options.record == nullptr) : createAudioOutput());
            audio.onFinished([](void* asset, void* cache) { static_cast<Assets*>(cache)->release(static_cast<Asset*>(asset)); }, &assets);
            coroutines.init(host);
            save.init(platform, "showcase");
            if (options.frames || options.tour || (options.demo > 0 && !options.saves) || options.docs)
            {
                save.setReadOnly();
            }
            loadSettings(services);
            services.development = options.dev;
            host.setStatementLimit(5000000);
            if (ok)
            {
                Binder binder(host);
                ok = bindEngine(binder, services);
                NX_INFO("%s, %s, %s, AngelScript %s, %u documented script calls", platform.name(), device->name(), audio.outputName(), ScriptHost::version(), static_cast<unsigned>(binder.count()));
                if (ok && options.docs)
                {
                    ok = writeText(options.docs, "# Script reference\n\nGenerated by `showcase --docs`. Do not edit.\n" + binder.reference());
                    screens.setCanQuit(false);
                }
            }
            ok = ok && (options.docs || screens.init(services, "showcase", "main"));

            if (ok && !options.docs)
            {
                if (options.screen && options.demo <= 0)
                {
                    screens.push(options.screen);
                }
                bool done = false;
                Tour tour(options.tour);
                device->setWait(!options.bench && !options.record);
                Recording recording;
                if (options.record && !recording.open(options.record, options.recordSound, *device))
                {
                    NX_ERROR("cannot write %s", options.record);
                    ok = false;
                    done = true;
                }
                const double recordingBegan = platform.seconds();
                double updating = 0, drawing = 0, began = 0;
                Demo demo(options.demo, options.screen);
                double last = platform.seconds();
                double nextLook = 0;
                float unspent = 0, between = 0;
                int frame = 0;
                while (!done && platform.pumpEvents() && !screens.quitRequested())
                {
                    const double now = platform.seconds();
                    if (frame == 30)
                    {
                        began = now;  // once it has settled
                        updating = drawing = 0;
                    }
                    const float elapsed = static_cast<float>(now - last);
                    last = now;
                    // The time is what is averaged: a short frame after a long one mustn't look like speed.
                    between = frame < 10 ? elapsed : between * 0.95f + elapsed * 0.05f;
                    services.fps = 1.0f / (between > 0.0005f ? between : 0.0005f);

                    if (options.frames)
                    {
                        assets.finish();  // loading times would make two runs differ
                    }
                    else
                    {
                        assets.update();
                    }
                    audio.update();
                    net.update(now);
                    if (options.dev && now >= nextLook)
                    {
                        nextLook = now + 0.5;
                        assets.reloadChanged();
                        screens.reloadChanged();
                    }

                    // Whole steps of a fixed length, so every machine runs the same. After a long stall
                    // the time is dropped rather than caught up on.
                    const bool fixed = options.frames > 0 || options.tour > 0 || options.record;
                    unspent = fixed ? kStep : unspent + (elapsed > 0.1f ? 0.1f : elapsed);
                    // Half a frame early is close enough, and avoids a stutter when the display is 60 Hz.
                    while (unspent >= kStep * 0.5f && !screens.quitRequested())
                    {
                        if (options.menu && frame == options.frames - 5)
                        {
                            input.simulate(Action::Menu);
                        }
                        if (options.demo > 0)
                        {
                            done = demo.step(services, kStep) || done;
                        }
                        u32 screenWidth = 0, screenHeight = 0;
                        device->size(screenWidth, screenHeight);
                        input.setScreen(static_cast<float>(screenWidth), static_cast<float>(screenHeight));
                        input.update(platform.input(), kStep);
                        services.time += kStep;
                        screens.update(kStep);
                        unspent -= kStep;
                        if (options.tour)
                        {
                            done = tour.step(services, ok) || done;
                        }
                    }
                    const double updated = platform.seconds();
                    updating += updated - now;
                    if (device->beginFrame({ 0, 0, 0, 1 }))
                    {
                        canvas.begin();
                        screens.draw();
                        // Where nothing else shows the pointer, it is drawn here: an arrow, outlined.
                        if ((platform.drawsPointer() && input.raw().hasMouse) || input.stickPointing())
                        {
                            u32 w = 0, h = 0;
                            device->size(w, h);
                            canvas.design(static_cast<float>(w), static_cast<float>(h));
                            const float x = input.raw().mouseX, y = input.raw().mouseY, k = static_cast<float>(h) / 480;
                            const gpu::Color dark = { 0, 0, 0, 0.85f }, light = { 1, 1, 1, 1 };
                            canvas.triangle({ x - 1.5f * k, y - 2.5f * k }, { x - 1.5f * k, y + 20 * k }, { x + 15 * k, y + 15 * k }, dark, dark, dark);
                            canvas.triangle({ x, y }, { x, y + 16 * k }, { x + 11.5f * k, y + 12.5f * k }, light, light, light);
                        }
                        if (options.demo > 0 && !options.clean)
                        {
                            demo.caption(services);
                        }
                        canvas.end();
                        drawing += platform.seconds() - updated;
                        device->endFrame();
                        services.draws = canvas.draws();
                        if (options.record && !recording.frame(*device, audio))
                        {
                            NX_ERROR("the recording stopped");
                            done = true;
                        }
                    }
                    host.collectStep();
                    frame++;
                    // No faster than it is seen, as some of it waits for a disk or a network.
                    if (const double ahead = options.record ? recordingBegan + frame / 60.0 - platform.seconds() : 0; ahead > 0.002)
                    {
                        platform.sleep(ahead);
                    }
                    done = done || (options.frames > 0 && frame >= options.frames);
                }
                if (screens.failed())
                {
                    NX_ERROR("%s: %s", screens.current(), screens.error());
                    ok = false;
                }
                demo.finish(services);
                if (options.bench && frame > 30)
                {
                    const double frames = frame - 30;
                    NX_INFO("%s: %.2f ms a frame: %.2f updating, %.2f drawing, %u draws", screens.current(), (platform.seconds() - began) * 1000 / frames, updating * 1000 / frames, drawing * 1000 / frames, static_cast<unsigned>(device->stats().draws));
                }
                if (device->stats().rejected)
                {
                    NX_ERROR("%u draws were rejected", static_cast<unsigned>(device->stats().rejected));
                    ok = false;
                }
                if (options.capture)
                {
                    ok = saveCapture(*device, options.capture) && ok;
                }
            }
            screens.shutdown();
            coroutines.shutdown();
            host.shutdown();
            audio.shutdown();
            saveSettings(services);
            save.flush();
            assets.trim();
            if (ok && assets.stats().inUse)
            {
                NX_ERROR("%u assets were still held", static_cast<unsigned>(assets.stats().inUse));
                ok = false;
            }
            webStop();
            net.shutdown();
            solids.shutdown();
            canvas.shutdown();
            assets.shutdown();
            secureShutdown();
        }
        destroy(device);
        return ok;
    }

    void findTour(const char* name, void* user)
    {
        Options& options = *static_cast<Options*>(user);
        if (strcmp(name, "tour") == 0 && options.tour == 0)
        {
            options.tour = 3;
        }
        else if (strcmp(name, "demo") == 0 && options.demo <= 0)
        {
            options.demo = 8;
        }
        else if (strcmp(name, "null") == 0)
        {
            options.null = true;
        }
#if defined(NX_GPU_D3D8)
        else if (strcmp(name, "trial") == 0)
        {
            options.saves = "D:\\media\\trial";
        }
#endif
    }
}

#if defined(NX_GPU_D3D8) || defined(NX_GPU_D3D9)
int main()
{
    const int argc = 0;  // a title has no command line
    char** argv = nullptr;
#else
int main(int argc, char** argv)
{
#endif
    Options options;
    const auto value = [&](int& i) { return i + 1 < argc ? argv[++i] : ""; };
    for (int i = 1; i < argc; i++)
    {
        const char* a = argv[i];
        if (strcmp(a, "--screen") == 0) options.screen = value(i);
        else if (strcmp(a, "--capture") == 0) options.capture = value(i);
        else if (strcmp(a, "--record") == 0) options.record = value(i);
        else if (strcmp(a, "--record-sound") == 0) options.recordSound = value(i);
        else if (strcmp(a, "--saves") == 0) options.saves = value(i);
        else if (strcmp(a, "--docs") == 0) options.docs = value(i);
        else if (strcmp(a, "--frames") == 0) options.frames = atoi(value(i));
        else if (strcmp(a, "--tour") == 0) options.tour = atoi(value(i));
        else if (strcmp(a, "--demo") == 0) options.demo = static_cast<float>(atof(value(i)));
        else if (strcmp(a, "--budget") == 0) options.budget = static_cast<size_t>(atof(value(i)) * 1024 * 1024);
        else if (strcmp(a, "--size") == 0) { options.width = static_cast<u32>(atoi(value(i))); options.height = static_cast<u32>(atoi(value(i))); }
        else if (strcmp(a, "--hidden") == 0) options.hidden = true;
        else if (strcmp(a, "--clean") == 0) options.clean = true;
        else if (strcmp(a, "--null") == 0) options.null = true;
        else if (strcmp(a, "--menu") == 0) options.menu = true;
        else if (strcmp(a, "--bench") == 0) options.bench = true;
        else if (strcmp(a, "--dev") == 0) options.dev = true;
        else if (strcmp(a, "--meeting") == 0) options.meeting = atoi(value(i));
    }

    nx::Platform* platform = nx::createPlatform();
    bool ok = platform != nullptr;
    if (platform)
    {
        // Consoles have no command line: a folder called tour, or demo, on the disc asks for one. One called
        // trial has the saves.
        platform->listFolders("", findTour, &options);
        if (options.saves && !platform->setSaveFolder(options.saves))
        {
            NX_ERROR("cannot keep saves in %s", options.saves);
            ok = false;
        }
        ok = ok && (options.meeting ? meet(*platform, options) : run(*platform, options));
    }
    nx::destroy(platform);

    const nx::MemStats left = nx::memStatsTotal();
    if (left.count)
    {
        NX_ERROR("at exit: %u bytes in %u allocations still held", static_cast<unsigned>(left.bytes), static_cast<unsigned>(left.count));
        ok = false;
    }
    NX_INFO("%s", ok ? "EXIT OK" : "EXIT FAILED");
    return ok ? 0 : 1;
}
