#include "test.h"

#include "base/thread.h"
#include "base/json_value.h"
#include "rig.h"

#include <string.h>

using namespace nx;
using namespace nx::test;

namespace
{
    struct Held
    {
        JsonValue* v = create<JsonValue>(MemTag::Script);
        ~Held() { v->release(); }
        JsonValue* operator->() const { return v; }

        bool parse(const char* text)
        {
            std::string error;
            return v->parse(text, strlen(text), error);
        }
    };

    void open(Rig& r, const char* code)
    {
        r.platform.extraPath = "showcase/shapes/main.as";
        r.platform.extraText = code;
        r.screens.init(r.services, "showcase", "main");
        r.step(2);
        r.screens.push("shapes");
        r.step(2);
        if (r.screens.failed())
        {
            NX_ERROR("%s", r.screens.error());
        }
    }
}

NX_TEST(jsonComesBackAsItWas)
{
    Held j;
    NX_EXPECT(j.parse("{\"a\": [1, 2.5, -0.1], \"b\": {\"c\": \"x\\\"\\n\\\\\", \"d\": true, \"e\": null}, \"f\": 12345678901}"));
    const std::string one = j->write(true);
    NX_EXPECT(one == "{\"a\": [1, 2.5, -0.1], \"b\": {\"c\": \"x\\\"\\n\\\\\", \"d\": true, \"e\": null}, \"f\": 12345678901}");
    const std::string lines = j->write(false);
    NX_EXPECT(lines.find("{\n  \"a\": [1, 2.5, -0.1],\n  \"b\": {") == 0 && lines.back() == '\n');
    Held again;
    NX_EXPECT(again.parse(lines.c_str()) && again->write(true) == one);
    std::string error;
    NX_EXPECT(!again->parse("{\n\"a\" 1}", 8, error) && error.find("line 2") == 0 && again->type() == JsonValue::Type::Null);
    // A float is as short as it was.
    Held n;
    n->setNumber(static_cast<double>(0.1f));
    NX_EXPECT(n->write(true) == "0.1");
    n->setNumber(0.0 / (n->count() ? 1.0 : 0.0));
    NX_EXPECT(n->write(true) == "0");
}

NX_TEST(jsonPartsAreCopiedAndKept)
{
    Held j;
    j.parse("{\"a\": {\"b\": [1, 2]}}");
    // Into itself: as it was.
    JsonValue* inside = j->find("a")->put("all");
    NX_EXPECT(inside->setCopy(*j.v));
    NX_EXPECT(j->write(true) == "{\"a\": {\"b\": [1, 2], \"all\": {\"a\": {\"b\": [1, 2], \"all\": null}}}}");
    // A part outlives what it was in.
    JsonValue* part = j->find("a")->find("b");
    part->addRef();
    j->setNumber(3);
    NX_EXPECT(part->write(true) == "[1, 2]");
    part->removeAt(0);
    part->removeAt(7);
    NX_EXPECT(part->count() == 1);
    part->release();
    // It can only go so deep.
    JsonValue* at = j.v;
    u32 depth = 0;
    while (JsonValue* next = at->put("x"))
    {
        at = next;
        depth++;
    }
    NX_EXPECT(depth == JsonValue::kMaxDepth);
    Held other;
    other.parse("[1]");
    NX_EXPECT(!at->setCopy(*other.v));
}

NX_TEST(scriptsHaveJson)
{
    Rig r;
    open(r, R"(
        class A : Screen
        {
            void onEnter()
            {
                Json j("{\"list\": [1, {\"x\": 2}], \"color\": \"#ff8000\", \"on\": true}");
                Json@ list = j["list"];
                string s = "" + int(list[0].number()) + int(list[1]["x"].number()) + list.count + j.key(2);
                s += j["no"]["such"][4].isNull ? "y" : "n";
                s += j["on"].flag() && j["color"].color().g > 0.4f ? "y" : "n";
                j.set("place", Vec3(1, 2.5f, 3));
                j.set("color", Color(1, 0, 0, 0.5f));
                j.set("name", "Ann");
                j.set("n", 7);
                j.remove("on");
                list.add("more");
                list.add(Json());
                list.remove(0);
                j.set("self", j);
                s += " " + j.write(true);
                string error;
                s += j.parse("{", error) ? "y" : error;
                Save::set("s", s);
            }
        }
    )");
    NX_EXPECT(!r.screens.failed());
    const std::string body = "\"list\": [{\"x\": 2}, \"more\", null], \"color\": \"#ff000080\", \"place\": [1, 2.5, 3], \"name\": \"Ann\", \"n\": 7";
    NX_EXPECT(r.save.get("s", "") == "122onyy {" + body + ", \"self\": {" + body + "}}line 1: expected a name in quotes");
}

NX_TEST(screensWriteTheirOwnFiles)
{
    Rig r;
    open(r, R"(
        class A : Screen
        {
            void onEnter()
            {
                Assets::write("asset:rooms/first.json", "{\"entities\": [{\"shape\": \"box\"}, {\"shape\": \"sphere\", \"place\": [1, 2, 3]}]}");
                Save::set("read", Assets::read("asset:rooms/first.json"));
                int made = Space::load("asset:rooms/first.json");
                Json thing;
                thing.set("shape", "cone");
                thing.set("place", Vec3(4, 5, 6));
                thing.set("name", "it");
                Entity@ e = Space::make(thing, Vec3(1, 0, 0));
                Save::set("made", "" + made + " " + int(e.place.x) + e.name);
            }
        }
    )");
    NX_EXPECT(!r.screens.failed());
    NX_EXPECT(strstr(r.save.get("read", ""), "sphere") != nullptr && strcmp(r.save.get("made", ""), "2 5it") == 0);
    NX_EXPECT(r.platform.kept.count("shapes.rooms.first") == 1 && r.platform.written.empty());

    // And back to what came with it.
    Rig f;
    open(f, "class A : Screen { void onEnter() { Assets::write(\"asset:first.json\", \"[]\"); Assets::write(\"asset:other.json\", \"[]\"); Assets::forget(\"asset:first.json\"); Assets::forget(\"asset:never.json\"); } }");
    NX_EXPECT(!f.screens.failed() && f.platform.kept.count("shapes.first") == 0 && f.platform.kept.count("shapes.other") == 1);
    Rig no;
    open(no, "class A : Screen { void onEnter() { Assets::forget(\"global:a.json\"); } }");
    NX_EXPECT(no.screens.failed());

    // While developing, it is the file itself.
    Rig d;
    d.services.development = true;
    open(d, "class A : Screen { void onEnter() { Save::set(\"ok\", Assets::write(\"asset:first.json\", \"[]\") ? \"y\" : \"n\"); } }");
    NX_EXPECT(strcmp(d.save.get("ok", ""), "y") == 0 && d.platform.written == "showcase/shapes/asset/first.json" && d.platform.kept.empty());

    // Nothing but that.
    for (const char* name : { "global:a.json", "asset:main.as", "asset:../../a.json", "asset:a b.json", "asset:.json", "asset:aVeryLongNameThatNoXboxCouldEverKeep.json", "C:/a.json", "" })
    {
        Rig bad;
        open(bad, (std::string("class A : Screen { void onEnter() { Assets::write(\"") + name + "\", \"x\"); } }").c_str());
        NX_EXPECT(bad.screens.failed() && bad.platform.kept.empty() && bad.platform.written.empty());
    }

    // Nothing at all, unattended.
    Rig u;
    u.save.setReadOnly();
    open(u, "class A : Screen { void onEnter() { Assets::write(\"asset:first.json\", \"[]\"); } }");
    NX_EXPECT(!u.screens.failed() && u.platform.kept.empty());
}

NX_TEST(aThingThatIsWrongSaysSo)
{
    Rig r;
    open(r, "class A : Screen { void onEnter() { Json j; j.set(\"shape\", \"hat\"); Space::make(j); } }");
    NX_EXPECT(r.screens.failed() && strstr(r.screens.error(), "isn't a shape"));
}

NX_TEST(playersBringFiles)
{
    Rig r;
    r.platform.dropped = "chair.obj";
    open(r, "class A : Screen { void onEnter() { array<string>@ l = Assets::list(\"own:\"); array<string>@ d = Assets::dropped(); array<string>@ none = Assets::dropped();"
            "Save::set(\"s\", l[0] + l[1] + d[0] + none.length() + Assets::list(\"asset:model/\").length()); } }");
    NX_EXPECT(!r.screens.failed() && strcmp(r.save.get("s", ""), "a.objb.stlchair.obj00") == 0);
    for (const char* name : { "own:../a", "C:/", "a" })
    {
        Rig bad;
        open(bad, (std::string("class A : Screen { void onEnter() { Assets::list(\"") + name + "\"); } }").c_str());
        NX_EXPECT(bad.screens.failed());
    }
}

NX_TEST(screensIncludeOtherFiles)
{
    Rig r;
    r.platform.files["global/lib.as"] = "#include \"global:more.as\"\nint twice(int v) { return more(v) * 2; }\n";
    r.platform.files["global/more.as"] = "int more(int v) { return v + 1; }\n";
    open(r, "#include \"global:lib.as\"\n  #include \"global:more.as\"\nclass A : Screen { void onEnter() { Save::set(\"t\", \"\" + twice(4)); } }");
    NX_EXPECT(!r.screens.failed() && strcmp(r.save.get("t", ""), "10") == 0);
    Rig bad;
    open(bad, "#include \"global:none.as\"\nclass A : Screen {}");
    NX_EXPECT(bad.screens.failed() && strstr(bad.screens.error(), "none.as"));
}


NX_TEST(scriptsHavePixelsDataAndRandom)
{
    Rig r;
    open(r, R"(
        class A : Screen
        {
            // A mistake stops the screen, which the test sees.
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            bool near(float a, float b) { return abs(a - b) < 0.01f; }
            void onEnter()
            {
                // Pixels: made, changed, read, pasted, kept as a PNG and read back.
                Pixels p(4, 3, Color(1, 0, 0));
                check(p.width == 4 && p.height == 3 && near(p.get(3, 2).r, 1) && near(p.get(9, 9).a, 0));
                p.set(1, 1, Color(0, 0.5f, 1, 0.5f));
                Color c = p.get(1, 1);
                check(near(c.g, 0.5f) && near(c.b, 1) && near(c.a, 0.5f));
                Pixels small(2, 2, Color(0, 1, 0));
                p.paste(small, 3, -1);
                check(near(p.get(3, 0).g, 1) && near(p.get(2, 0).r, 1) && near(p.get(3, 1).r, 1));
                check(p.texture !is null && p.png().substr(1, 3) == "PNG");
                check(p.save("saved:shot.png"));
                Pixels@ back = Assets::pixels("saved:shot.png");
                check(back !is null && back.width == 4 && near(back.get(1, 1).b, 1) && near(back.get(3, 0).g, 1));
                check(Assets::pixels("saved:none.png") is null);
                Pixels@ bricks = Assets::pixels("global:picture/bricks.png");
                check(bricks !is null && bricks.width > 0 && bricks.copy().width == bricks.width);

                // Bytes.
                string text = "the same words over and over, the same words over and over, the same words";
                string squeezed = Data::compress(text);
                check(squeezed.length() < text.length() && Data::uncompress(squeezed) == text);
                check(Data::md5("abc") == "900150983cd24fb0d6963f7d28e17f72");
                check(Data::sha1("abc") == "a9993e364706816aba3e25717850c26c9cd0d89d");
                check(Data::sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
                check(Data::base64("hello") == "aGVsbG8=" && Data::unbase64("aGVsbG8=") == "hello" && Data::unbase64(Data::base64(squeezed)) == squeezed);
                check(Data::hex("\x01\xff") == "01ff" && Data::unhex("01FF") == "\x01\xff");

                // Random numbers from a seed: the same each time.
                Random a(42), b(42);
                for (int i = 0; i < 50; i++)
                    check(a.randomInt(0, 1000) == b.randomInt(0, 1000));
                string kept = a.state;
                float next = a.random();
                b.state = kept;
                check(b.random() == next && a.seed == 42);
                float total = 0, spread = 0;
                for (int i = 0; i < 4000; i++)
                {
                    float v = a.normal(10, 2);
                    total += v;
                    spread += (v - 10) * (v - 10);
                }
                check(abs(total / 4000 - 10) < 0.2f && abs(sqrt(spread / 4000) - 2) < 0.2f);
                int low = 99, high = -99;
                for (int i = 0; i < 500; i++)
                {
                    int v = a.randomInt(-2, 2);
                    if (v < low) low = v;
                    if (v > high) high = v;
                }
                check(low == -2 && high == 2);
                Save::set("done", "y");
            }
        }
    )");
    NX_EXPECT(!r.screens.failed() && strcmp(r.save.get("done", ""), "y") == 0);
    NX_EXPECT(r.platform.kept.count("shapes.shot.png") == 1);

    // What can't be.
    for (const char* code : { "Data::uncompress(\"not that\");", "Data::unbase64(\"*\");", "Data::unhex(\"0g\");", "Pixels p(0, 4);", "Pixels p(4, 4); p.save(\"own:a.png\");",
             "Pixels p(4, 4); p.save(\"saved:../a.png\");" })
    {
        Rig bad;
        open(bad, (std::string("class A : Screen { void onEnter() { ") + code + " } }").c_str());
        NX_EXPECT(bad.screens.failed());
    }
}

NX_TEST(scriptsHaveCurvesBatchesFontsAndZips)
{
    Rig r;
    open(r, R"(
        class A : Screen
        {
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            bool near(float a, float b, float within = 0.01f) { return abs(a - b) < within; }
            bool near(const Vec2 &in a, const Vec2 &in b, float within = 0.01f) { return near(a.x, b.x, within) && near(a.y, b.y, within); }
            Batch@ stars;
            Font@ pixel, chunky;
            void onEnter()
            {
                // A cubic curve: its ends, its middle, its slope, its length, and a part of it.
                array<Vec2> controls = { Vec2(0, 0), Vec2(0, 100), Vec2(100, 100), Vec2(100, 0) };
                Curve c(controls);
                check(c.count == 4 && c.degree == 3);
                check(near(c.at(0), Vec2(0, 0)) && near(c.at(1), Vec2(100, 0)) && near(c.at(0.5f), Vec2(50, 75)));
                check(near(c.slope(0), Vec2(0, 300)) && near(c.slope(0.5f).normalized(), Vec2(1, 0)));
                array<Vec2>@ along = c.points(11);
                check(along.length() == 11 && near(along[5], c.at(0.5f)));
                check(near(c.length(), 200, 0.5f));
                Curve@ half = c.part(0.25f, 0.75f);
                check(half.count == 4 && near(half.at(0), c.at(0.25f), 0.05f) && near(half.at(1), c.at(0.75f), 0.05f) && near(half.at(0.5f), c.at(0.5f), 0.05f));
                c.insert(1, Vec2(-50, 0));
                c.remove(1);
                check(c.count == 4 && near(c.point(1), Vec2(0, 100)));

                // Polygons cut into triangles, and whether they bulge.
                array<Vec2> square = { Vec2(0, 0), Vec2(10, 0), Vec2(10, 10), Vec2(0, 10) };
                array<Vec2> ell = { Vec2(0, 0), Vec2(20, 0), Vec2(20, 10), Vec2(10, 10), Vec2(10, 20), Vec2(0, 20) };
                check(triangulate(square).length() == 6 && triangulate(ell).length() == 12);
                check(isConvex(square) && !isConvex(ell));
                float area = 0;
                array<uint>@ t = triangulate(ell);
                for (uint i = 0; i < t.length(); i += 3)
                    area += abs((ell[t[i + 1]] - ell[t[i]]).cross(ell[t[i + 2]] - ell[t[i]])) / 2;
                check(near(area, 300));

                // Vec2 and colours.
                check(near(Vec2(1, 0).rotated(90), Vec2(0, 1)) && near(Vec2(0, 1).angle(), 90) && near(Vec2(3, 4).length(), 5));
                check(near(Vec2(1, 1).distance(Vec2(4, 5)), 5) && near(Vec2(1, 0).perpendicular(), Vec2(0, 1)) && Vec2(1, 0).cross(Vec2(0, 1)) > 0);
                Color g = toLinear(Color(0.5f, 1, 0));
                check(near(g.r, 0.214f) && near(g.g, 1) && near(toGamma(g).r, 0.5f));

                // Squeezed, and checked.
                string text = "the same words over and over, the same words over and over, the same words";
                string gz = Data::gzip(text);
                check(gz.length() < text.length() && gz[0] == 0x1f && gz[1] == 0x8b && Data::gunzip(gz) == text);
                check(Data::inflate(Data::deflate(text)) == text && Data::crc32("123456789") == 0xCBF43926);

                // Packed numbers and text, either way round.
                Packed packed;
                packed.writeU8(250);
                packed.writeI16(-2);
                packed.writeU32(0xDEADBEEF);
                packed.writeFloat(1.5f);
                packed.writeDouble(-0.25);
                packed.writeText("name");
                packed.writeFixed("abc", 5);
                packed.writeI64(-5000000000);
                check(packed.size == 1 + 2 + 4 + 4 + 8 + 5 + 5 + 8 && packed.bytes[1] == 0xFE);
                Packed back(packed.bytes);
                check(back.readU8() == 250 && back.readI16() == -2 && back.readU32() == 0xDEADBEEF && back.readFloat() == 1.5f);
                check(back.readDouble() == -0.25 && back.readText() == "name" && back.readFixed(5) == "abc" && back.readI64() == -5000000000);
                check(!back.more && back.whole);
                back.readU8();
                check(!back.whole);
                Packed big;
                big.bigEndian = true;
                big.writeU16(0x1234);
                check(big.bytes[0] == 0x12 && big.bytes[1] == 0x34);

                // A zip made, read back, and a picture from it.
                Zip z;
                z.add("words.txt", text);
                z.add("levels/one.json", "{}", 0);
                Pixels p(3, 2, Color(0, 1, 0));
                z.add("dot.png", p.png());
                string file = z.bytes;
                Zip opened(file);
                check(!opened.failed && opened.count == 3 && opened.has("levels/one.json") && opened.size("words.txt") == text.length());
                check(opened.read("words.txt") == text && opened.read("levels/one.json") == "{}" && opened.name(0) == "words.txt");
                Pixels@ dot = Assets::pixelsFrom(opened.read("dot.png"));
                check(dot !is null && dot.width == 3 && near(dot.get(2, 1).g, 1));
                check(Assets::pixelsFrom("not a picture") is null && Zip("not a zip").failed);
                opened.remove("dot.png");
                check(Zip(opened.bytes).count == 2);

                // A batch of sprites; fonts drawn beforehand.
                @stars = Batch(Assets::texture("global:picture/bricks.png"));
                for (uint i = 0; i < 1000; i++)
                    stars.addFrame(4, 4, i, i % 40 * 20, i / 40 * 20, 16, 16, Color(1, 1, 1, 0.5f), i);
                uint last = stars.addPart(0, 0, 8, 8, 0, 0, 4, 4);
                stars.move(last, 5, 6, 45);
                stars.color(last, Red);
                check(stars.count == 1001 && near(stars.place(last), Vec2(5, 6)));
                stars.removeLast();
                check(stars.count == 1000 && stars.texture !is null);
                @pixel = Assets::imageFont("global:font/pixel.png", " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?:-+=\'()/%", 14);
                @chunky = Assets::font("global:font/chunky.fnt", 36);
                check(!pixel.failed && !chunky.failed);
                check(near(pixel.measure("I").y, 18) && near(pixel.measure("AB").x, 14 * (5 + 1 + 5 + 1) / 7.0f) && near(chunky.measure("A").y, 36));
                Save::set("done", "y");
            }
            void draw(Canvas@ c)
            {
                c.batch(stars, 10, 10);
                c.paragraph(pixel, "TWO LINES OF WORDS, SPREAD TO FILL THEIR WIDTH. AND A LAST ONE.", 10, 10, 200, White, AlignJustify);
                c.rich(chunky, "[color=red]JUSTIFIED[/color] TEXT, RICH TOO, OVER LINES", 10, 100, 200, White, AlignJustify);
            }
        }
    )");
    r.step(3);
    NX_EXPECT(!r.screens.failed() && strcmp(r.save.get("done", ""), "y") == 0);

    // What can't be.
    for (const char* code : { "Data::gunzip(\"not that\");", "Data::inflate(\"\");", "Zip z; z.read(\"none\");", "Curve c; for (int i = 0; i < 33; i++) c.add(Vec2());",
             "Batch b; b.move(3, 0, 0);", "Assets::imageFont(\"global:font/pixel.png\", \"\");" })
    {
        Rig bad;
        open(bad, (std::string("class A : Screen { void onEnter() { ") + code + " } }").c_str());
        NX_EXPECT(bad.screens.failed());
    }
}

NX_TEST(scriptsMakeSound)
{
    Rig r;
    open(r, R"(
        class A : Screen
        {
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            void onEnter()
            {
                // A tone made one sample at a time, and read back.
                Samples beep(4800, 48000, 1);
                for (uint i = 0; i < beep.frames; i++)
                    beep.set(i, sin(float(i) * 6.2831853f * 440 / 48000) * 0.5f);
                check(beep.channels == 1 && abs(beep.seconds - 0.1f) < 0.001f && abs(beep.get(12) - sin(12 * 6.2831853f * 440 / 48000) * 0.5f) < 0.001f);
                check(beep.get(99999) == 0);
                beep.play(0.5f);
                Samples stereo(10, 22050, 2);
                stereo.set(3, -0.5f, 0.25f);
                check(abs(stereo.get(3, 0) + 0.5f) < 0.001f && abs(stereo.get(3, 1) - 0.25f) < 0.001f);

                // A sound's own samples.
                Samples@ coin = Assets::samples("global:sound/coin.wav");
                check(coin !is null && coin.frames > 1000 && coin.rate > 0);
                check(Assets::samples("global:sound/none.wav") is null);

                // A stream takes as much as there is room for.
                AudioStream stream(48000, 2, 0.1f);
                check(stream.room == 4800 && stream.queued == 0 && stream.channels == 2);
                array<float> chunk(2000 * 2, 0.1f);
                check(stream.write(chunk) == 2000 && stream.queued == 2000);
                stream.write(chunk);
                check(stream.write(chunk) == 800 && stream.room == 0);
                stream.clear();
                stream.play(0.8f);

                // Every effect, and none.
                Audio::bandPass(BusEffects, 1000, 1);
                Audio::distortion(BusEffects, 0.5f);
                Audio::chorus(BusMusic, 0.5f);
                Audio::flanger(BusMusic, 0.5f, 0.3f, 0.6f, 1);
                Audio::compressor(BusInterface, -12, 4);
                Audio::equalizer(BusInterface, 3, -2, 6, 0.5f);
                Audio::clearEffects(0.2f);
                Save::set("done", "y");
            }
        }
    )");
    NX_EXPECT(!r.screens.failed() && strcmp(r.save.get("done", ""), "y") == 0);

    for (const char* code : { "Samples s(0);", "Samples s(10, 48000, 3);", "AudioStream s(48000, 5);", "Samples s(10, 10);" })
    {
        Rig bad;
        open(bad, (std::string("class A : Screen { void onEnter() { ") + code + " } }").c_str());
        NX_EXPECT(bad.screens.failed());
    }
}

NX_TEST(scriptsAskTheWindowAndSystem)
{
    Rig r;
    open(r, R"(
        class A : Screen
        {
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            void onEnter()
            {
                // No window to change here, and nothing goes wrong asking.
                check(!Window::available);
                Window::fullscreen = true;
                Window::title = "Changed";
                check(Window::title == "Changed" && !Window::resize(800, 600));
                Window::vsync = false;
                check(!Window::vsync);
                Window::vsync = true;
                Window::pointer = PointerHand;
                check(Window::pointer == PointerHand && Window::focused);
                check(!Window::message("Hello", "there") && !Window::ask("Well?", "Yes or no"));

                // A clipboard of the game's own, where the system has none.
                System::clipboard = "copied text";
                check(System::clipboard == "copied text");
                check(System::processors >= 1 && System::locale.length() >= 2 && System::language.findFirst("-") < 0);
                check(!System::open(""));
                check(System::battery >= -1);
                Save::set("done", "y");
            }
        }
    )");
    NX_EXPECT(!r.screens.failed() && strcmp(r.save.get("done", ""), "y") == 0);
}

NX_TEST(scriptsRunOnThreads)
{
    Rig r;
    r.platform.files["showcase/shapes/asset/square.as"] = R"(
        void main()
        {
            Channel jobs("jobs"), results("results");
            Json totals;
            while (!Worker::stopping)
            {
                string job = jobs.demand(5);
                if (job == "end" || job.isEmpty()) break;
                double n = parseFloat(job);
                results.push(formatFloat(n * n, "", 0, 0));
            }
            results.push("done " + Data::sha1("abc").substr(0, 6));
        }
    )";
    r.platform.files["showcase/shapes/asset/broken.as"] = "void main() { array<int> a; a[3] = 1; }";
    r.platform.files["showcase/shapes/asset/forever.as"] = "void main() { uint n = 0; while (true) { n++; if (n % 1000 == 0) Worker::sleep(0); } }";
    open(r, R"(
        class A : Screen
        {
            Thread@ squares = Thread("asset:square.as");
            Thread@ broken = Thread("asset:broken.as");
            Thread@ forever = Thread("asset:forever.as");
            Channel jobs("jobs"), results("results");
            array<string> got;
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            void onEnter()
            {
                check(!squares.started && squares.start() && squares.started);
                for (int i = 1; i <= 5; i++) jobs.push("" + i);
                jobs.push("end");
                broken.start();
                forever.start();
                check(!Worker::inThread);
            }
            void update(float dt)
            {
                string value;
                while (results.pop(value)) got.insertLast(value);
                if (got.length() == 6 && !squares.running && !broken.running)
                {
                    check(got[0] == "1" && got[1] == "4" && got[4] == "25" && got[5] == "done a9993e");
                    check(broken.error.findFirst("Index out of bounds") >= 0 && squares.error.isEmpty());
                    check(forever.running);
                    forever.stop();
                    forever.wait();
                    check(!forever.running && forever.error.isEmpty());
                    Save::set("done", "y");
                }
            }
        }
    )");
    for (int i = 0; i < 3000 && strcmp(r.save.get("done", ""), "y") != 0 && !r.screens.failed(); i++)
    {
        r.step();
        sleepMilliseconds(1);
    }
    NX_EXPECT(!r.screens.failed() && strcmp(r.save.get("done", ""), "y") == 0);

    // What can't be: no such file, no main(), a mistake in it, and the screen sleeping or waiting.
    r.platform.files["showcase/shapes/asset/nomain.as"] = "void other() {}";
    r.platform.files["showcase/shapes/asset/wrong.as"] = "void main() { nonsense(); }";
    for (const char* code : { "Thread t(\"asset:none.as\");", "Thread t(\"asset:nomain.as\");", "Thread t(\"asset:wrong.as\");", "Worker::sleep(1);", "Channel c(\"x\"); c.demand(1);" })
    {
        Rig bad;
        bad.platform.files = r.platform.files;
        open(bad, (std::string("class A : Screen { void onEnter() { ") + code + " } }").c_str());
        NX_EXPECT(bad.screens.failed());
    }
}
