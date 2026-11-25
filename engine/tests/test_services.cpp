#include "test.h"
#include "rig.h"

#include "assets/formats.h"

#include <stdlib.h>

using namespace nx;
using namespace nx::test;

namespace
{
    // Runs a script as the "shapes" screen.
    void openCode(Rig& w, const char* code)
    {
        w.platform.extraPath = "showcase/shapes/main.as";
        w.platform.extraText = code;
        w.screens.init(w.services, "showcase", "main");
        w.step(2);
        w.screens.push("shapes");
        w.step(2);
        if (w.screens.failed())
        {
            NX_ERROR("%s", w.screens.error());
        }
    }

    bool saved(Rig& w, const char* key, const char* value)
    {
        const char* got = w.save.get(key, "(nothing)");
        if (strcmp(got, value) != 0)
        {
            NX_ERROR("%s is '%s', not '%s'", key, got, value);
            return false;
        }
        return true;
    }
}

NX_TEST(cookedTexturesAreChecked)
{
    // 5 by 3 in an 8 by 4 BC1 texture: two blocks of eight bytes.
    u8 file[kTextureHeader + 16] = { 'N', 'X', 'T', '1', 5, 0, 3, 0, 8, 0, 4, 0, 1 };
    gpu::TextureData data;
    NX_EXPECT(readCookedTexture(file, sizeof(file), data) && data.width == 5 && data.fullWidth == 8 && data.format == gpu::PixelFormat::Bc1);
    NX_EXPECT(!readCookedTexture(file, sizeof(file) - 1, data));
    const struct { u32 at; u8 value; } damage[] = { { 5, 200 }, { 8, 200 }, { 9, 200 }, { 12, 200 }, { 4, 9 }, { 4, 0 } };
    for (const auto& d : damage)
    {
        u8 bad[sizeof(file)];
        memcpy(bad, file, sizeof(file));
        bad[d.at] = d.value;
        NX_EXPECT(!readCookedTexture(bad, sizeof(bad), data));
    }

    Rig w;
    w.platform.extraPath = "cooked.png";
    w.platform.extraText.assign(reinterpret_cast<const char*>(file), sizeof(file));
    Asset* a = w.assets.texture("cooked.png");
    w.assets.finish();
    NX_EXPECT(a->ready && !a->failed && a->width == 5 && a->bytes == 16);
    w.assets.release(a);
}

NX_TEST(changedFilesAreReloaded)
{
    Rig w;
    Asset* ship = w.assets.texture(kShip);
    w.assets.finish();
    const u32 loads = w.assets.stats().loads;
    w.assets.reloadChanged();
    NX_EXPECT(w.assets.pending() == 0);
    w.platform.time++;
    w.assets.reloadChanged();
    NX_EXPECT(w.assets.pending() == 1 && ship->ready);  // the old one is used until the new one is here
    w.assets.finish();
    NX_EXPECT(w.assets.stats().loads == loads + 1 && ship->ready && ship->width == 110);
    NX_EXPECT(w.assets.stats().inUseBytes == ship->bytes);

    // Half saved: what there was stays.
    w.platform.extraPath = kShip;
    w.platform.extraText = "not an image";
    w.platform.time++;
    w.assets.reloadChanged();
    w.assets.finish();
    NX_EXPECT(ship->ready && !ship->failed && ship->width == 110);
    w.assets.release(ship);

    openCode(w, "class A : Screen {}");
    NX_EXPECT(!w.screens.failed() && w.services.reloads == 0);
    w.screens.reloadChanged();
    NX_EXPECT(w.services.reloads == 0);
    w.platform.time++;
    w.platform.extraText = "class A : Screen { void onEnter() { Save::set(\"new\", \"yes\"); } }";
    w.screens.reloadChanged();
    w.step(2);
    NX_EXPECT(w.services.reloads == 1 && !w.screens.failed() && saved(w, "new", "yes"));
    // A broken one shows its error, and mending it carries on.
    w.platform.time++;
    w.platform.extraText = "class A : Screen {";
    w.screens.reloadChanged();
    w.step(2);
    NX_EXPECT(w.screens.failed());
    w.platform.time++;
    w.platform.extraText = "class A : Screen {}";
    w.screens.reloadChanged();
    w.step(2);
    NX_EXPECT(!w.screens.failed());
}

NX_TEST(screensWaitForTheirAssets)
{
    Rig w;
    openCode(w, R"(
        class A : Screen
        {
            Texture@ t = Assets::texture("global:missing.png");
            bool entered;
            void onEnter() { entered = true; Save::set("ready", "" + t.ready); }
            void update(float dt) { if (!entered) Save::set("early", "yes"); }
        }
    )");
    NX_EXPECT(!w.screens.loading() && !w.screens.failed());
    NX_EXPECT(saved(w, "ready", "true") && saved(w, "early", "(nothing)"));
}

NX_TEST(coroutinesWaitAndEnd)
{
    Rig w;
    openCode(w, R"(
        class A : Screen
        {
            void onEnter() { start(Coroutine(this.run)); start(forever); }
            void run()
            {
                Save::set("step", "1");
                wait(0.5f);
                Save::set("step", "2");
                for (int i = 0; i < 3; i++) nextUpdate();
                Save::set("step", "3");
            }
        }
        void forever() { while (true) wait(0.01f); }
    )");
    w.step(2);
    NX_EXPECT(saved(w, "step", "1") && w.coroutines.count() == 2);
    w.step(20);
    NX_EXPECT(saved(w, "step", "1"));
    w.step(9);
    // The menu holds them still.
    w.press(Action::Menu);
    w.step(60);
    const std::string before = w.save.get("step", "");
    w.step(60);
    NX_EXPECT(saved(w, "step", before.c_str()));
    w.press(Action::Menu);
    w.step(10);
    NX_EXPECT(saved(w, "step", "3") && w.coroutines.count() == 1);
    w.screens.home();
    w.step(2);
    NX_EXPECT(w.coroutines.count() == 0);

    const struct { const char* code; const char* error; } cases[] = {
        { "class A : Screen { void onEnter() { wait(1); } }", "only inside a coroutine" },
        { "class A : Screen { void onEnter() { start(bad); } } void bad() { nextUpdate(); int z = 0; z = 1 / z; }", "Divide by zero" },
        { "class A : Screen { void onEnter() { start(bad); } } void bad() { while (true) {} }", "endless loop" },
        { "class A : Screen { void onEnter() { for (int i = 0; i < 1000; i++) start(more); } } void more() { wait(1); }", "another coroutine" },
        { "class A : Screen { void onEnter() { Coroutine@ c; start(c); } }", "coroutine" },
    };
    for (const auto& c : cases)
    {
        w.platform.extraText = c.code;
        w.screens.push("shapes");
        w.step(5);
        NX_EXPECT(w.screens.failed() && strstr(w.screens.error(), c.error) != nullptr);
        if (!w.screens.failed() || strstr(w.screens.error(), c.error) == nullptr)
        {
            NX_ERROR("wanted '%s', got '%s'", c.error, w.screens.error());
        }
        w.screens.home();
        w.step(2);
        NX_EXPECT(w.coroutines.count() == 0);
    }
}

NX_TEST(tweensMoveByThemselves)
{
    Rig w;
    openCode(w, R"(
        class A : Screen
        {
            Tween t;
            void onEnter()
            {
                Tween rest(5);
                Save::set("rest", "" + rest.value + rest.done);
                t = Tween(10, 20, 1, EaseLinear);
            }
            void update(float dt)
            {
                Save::set("now", "" + int(t.value + 0.001f));
                Save::set("done", t.done ? "yes" : "no");
                if (t.done && t.value == 20) t.go(0, 0.5f, EaseBounce);
            }
        }
    )");
    NX_EXPECT(saved(w, "rest", "5true") && saved(w, "done", "no"));
    w.step(30);
    const int half = atoi(w.save.get("now", "0"));
    NX_EXPECT(half >= 14 && half <= 16);
    w.step(70);
    NX_EXPECT(saved(w, "done", "yes") && saved(w, "now", "0"));
    NX_EXPECT(!w.screens.failed());
}

NX_TEST(savesComeBack)
{
    TestPlatform platform;
    {
        Save save;
        save.init(platform, "t");
        NX_EXPECT(strcmp(save.get("x", "none"), "none") == 0);
        NX_EXPECT(save.set("name", "a \"quoted\" \\ line\nand \xC3\xA9") && save.set("score", "12") && save.set("gone", "1"));
        save.remove("gone");
        NX_EXPECT(save.flush());
        const std::string before = platform.saved;
        platform.saved = "x";
        NX_EXPECT(save.flush() && platform.saved == "x");  // nothing has changed: nothing is written
        save.set("score", "12");
        NX_EXPECT(save.flush() && platform.saved == "x");
        platform.saved = before;
        NX_EXPECT(!save.set(std::string(100, 'k').c_str(), "") && !save.set("k", std::string(5000, 'v').c_str()));
        for (int i = 0; i < 300; i++)
        {
            save.set(("key" + std::to_string(i)).c_str(), "v");
        }
        NX_EXPECT(!save.set("one more", "v"));
    }
    Save save;
    save.init(platform, "t");
    NX_EXPECT(strcmp(save.get("name", ""), "a \"quoted\" \\ line\nand \xC3\xA9") == 0 && strcmp(save.get("score", ""), "12") == 0);
    NX_EXPECT(strcmp(save.get("gone", "none"), "none") == 0);
    const char* const damaged[] = { "", "{", "{\"a\"", "{\"a\":\"b", "nonsense", "{\"a\":\"b\",}", "{\"a\":{\"b\":[1,2" };
    for (const char* bad : damaged)
    {
        platform.saved = bad;
        Save other;
        other.init(platform, "t");
        NX_EXPECT(other.flush());
    }
}

NX_TEST(soundsFromScripts)
{
    Rig w;
    openCode(w, R"(
        class A : Screen
        {
            Sound@ s = Assets::sound("global:missing.wav");
            void onEnter()
            {
                Voice v = s.play(1, 0, 1, true);
                Voice none;
                none.stop();
                v.fade(0.5f, 1);
                Save::set("r", "" + s.ready + s.failed + v.playing + none.playing + Audio::voices);
            }
        }
    )");
    NX_EXPECT(!w.screens.failed() && saved(w, "r", "truetruefalsefalse0"));
}

NX_TEST(bindingsChange)
{
    Input input;
    InputState state;
    const auto step = [&] { input.update(state, 1.0f / 60); };
    const auto key = [&](Key k, bool down) { state.keys[static_cast<size_t>(k)] = down; };
    NX_EXPECT(input.describeKeys(Action::Accept) == "Enter, Space");
    input.listen(Action::Accept);
    step();
    NX_EXPECT(input.listening());
    key(Key::K, true);
    step();
    NX_EXPECT(!input.listening() && input.describeKeys(Action::Accept) == "K, Space");
    NX_EXPECT(!input.held(Action::Accept));  // choosing it doesn't also do it
    key(Key::K, false);
    step();
    key(Key::K, true);
    step();
    NX_EXPECT(input.pressed(Action::Accept));
    key(Key::K, false);
    key(Key::Enter, true);
    step();
    NX_EXPECT(!input.held(Action::Accept));
    key(Key::Enter, false);
    step();

    // Escape calls it off, and the menu can't be taken away.
    input.listen(Action::Fire);
    key(Key::Escape, true);
    step();
    NX_EXPECT(!input.listening() && input.describeKeys(Action::Fire) == "Space");
    key(Key::Escape, false);
    step();
    input.listen(Action::Menu);
    NX_EXPECT(!input.listening());

    input.listen(Action::Fire);
    state.pads[0].connected = true;
    state.pads[0].buttons = 1u << static_cast<u32>(Button::Y);
    step();
    NX_EXPECT(!input.listening() && input.describeButtons(Action::Fire) == "Y");
    state.pads[0].buttons = 0;
    step();

    const std::string text = input.saveBindings();
    Input other;
    other.loadBindings(text.c_str());
    NX_EXPECT(other.describeKeys(Action::Accept) == "K, Space" && other.describeButtons(Action::Fire) == "Y");
    other.loadBindings("accept=;fire=Space;=;;menu=K,K,A,A;what=K,K,A,A;up=Nothing,None,A,Wrong");
    NX_EXPECT(other.describeKeys(Action::Accept) == "K, Space" && other.describeKeys(Action::Menu) == "Escape" && other.describeKeys(Action::Up) == "Up");
    other.resetBindings();
    NX_EXPECT(other.saveBindings() == Input().saveBindings());
}

namespace
{
    u32 g_fetched = 0;

    // Anything from the web is this one picture, but for what isn't there.
    bool pretendWeb(const char* url, FileData& out)
    {
        g_fetched++;
        if (strstr(url, "missing"))
        {
            return false;
        }
        const u8 file[kTextureHeader + 16] = { 'N', 'X', 'T', '1', 5, 0, 3, 0, 8, 0, 4, 0, 1 };
        u8* data = static_cast<u8*>(memAlloc(sizeof(file), MemTag::Texture));
        memcpy(data, file, sizeof(file));
        out.take(data, sizeof(file));
        return true;
    }
}

NX_TEST(picturesComeFromTheWeb)
{
    g_fetched = 0;
    Rig w;
    w.assets.setWeb(pretendWeb);
    Asset* a = w.assets.texture("https://example.com/A.png");
    Asset* same = w.assets.texture("https://example.com/A.png");
    Asset* other = w.assets.texture("https://example.com/a.png");   // names there mind their case
    Asset* missing = w.assets.texture("https://example.com/missing.png");
    w.assets.finish();
    NX_EXPECT(a == same && a != other && g_fetched == 3);
    NX_EXPECT(a->ready && !a->failed && a->width == 5);
    NX_EXPECT(missing->failed);
    char longest[300];
    memset(longest, 'x', sizeof(longest));
    memcpy(longest, "https://", 8);
    longest[sizeof(longest) - 1] = 0;
    Asset* tooLong = w.assets.texture(longest);
    Asset* sound = w.assets.sound("https://example.com/a.wav");
    w.assets.finish();
    NX_EXPECT(tooLong->failed && sound->failed);
    for (Asset* each : { a, same, other, missing, tooLong, sound })
    {
        w.assets.release(each);
    }
}

NX_TEST(effectsAreLeftOutWhereThereAreNone)
{
    Rig w;
    openCode(w, "class S : Screen { Effect@ e = Assets::effect(\"global:effect/tube.fx\"); Effect@ none = Assets::effect(\"global:effect/nothing.fx\"); Entity@ box;"
                "void onEnter() { e.set(0, 1); @box = Space::spawn(Vec3()); box.shape(SolidBox, Red); box.effect(e);"
                "  Save::set(\"works\", e.works ? \"yes\" : \"no\"); Save::set(\"failed\", none.failed ? \"yes\" : \"no\"); }"
                "void draw(Canvas@ c) { c.design(1280, 720); Space::draw(c); Save::set(\"bloom\", c.bloom() ? \"yes\" : \"no\"); Save::set(\"effect\", c.effect(e) ? \"yes\" : \"no\"); } }");
    w.step(3);
    NX_EXPECT(!w.screens.failed());
    NX_EXPECT(saved(w, "works", "no") && saved(w, "failed", "yes") && saved(w, "bloom", "no") && saved(w, "effect", "no"));
}
