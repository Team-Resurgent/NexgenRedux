#include "test.h"

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
