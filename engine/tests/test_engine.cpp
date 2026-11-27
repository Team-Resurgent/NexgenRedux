#include "test.h"

#include <math.h>
#include "rig.h"

#include "assets/decode.h"
#include "base/json.h"

using namespace nx;
using namespace nx::test;

NX_TEST(assetsAreSharedAndKeptWarm)
{
    Rig w;
    const int before = w.platform.reads;
    Asset* a = w.assets.texture(kShip);
    Asset* b = w.assets.texture("showcase\\Parallax/./x/../asset//Ship.png");
    NX_EXPECT(a == b && a->refs == 2 && !a->ready && w.assets.pending() == 1);
    w.assets.finish();
    NX_EXPECT(a->ready && !a->failed && w.assets.pending() == 0);
    NX_EXPECT(a->width == 110 && a->height == 95);
    NX_EXPECT(w.platform.reads == before + 1);
    w.assets.release(a);
    w.assets.release(b);
    NX_EXPECT(w.assets.stats().inUse == 0 && w.assets.stats().warm == 1);
    Asset* again = w.assets.texture(kShip);
    NX_EXPECT(again == a && w.platform.reads == before + 1);
    w.assets.release(again);
    w.assets.trim();
    NX_EXPECT(w.assets.stats().warm == 0);
    NX_EXPECT(w.device->stats().textureBytes == 16 * 16 * 4 + 4 + 32 * 32 * 4);  // only the placeholders and the canvas's dot are left
}

NX_TEST(assetsStayWithinBudget)
{
    Rig w(5 * 1024 * 1024);
    Asset* stars = w.assets.texture(kStars);  // 4 MB
    w.assets.finish();
    w.assets.release(stars);
    NX_EXPECT(w.assets.stats().warm == 1);
    Asset* other = w.assets.texture("showcase/parallax/asset/stars2.png");  // another 4 MB
    w.assets.finish();
    NX_EXPECT(w.assets.stats().evictions == 1 && w.assets.stats().warm == 0);
    // Both in use is over budget, but neither may be taken away.
    Asset* back = w.assets.texture(kStars);
    w.assets.finish();
    NX_EXPECT(!back->failed && !other->failed && w.assets.stats().inUse == 2);
    NX_EXPECT(w.assets.stats().thrash == 1);
    w.assets.release(back);
    w.assets.release(other);
    NX_EXPECT(w.assets.stats().inUseBytes + w.assets.stats().warmBytes <= w.assets.budget(AssetType::Texture));
}

NX_TEST(badAssetsBecomePlaceholders)
{
    Rig w;
    for (const char* path : { "missing.png", "../../secret.png", "c:/windows/win.ini", "", "showcase/main/main.as" })
    {
        Asset* a = w.assets.texture(path);
        w.assets.finish();
        NX_EXPECT(a->failed && a->texture);
        const int reads = w.platform.reads;
        Asset* again = w.assets.texture(path);
        NX_EXPECT(again == a && w.platform.reads == reads);  // remembered, not looked for again
        w.assets.release(again);
        w.assets.release(a);
    }
    Asset* font = w.assets.font("showcase/main/main.as", 20);
    NX_EXPECT(font->failed);
    w.device->beginFrame({});
    w.canvas.begin();
    NX_EXPECT(w.canvas.text(*font, "nothing", 0, 0, {}) == 0);
    w.canvas.end();
    w.device->endFrame();
    w.assets.release(font);
}

NX_TEST(damagedFilesDontCrash)
{
    TestPlatform platform;
    for (const char* path : { kShip, "global/freesans.sfn" })
    {
        FileData file;
        NX_EXPECT(platform.readFile(path, file, MemTag::General));
        u32 random = 12345;
        for (int round = 0; round < 60; round++)
        {
            u8* copy = static_cast<u8*>(memAlloc(file.size + 1, MemTag::General));
            memcpy(copy, file.data, file.size + 1);
            for (int i = 0; i < 8; i++)
            {
                random = random * 1664525 + 1013904223;
                // Image damage only: SSFN trusts its file, so fonts are cut short and no more.
                if (path == kShip)
                {
                    copy[random % file.size] = static_cast<u8>(random >> 24);
                }
            }
            const size_t size = round % 3 == 0 ? random % file.size : file.size;
            if (path == kShip)
            {
                u32 width, height;
                if (imageSize(copy, size, width, height) && gpu::validTextureSize(width, height))
                {
                    if (u8* pixels = decodeImage(copy, size, width, height))
                    {
                        freeImage(pixels);
                    }
                }
            }
            memFree(copy);
        }
    }
}

NX_TEST(fontsRender)
{
    Rig w;
    Asset* font = w.assets.font("global/freesans.sfn", 32);
    NX_EXPECT(!font->failed && font->font != nullptr);
    if (font->font)
    {
        const Vec2 one = w.canvas.measure(*font, "i");
        const Vec2 wide = w.canvas.measure(*font, "W");
        const Vec2 two = w.canvas.measure(*font, "WW\nW");
        NX_EXPECT(one.x > 2 && wide.x > one.x * 2);
        NX_EXPECT(two.x == wide.x * 2 && two.y == wide.y * 2);
        NX_EXPECT(w.canvas.measure(*font, "\xC3\xA9").x > 5);          // an accented letter
        NX_EXPECT(w.canvas.measure(*font, "\xC3").x > 0);              // cut short
        NX_EXPECT(w.canvas.measure(*font, "\xF0\x9F\x98").x > 0);      // cut short
        NX_EXPECT(w.canvas.measure(*font, "\xFF\xFE\x80").x > 0);      // nonsense
        const Glyph& g = font->font->glyphs['A' - kFirstGlyph];
        NX_EXPECT(g.height > 10 && g.height < 32 && g.y < 0 && g.u1 > g.u0 && g.v1 > g.v0);
    }
    w.assets.release(font);
}

NX_TEST(richTextWrapsAsItsWordsDo)
{
    Rig w;
    Asset* font = w.assets.font("global/freesans.sfn", 24);
    if (font->font)
    {
        const char* plain = "Some words, some of them bold and some of them coloured, wrapped to a narrow width.";
        const char* marked = "Some words, [b]some of them bold[/b] and [color=#ff8000]some of [u]them[/u] coloured[/color], wrapped to a [s]narrow[/s] width.";
        for (const float width : { 120.0f, 260.0f, 2000.0f })
        {
            NX_EXPECT(w.canvas.richHeight(*font, marked, width) == w.canvas.paragraphHeight(*font, plain, width));
        }
        // [[ is a bracket; tags it doesn't know, and a colour it can't read, stay as they were.
        NX_EXPECT(w.canvas.richHeight(*font, "[[b]", 2000) == w.canvas.paragraphHeight(*font, "[b]", 2000));
        NX_EXPECT(w.canvas.richHeight(*font, "[x] [color=nope]", 50) == w.canvas.paragraphHeight(*font, "[x] [color=nope]", 50));
        w.device->beginFrame({});
        w.canvas.begin();
        // Drawn, the height is the end less the start, which may round a little differently.
        NX_EXPECT(fabsf(w.canvas.rich(*font, marked, 10, 10, 200, { 1, 1, 1, 1 }) - w.canvas.paragraphHeight(*font, plain, 200)) < 0.01f);
        w.canvas.rich(*font, "[b][u][color=red]unclosed", 10, 10, 200, { 1, 1, 1, 1 });
        w.canvas.end();
        w.device->endFrame();
    }
    w.assets.release(font);
}

NX_TEST(picturesAreDrawnInto)
{
    Rig w;
    const size_t before = w.device->stats().textureBytes;
    Asset* picture = w.assets.target(64, 32);
    NX_EXPECT(picture && picture->ready && picture->width == 64 && w.device->stats().textureBytes > before);
    if (picture)
    {
        w.device->beginFrame({});
        w.canvas.begin();
        w.canvas.design(1280, 720);
        w.canvas.push();
        w.canvas.translate(100, 50);
        const Vec2 at = w.canvas.toDesign(500, 300);
        NX_EXPECT(w.canvas.beginTarget(*picture, { 0, 0, 0, 0 }));
        // Its own pixels, and nothing of the screen's.
        NX_EXPECT(w.canvas.width() == 64 && w.canvas.height() == 32 && w.canvas.inTarget());
        u32 width = 0, height = 0;
        w.device->size(width, height);
        NX_EXPECT(width == 64 && height == 32);
        w.canvas.rect(0, 0, 64, 32, { 1, 0, 0, 1 });
        NX_EXPECT(!w.canvas.beginTarget(*picture, { 0, 0, 0, 0 }));   // one at a time
        NX_EXPECT(!w.canvas.bloom(1, 0.5f, 1));                          // the screen's only
        w.canvas.endTarget();
        // Back as it was.
        const Vec2 again = w.canvas.toDesign(500, 300);
        NX_EXPECT(!w.canvas.inTarget() && w.canvas.width() == 1280 && w.canvas.height() == 720 && again.x == at.x && again.y == at.y);
        w.device->size(width, height);
        NX_EXPECT(width == 1280 && height == 720);
        w.canvas.image(*picture, 10, 10, 64, 32, { 1, 1, 1, 1 });
        w.canvas.pop();
        // One left open is closed at the end.
        NX_EXPECT(w.canvas.beginTarget(*picture, { 0, 0, 0, 1 }));
        w.canvas.end();
        NX_EXPECT(!w.canvas.inTarget());
        w.device->endFrame();
        w.assets.release(picture);
        NX_EXPECT(w.device->stats().textureBytes == before);           // gone when let go: nobody else can ask for it
    }
    NX_EXPECT(w.assets.target(0, 10) == nullptr);
}

NX_TEST(thinLettersAreNeverLost)
{
    // Drawn smaller than it was made, anything narrower than a pixel could fall between two.
    Rig w;
    for (const float scale : { 2.0f / 3, 1.0f, 1.5f })
    {
        w.assets.setFontScale(scale);
        for (const u32 size : { 16u, 18u, 22u, 24u })
        {
            Asset* font = w.assets.font("global/freesans.sfn", size);
            NX_EXPECT(font->font != nullptr);
            for (const char c : { 'i', 'l', 'I', '.', '|' })
            {
                NX_EXPECT(font->font->glyphs[c - kFirstGlyph].width * font->font->scale * scale > 1);
            }
            w.assets.release(font);
        }
    }
}

NX_TEST(canvasBatches)
{
    Rig w;
    Asset* ship = w.assets.texture(kShip);
    Asset* stars = w.assets.texture(kStars);
    w.assets.finish();
    w.device->beginFrame({});
    w.canvas.begin();
    w.canvas.design(1280, 720);
    for (int i = 0; i < 500; i++)
    {
        w.canvas.push();
        w.canvas.rotate(static_cast<float>(i));
        w.canvas.image(*ship, 0, 0, 10, 10, { 1, 0, 0, 1 });
        w.canvas.pop();
    }
    w.canvas.end();
    NX_EXPECT(w.canvas.draws() == 1);
    w.device->endFrame();
    NX_EXPECT(w.device->stats().vertices == 3000);

    w.device->beginFrame({});
    w.canvas.begin();
    w.canvas.image(*ship, 0, 0, 10, 10, {});
    w.canvas.image(*stars, 0, 0, 10, 10, {});
    w.canvas.image(*ship, 0, 0, 10, 10, {});
    w.canvas.setBlend(gpu::Blend::Additive);
    w.canvas.image(*ship, 0, 0, 10, 10, {});
    w.canvas.clip(0, 0, 5, 5);
    w.canvas.image(*ship, 0, 0, 10, 10, {});
    w.canvas.end();
    w.device->endFrame();
    NX_EXPECT(w.canvas.draws() == 5 && w.device->stats().rejected == 0);

    // Far too much in one frame is left out, not written past the end.
    w.device->beginFrame({});
    w.canvas.begin();
    for (int i = 0; i < 40000; i++)
    {
        w.canvas.rect(0, 0, 1, 1, {});
    }
    for (int i = 0; i < 1000; i++)
    {
        w.canvas.push();
    }
    for (int i = 0; i < 1100; i++)
    {
        w.canvas.pop();
    }
    w.canvas.end();
    w.device->endFrame();
    NX_EXPECT(w.device->stats().vertices <= gpu::kStreamVertices && w.device->stats().rejected == 0);
    w.assets.release(ship);
    w.assets.release(stars);
}

NX_TEST(designFitsAnyScreen)
{
    Rig w;
    w.canvas.design(640, 480);  // on 1280 by 720: scale 1.5, with bars at the sides
    const Vec2 middle = w.canvas.toDesign(640, 360);
    NX_EXPECT(middle.x == 320 && middle.y == 240);
    const Vec2 corner = w.canvas.toDesign(160, 0);
    NX_EXPECT(corner.x == 0 && corner.y == 0);
}

NX_TEST(stickMovesAPointer)
{
    Input input;
    input.setScreen(640, 480);
    InputState s;
    s.pads[0].connected = true;
    s.pads[0].leftX = 1;
    // Not unless the player chose it and the screen wants one: until then the stick moves.
    input.update(s, 1.0f / 60);
    NX_EXPECT(!input.stickPointing() && input.move().x > 0.9f);
    input.setStickPointer(true);
    input.setWantsPointer(true);
    for (int i = 0; i < 30; i++)
    {
        input.update(s, 1.0f / 60);
    }
    NX_EXPECT(input.stickPointing() && input.raw().hasMouse && input.raw().mouseX > 400 && input.raw().mouseX < 640 && input.move().x == 0);
    // A clicks, and is not also Accept.
    s.pads[0].leftX = 0;
    s.pads[0].buttons = 1u << static_cast<u32>(Button::A);
    input.update(s, 1.0f / 60);
    NX_EXPECT(input.raw().mouseButtons[0] && !input.held(Action::Accept));
    // Kept on the screen.
    s.pads[0].leftX = 1;
    for (int i = 0; i < 300; i++)
    {
        input.update(s, 1.0f / 60);
    }
    NX_EXPECT(input.raw().mouseX <= 639);
    // A mouse that moves takes over.
    s.pads[0].leftX = 0;
    s.hasMouse = true;
    s.mouseX = 100;
    s.mouseY = 50;
    input.update(s, 1.0f / 60);
    NX_EXPECT(input.raw().mouseX == 100 && input.raw().mouseY == 50);
    input.setWantsPointer(false);
    s.pads[0].leftX = 1;
    input.update(s, 1.0f / 60);
    NX_EXPECT(!input.stickPointing() && input.move().x > 0.9f);
}

NX_TEST(inputRumble)
{
    Input input;
    InputState s;
    s.pads[0].connected = s.pads[1].connected = true;
    input.update(s, 1.0f / 60);
    float heavy = -1, light = -1;
    NX_EXPECT(!input.rumbleChanged(0, heavy, light));   // still, and the platform already knows
    input.rumble(-1, 0.8f, 0.3f, 0.5f);
    NX_EXPECT(input.rumbleChanged(0, heavy, light) && heavy == 0.8f && light == 0.3f);
    NX_EXPECT(!input.rumbleChanged(0, heavy, light));   // told once
    NX_EXPECT(input.rumbleChanged(1, heavy, light) && heavy == 0.8f);
    NX_EXPECT(!input.rumbleChanged(2, heavy, light));   // nothing there
    input.rumble(1, 2, -1, 1);                          // kept within 0 to 1
    NX_EXPECT(input.rumbleChanged(1, heavy, light) && heavy == 1 && light == 0);
    for (int i = 0; i < 40; i++)
    {
        input.update(s, 1.0f / 60);
    }
    NX_EXPECT(input.rumbleChanged(0, heavy, light) && heavy == 0 && light == 0);   // its half second is over
    NX_EXPECT(!input.rumbleChanged(1, heavy, light));                              // a second, not over yet
    input.setRumbleOn(false);
    NX_EXPECT(input.rumbleChanged(1, heavy, light) && heavy == 0);                 // the player said no
    input.setRumbleOn(true);
    input.stopRumble();
    NX_EXPECT(!input.rumbleChanged(1, heavy, light));
}

NX_TEST(inputActions)
{
    Input input;
    InputState s;
    s.keys[static_cast<size_t>(Key::Up)] = true;
    input.update(s, 1.0f / 60);
    NX_EXPECT(input.pressed(Action::Up) && input.held(Action::Up) && input.repeated(Action::Up) && input.move().y == -1);
    input.update(s, 1.0f / 60);
    NX_EXPECT(!input.pressed(Action::Up) && input.held(Action::Up) && !input.repeated(Action::Up));
    int repeats = 0;
    for (int i = 0; i < 60; i++)
    {
        input.update(s, 1.0f / 60);
        repeats += input.repeated(Action::Up);
    }
    NX_EXPECT(repeats >= 4 && repeats <= 6);
    input.consume();
    input.update(s, 1.0f / 60);
    NX_EXPECT(!input.held(Action::Up));  // still down, so still ignored
    s = {};
    input.update(s, 1.0f / 60);
    s.keys[static_cast<size_t>(Key::Up)] = true;
    input.update(s, 1.0f / 60);
    NX_EXPECT(input.pressed(Action::Up));

    // Holding Back reaches the menu; tapping it doesn't.
    s = {};
    s.pads[2].connected = true;
    s.pads[2].buttons = 1u << static_cast<u32>(Button::Back);
    for (int i = 0; i < 20; i++)
    {
        input.update(s, 1.0f / 60);
    }
    NX_EXPECT(!input.held(Action::Menu));
    for (int i = 0; i < 20; i++)
    {
        input.update(s, 1.0f / 60);
    }
    NX_EXPECT(input.held(Action::Menu) && input.usingPad());

    s = {};
    s.keys[static_cast<size_t>(Key::Right)] = s.keys[static_cast<size_t>(Key::Down)] = true;
    input.update(s, 1.0f / 60);
    NX_EXPECT(input.move().dot(input.move()) < 1.0001f);  // diagonals are no faster
    Action a;
    NX_EXPECT(Input::parse("fire", a) && a == Action::Fire && !Input::parse("nonsense", a));
}

NX_TEST(jsonReadsWhatItShouldAndNothingElse)
{
    const char* text = "{ \"skip\": { \"title\": [1, {\"x\": \"}\"}, true] }, \"title\": \"A \\\"b\\\" \\u00e9\\n\", \"order\": -12.5 }";
    char out[32];
    double number = 0;
    NX_EXPECT(jsonString(text, "title", out, sizeof(out)) && strcmp(out, "A \"b\" \xC3\xA9\n") == 0);
    NX_EXPECT(jsonNumber(text, "order", number) && number == -12.5);
    NX_EXPECT(!jsonString(text, "order", out, sizeof(out)) && !jsonNumber(text, "title", number) && !jsonString(text, "none", out, sizeof(out)));
    NX_EXPECT(jsonString(text, "title", out, 4) && strcmp(out, "A \"") == 0);
    for (const char* bad : { "", "{", "{\"a\"", "{\"a\":", "{\"a\":\"", "{\"a\":\"\\", "{\"a\":\"\\u12", "[1]", "{\"b\":[[[[,\"a\":\"x\"}", "{\"b\":tru,\"a\":\"x\"}" })
    {
        NX_EXPECT(!jsonString(bad, "a", out, sizeof(out)));
    }
    std::string deep = "{\"b\":";
    deep.append(100000, '[');
    NX_EXPECT(!jsonString(deep.c_str(), "a", out, sizeof(out)));
}

NX_TEST(everyScriptCallIsDescribed)
{
    Rig w;
    NX_EXPECT(w.ok && w.documented > 100);
}

NX_TEST(screensOpenAndComeBack)
{
    Rig w;
    NX_EXPECT(w.screens.init(w.services, "showcase", "main"));
    w.step(5);
    NX_EXPECT(w.screens.depth() == 1 && !w.screens.failed());

    // As a player would: open the chosen tile, then come back through the menu.
    w.press(Action::Accept);
    NX_EXPECT(w.screens.depth() == 2 && strcmp(w.screens.current(), "shapes") == 0 && !w.screens.failed());
    w.step(5);
    w.press(Action::Menu);
    NX_EXPECT(w.screens.menuOpen());
    w.press(Action::Down);
    w.press(Action::Down);
    w.press(Action::Accept);
    NX_EXPECT(w.screens.depth() == 1 && !w.screens.menuOpen() && strcmp(w.screens.current(), "main") == 0);

    // Resume goes back to where it was.
    w.press(Action::Accept);
    w.press(Action::Menu);
    w.press(Action::Accept);
    NX_EXPECT(w.screens.depth() == 2 && !w.screens.menuOpen());

    // From home, the menu's last item quits.
    w.screens.home();
    w.step();
    w.press(Action::Menu);
    w.press(Action::Up);
    w.press(Action::Accept);
    NX_EXPECT(w.screens.quitRequested());
}

NX_TEST(brokenScreensDontStopTheApp)
{
    Rig w;
    NX_EXPECT(w.screens.init(w.services, "showcase", "main"));
    for (int fault = 0; fault < 4; fault++)
    {
        w.screens.push("faulty");
        w.step(2);
        for (int i = 0; i < fault; i++)
        {
            w.press(Action::Down);
        }
        NX_EXPECT(!w.screens.failed());
        w.press(Action::Accept);
        NX_EXPECT(w.screens.failed() && strstr(w.screens.error(), "faulty/main.as:") != nullptr);
        if (fault == 0)
        {
            w.press(Action::Accept);  // Try again
            NX_EXPECT(!w.screens.failed() && w.screens.depth() == 2);
            w.screens.home();
        }
        else
        {
            w.press(Action::Down);
            w.press(Action::Accept);  // Main menu
        }
        w.step(2);
        NX_EXPECT(w.screens.depth() == 1 && !w.screens.failed());
    }

    const struct { const char* code; const char* error; } cases[] = {
        { "class A : Screen { void update(float dt) { }", "compile" },
        { "void nothing() {}", "implements Screen" },
        { "class A : Screen { A() { int z = 0; z = 1 / z; } }", "Divide by zero" },
        { "int z = 0; int bad = 1 / z; class A : Screen {}", "Divide by zero" },
        { "int forever() { while (true) {} return 0; } int bad = forever(); class A : Screen {}", "global variable" },
        { "class A : Screen { void onEnter() { while (true) {} } }", "endless loop" },
        { "class A : Screen { void onEnter() { Assets::texture(\"../x.png\"); } }", "asset:" },
        { "class A : Screen { void update(float dt) { Input::held(\"jump\"); } }", "no action called jump" },
        { "class A : Screen { void draw(Canvas@ c) { Texture@ t; c.image(t, 0, 0); } }", "null texture" },
    };
    for (const auto& c : cases)
    {
        w.platform.extraPath = "showcase/test/main.as";
        w.platform.extraText = c.code;
        const u32 known = w.screens.count();
        NX_EXPECT(known == 4);  // "test" has no screen.json, so it isn't offered
        w.screens.push("shapes");
        w.platform.extraPath = "showcase/shapes/main.as";
        w.step(3);
        NX_EXPECT(w.screens.failed() && strstr(w.screens.error(), c.error) != nullptr);
        if (!w.screens.failed() || strstr(w.screens.error(), c.error) == nullptr)
        {
            NX_ERROR("wanted '%s', got '%s'", c.error, w.screens.error());
        }
        w.screens.home();
        w.step(2);
        NX_EXPECT(w.screens.depth() == 1);
    }
}

NX_TEST(screensLeaveNothingBehind)
{
    Rig w;
    NX_EXPECT(w.screens.init(w.services, "showcase", "main"));
    w.step(5);
    size_t settled = 0;
    u32 loads = 0;
    for (int i = 0; i < 200; i++)
    {
        w.screens.push(i % 2 ? "shapes" : "blend");
        w.step(4);
        NX_EXPECT(!w.screens.failed());
        w.screens.home();
        w.step(2);
        if (i == 100)
        {
            settled = memStats(MemTag::Script).bytes;
            loads = w.assets.stats().loads;
        }
    }
    NX_EXPECT(memStats(MemTag::Script).bytes <= settled + 1024);
    NX_EXPECT(w.assets.stats().loads == loads);  // nothing read twice
}

NX_TEST(everythingIsGivenBack)
{
    const size_t before = memStatsTotal().count;
    {
        Rig w;
        w.screens.init(w.services, "showcase", "main");
        w.step(10);
        w.screens.push("blend");
        w.step(10);
    }
    NX_EXPECT(memStatsTotal().count == before);
}
