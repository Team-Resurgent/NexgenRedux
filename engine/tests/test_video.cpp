#include "test.h"
#include "rig.h"

#include "video/video.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

using namespace nx;
using namespace nx::test;

namespace
{
    constexpr const char* kFilm = "showcase/video/asset/mandel.mpg";

    // Plays for a while, as the engine would, and says how loud it was.
    int run(Rig& r, Video& v, int frames)
    {
        int loudest = 0;
        for (int i = 0; i < frames; i++)
        {
            v.update(1.0f / 60);
            r.audio.update();
            i16 heard[800 * 2];
            r.audio.mix(heard, 800);
            for (const i16 s : heard)
            {
                loudest = abs(s) > loudest ? abs(s) : loudest;
            }
        }
        return loudest;
    }
}

NX_TEST(videosPlayTheirPicturesAndSound)
{
    Rig r;
    NX_EXPECT(r.ok);
    FileData file;
    NX_EXPECT(r.platform.readFile(kFilm, file, MemTag::General));
    Video* v = create<Video>(MemTag::General, r.assets, r.audio);
    u8* bytes = file.data;
    file.data = nullptr;
    NX_EXPECT(v->open(bytes, file.size));
    NX_EXPECT(v->width() == 480 && v->height() == 272 && v->picture() && v->picture()->ready);
    NX_EXPECT(v->duration() > 11 && v->duration() < 13 && v->framerate() > 24 && v->framerate() < 26);
    NX_EXPECT(!v->playing() && v->frames() == 0 && v->time() == 0);

    // Nothing happens until it is played.
    run(r, *v, 10);
    NX_EXPECT(v->frames() == 0);
    v->play();
    const int loud = run(r, *v, 60);
    NX_EXPECT(v->playing() && v->frames() >= 20 && v->frames() <= 30);
    NX_EXPECT(fabsf(v->time() - 1) < 0.1f);
    NX_EXPECT(loud > 1000);

    // Paused, it stays where it is; stopped, it goes back to the start.
    v->pause();
    const u32 shown = v->frames();
    run(r, *v, 30);
    NX_EXPECT(v->frames() == shown && !v->playing());
    v->stop();
    NX_EXPECT(v->time() < 0.05f && !v->done());
    v->setVolume(0.5f);
    NX_EXPECT(v->volume() == 0.5f);

    // To the end, where it stops by itself and its sound plays out.
    v->play();
    run(r, *v, 60 * 13);
    NX_EXPECT(v->done() && !v->playing());
    run(r, *v, 60);
    NX_EXPECT(r.audio.voicesInUse() == 0);
    // Played again, it starts over.
    v->play();
    run(r, *v, 30);
    NX_EXPECT(v->playing() && v->time() < 1);
    // Looping, it goes on past the end.
    v->setLoop(true);
    run(r, *v, 60 * 13);
    NX_EXPECT(v->playing() && !v->done());
    v->release();
    r.audio.update();
}

NX_TEST(whatIsNotAVideoIsRefused)
{
    Rig r;
    NX_EXPECT(r.ok);
    const char text[] = "this is not a film at all, just some words that go on for a while to fill a buffer";
    u8* bytes = static_cast<u8*>(memAlloc(sizeof(text), MemTag::General));
    memcpy(bytes, text, sizeof(text));
    Video* v = create<Video>(MemTag::General, r.assets, r.audio);
    NX_EXPECT(!v->open(bytes, sizeof(text)) && v->picture() == nullptr);
    v->play();
    v->update(0.1f);
    NX_EXPECT(!v->playing());
    v->release();
    Video* none = create<Video>(MemTag::General, r.assets, r.audio);
    NX_EXPECT(!none->open(nullptr, 0));
    none->release();
}

NX_TEST(playlistsPlayOneAfterAnother)
{
    Rig r;
    NX_EXPECT(r.ok);
    r.platform.extraPath = "showcase/shapes/main.as";
    r.platform.extraText = R"(
        class A : Screen
        {
            Playlist@ list = Playlist();
            array<uint> heard;
            // A mistake stops the screen, which the test sees.
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            void onEnter()
            {
                list.add(Assets::sound("global:sound/coin.wav"));
                list.add(Assets::sound("global:sound/bell.wav"));
                list.add(Assets::sound("global:sound/coin.wav"));
                list.loop = false;
                list.crossfade = 0;
                check(list.count == 3 && !list.playing);
                list.play();
            }
            void update(float dt)
            {
                if (heard.length() == 0 || heard[heard.length() - 1] != list.index)
                    heard.insertLast(list.index);
                if (!list.playing && heard.length() > 1)
                {
                    check(heard.length() == 3 && heard[0] == 0 && heard[1] == 1 && heard[2] == 2);
                    // Straight to one, and round again.
                    list.loop = true;
                    list.index = 1;
                    check(list.playing && list.index == 1);
                    list.stop(0);
                    Screens::home();
                }
            }
            void draw(Canvas@ c) {}
        }
    )";
    NX_EXPECT(r.screens.init(r.services, "showcase", "main"));
    r.step(2);
    r.screens.push("shapes");
    for (int i = 0; i < 1200 && (i < 5 || r.screens.depth() > 1); i++)
    {
        r.step();
    }
    NX_EXPECT(!r.screens.failed() && r.screens.depth() == 1);
}

NX_TEST(scriptsPlayVideos)
{
    Rig r;
    NX_EXPECT(r.ok);
    FileData film;
    NX_EXPECT(r.platform.readFile(kFilm, film, MemTag::General));
    r.platform.files["showcase/shapes/asset/film.mpg"] = std::string(reinterpret_cast<const char*>(film.data), film.size);
    r.platform.extraPath = "showcase/shapes/main.as";
    r.platform.extraText = R"(
        class A : Screen
        {
            Video@ film = Assets::video("asset:film.mpg");
            void onEnter() { film.play(); }
            void update(float dt)
            {
                Texture@ t = film.picture;
                if (film.frames > 5 && film.playing && t.width == 480 && t.height == 272 && film.time > 0.2)
                {
                    print("video works");
                    Screens::home();
                }
            }
            void draw(Canvas@ c) { c.image(film.picture, 0, 0, 480, 272); }
        }
    )";
    NX_EXPECT(r.screens.init(r.services, "showcase", "main"));
    r.step(2);
    r.screens.push("shapes");
    for (int i = 0; i < 200 && (i < 5 || r.screens.depth() > 1); i++)
    {
        r.step();
    }
    NX_EXPECT(!r.screens.failed() && r.screens.depth() == 1);
}
