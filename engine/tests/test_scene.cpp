#include "test.h"
#include "rig.h"

#include "base/json_tree.h"
#include "scene/world.h"

#include <math.h>
#include <stdlib.h>

using namespace nx;
using namespace nx::test;

namespace
{
    // A scene without scripts.
    struct Scene
    {
        Rig rig;
        World world{ rig.assets, rig.host, "none", 4096, 500 };

        void step(int count = 1)
        {
            for (int i = 0; i < count; i++)
            {
                NX_EXPECT(world.update(1.0f / 60));
            }
        }

        bool load(const char* json)
        {
            rig.platform.extraPath = "scene.json";
            rig.platform.extraText = json;
            return world.load("scene.json", "showcase/shapes");
        }

        u32 rigid(Vec2 at, float w, float h, float mass)
        {
            const u32 id = world.spawn(at);
            Collider* c = world.collider(id);
            c->shape = Shape::Box;
            c->halfWidth = w * 0.5f;
            c->halfHeight = h * 0.5f;
            Body* b = world.body(id);
            b->mass = mass;
            b->gravity = 1;
            b->friction = 0.6f;
            return id;
        }

        u32 ground(float top)
        {
            const u32 id = world.spawn({ 0, top + 50 });
            Collider* c = world.collider(id);
            c->shape = Shape::Box;
            c->halfWidth = 2000;
            c->halfHeight = 50;
            c->wall = true;
            return id;
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

    bool saved(Rig& r, const char* key, const char* value)
    {
        const char* got = r.save.get(key, "(nothing)");
        if (strcmp(got, value) != 0)
        {
            NX_ERROR("%s is '%s', not '%s'", key, got, value);
            return false;
        }
        return true;
    }
}

NX_TEST(jsonTreesAreRead)
{
    JsonTree j;
    const char good[] = "{ // a comment\n \"a\": [1, -2.5e1, true, null, \"x\\n\\u00e9\\\"\"], \"b\": { \"c\": { \"d\": 7 } }, }";
    NX_EXPECT(j.parse(good, strlen(good)));
    const JsonTree::Value root = j.root();
    NX_EXPECT(root["a"].count() == 5 && root["a"][0u].number() == 1 && root["a"][1u].number() == -25 && root["a"][2u].flag());
    NX_EXPECT(!root["a"][3u] && root["a"][3u].exists() && !root["a"][9u].exists());
    NX_EXPECT(strcmp(root["a"][4u].text(), "x\n\xC3\xA9\"") == 0);
    NX_EXPECT(root["b"]["c"]["d"].number() == 7 && root["no"]["such"][3u].number(4) == 4 && root["b"].line() == 2);
    const char* const bad[] = { "", "{", "[1", "{\"a\"}", "{\"a\":}", "{a:1}", "[1 2]", "\"x", "\"\\q\"", "\"\\u12", "tru", "-", "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[", "{} x", "nul" };
    for (const char* text : bad)
    {
        JsonTree t;
        NX_EXPECT(!t.parse(text, strlen(text)) && t.error()[0]);
    }
    // Anything cut short or with a byte changed is refused or read, never worse.
    u32 random = 77;
    for (u32 i = 0; i < 2000; i++)
    {
        std::string text = good;
        random = random * 1664525 + 1013904223;
        text[(random >> 8) % text.size()] = static_cast<char>(random >> 24);
        text.resize(i % 2 ? text.size() : (random >> 4) % text.size());
        JsonTree t;
        t.parse(text.c_str(), text.size());
    }
}

NX_TEST(entitiesMoveWithTheirParents)
{
    Scene s;
    const u32 a = s.world.spawn({ 100, 100 });
    const u32 b = s.world.spawn({ 10, 0 });
    const u32 c = s.world.spawn({ 5, 0 });
    s.world.setParent(b, a);
    s.world.setParent(c, b);
    s.world.find(a)->rotation = 90;
    s.world.find(b)->scale = { 2, 2 };
    Vec2 p = s.world.worldPosition(c);
    NX_EXPECT(fabsf(p.x - 100) < 0.01f && fabsf(p.y - 120) < 0.01f);
    s.step();
    NX_EXPECT(fabsf(s.world.find(c)->world[4] - 100) < 0.01f && fabsf(s.world.find(c)->world[5] - 120) < 0.01f);
    // Nothing can be inside itself.
    s.world.setParent(a, c);
    NX_EXPECT(s.world.parentOf(a) == 0 && s.world.parentOf(c) == b);
    // Destroying one takes its children.
    s.world.destroyEntity(b);
    NX_EXPECT(!s.world.alive(b) && s.world.count() == 3);
    s.step();
    NX_EXPECT(s.world.count() == 1 && !s.world.alive(c) && s.world.alive(a));
    // A long chain doesn't use up the stack.
    u32 last = a;
    for (int i = 0; i < 3000; i++)
    {
        const u32 next = s.world.spawn({ 0.1f, 0 });
        s.world.setParent(next, last);
        last = next;
    }
    s.step();
    s.world.destroyEntity(a);
    s.step();
    NX_EXPECT(s.world.count() == 0);
}

NX_TEST(entityIdsAreNeverMixedUp)
{
    Scene s;
    const u32 first = s.world.spawn({});
    EntityRef* ref = s.world.ref(first);
    EntityRef* same = s.world.ref(first);
    NX_EXPECT(ref == same);
    same->release();
    s.world.destroyEntity(first);
    NX_EXPECT(ref->id == first && !s.world.alive(first));
    s.step();
    NX_EXPECT(ref->id == 0);
    const u32 second = s.world.spawn({});
    NX_EXPECT((second & 0xFFFF) == (first & 0xFFFF) && second != first && !s.world.alive(first) && s.world.alive(second));
    ref->release();
    // Full is refused, and spaces come back.
    u32 made = 0;
    while (s.world.spawn({}))
    {
        made++;
    }
    NX_EXPECT(made == 4095);
    s.world.clear();
    NX_EXPECT(s.world.count() == 0 && s.world.spawn({}) != 0);
}

NX_TEST(referencesOutliveTheirWorld)
{
    EntityRef* kept[200];
    {
        Scene s;
        for (EntityRef*& r : kept)
        {
            r = s.world.ref(s.world.spawn({}));
        }
        kept[3]->release();
        kept[3] = nullptr;
    }
    for (EntityRef* r : kept)
    {
        if (r)
        {
            NX_EXPECT(r->world == nullptr && r->id == 0);
            r->release();
        }
    }
}

NX_TEST(spawningSettlesWithoutAllocating)
{
    Scene s;
    NX_EXPECT(s.load(R"({
        "particles": { "p": { "life": [0.1, 0.2], "speed": [10, 20] } },
        "prefabs": { "shot": { "rect": [4, 4], "velocity": [100, 0], "circle": 4, "hits": 0, "lifetime": 0.2, "emitter": "p", "rate": 60, "children": [ { "disc": 2 } ] } }
    })"));
    size_t before = 0;
    for (int i = 0; i < 600; i++)
    {
        s.world.spawnPrefab("shot", { 0, 0 });
        s.step();
        before = i == 200 ? memStatsTotal().totalCount : before;
    }
    NX_EXPECT(memStatsTotal().totalCount == before);
    NX_EXPECT(s.world.count() > 10 && s.world.count() < 40 && s.world.stats().particles > 10);
    s.step(60);
    NX_EXPECT(s.world.count() == 0 && s.world.stats().particles == 0);
}

NX_TEST(collidersOfEveryShapeMeet)
{
    Scene s;
    const auto make = [&](Shape shape, Vec2 at, float w, float h, float turn)
    {
        const u32 id = s.world.spawn(at);
        s.world.find(id)->rotation = turn;
        Collider* c = s.world.collider(id);
        c->shape = shape;
        c->halfWidth = w;
        c->halfHeight = h;
        return id;
    };
    const u32 circle = make(Shape::Circle, { 0, 0 }, 10, 10, 0);
    const u32 near = make(Shape::Circle, { 19, 0 }, 10, 10, 0);
    const u32 far = make(Shape::Circle, { 15, 15 }, 10, 10, 0);
    const u32 box = make(Shape::Box, { 0, 30 }, 40, 21, 0);
    const u32 turned = make(Shape::Box, { 0, -24 }, 10, 10, 45);
    const u32 corner = make(Shape::Box, { 18, 18 }, 10, 10, 0);
    s.step();
    NX_EXPECT(s.world.touching(circle, near) && !s.world.touching(circle, far));
    NX_EXPECT(s.world.touching(circle, box) && s.world.touching(circle, turned) && !s.world.touching(circle, corner));
    NX_EXPECT(!s.world.touching(turned, box) && s.world.touching(corner, box) && s.world.touching(corner, far));

    Vec2 hit;
    NX_EXPECT(s.world.raycast({ -100, 0 }, { 100, 0 }, kAllGroups, hit) == circle && fabsf(hit.x + 10) < 0.01f);
    NX_EXPECT(s.world.raycast({ 0, 200 }, { 0, 0 }, kAllGroups, hit) == box && fabsf(hit.y - 51) < 0.01f);
    NX_EXPECT(s.world.raycast({ -100, 500 }, { 100, 500 }, kAllGroups, hit) == 0 && hit.x == 100);
    NX_EXPECT(s.world.nearest({ 25, 1 }, 100, kAllGroups, 0) == near && s.world.nearest({ 25, 1 }, 100, kAllGroups, near) != near);
    NX_EXPECT(s.world.nearest({ 25, 1 }, 2, kAllGroups, 0) == 0 && s.world.nearest({ 0, 0 }, 100, 2, 0) == 0);
    u32 found = 0;
    s.world.within({ 0, 0 }, 1, kAllGroups, [&](u32) { found++; });
    NX_EXPECT(found == 1);
}

NX_TEST(solidBodiesStopAtTilesAndWalls)
{
    Scene s;
    NX_EXPECT(s.load(R"({
        "gravity": 1000,
        "map": { "tile": 20, "solid": [1], "platforms": [2], "legend": { "#": 1, "=": 2 }, "rows": [
            "          #",
            "          #",
            "    ==    #",
            "          #",
            "###########" ] }
    })"));
    const u32 id = s.rigid({ 30, 30 }, 16, 16, 0);
    Body* b = s.world.getBody(id);
    b->solid = true;
    s.step(60);
    b = s.world.getBody(id);
    NX_EXPECT(b->onGround && fabsf(s.world.find(id)->position.y - 72) < 0.1f && b->velocity.y == 0);
    // Along the floor without catching on the joins, as far as the wall; however fast.
    b->velocity.x = 6000;
    s.step(30);
    b = s.world.getBody(id);
    NX_EXPECT(fabsf(s.world.find(id)->position.x - 192) < 0.1f && b->onGround && b->velocity.x == 0);
    b->velocity.x = 10;
    s.step();
    NX_EXPECT(s.world.getBody(id)->onWall == 1);
    // Through a platform from below, and onto it from above.
    s.world.find(id)->position = { 100, 72 };
    b->velocity = { 0, -420 };
    s.step(12);
    NX_EXPECT(s.world.find(id)->position.y < 40);
    s.step(60);
    NX_EXPECT(fabsf(s.world.find(id)->position.y - 32) < 0.1f && s.world.getBody(id)->onGround);
    // Growing and shrinking on the floor doesn't turn the floor into a wall.
    s.world.find(id)->position = { 30, 72 };
    for (int i = 0; i < 60; i++)
    {
        s.world.find(id)->scale = { 1, 1 + 0.3f * sinf(static_cast<float>(i)) };
        s.world.getBody(id)->velocity.x = 60;
        s.step();
        NX_EXPECT(fabsf(s.world.find(id)->position.x - (30 + static_cast<float>(i + 1))) < 0.5f && s.world.find(id)->position.y < 80);
    }
    s.world.find(id)->scale = { 1, 1 };
    // Nor does leaning over: it stands as it did, without a flicker.
    s.world.find(id)->position = { 30, 72 };
    s.step(2);
    for (int i = 0; i < 60; i++)
    {
        s.world.find(id)->rotation = 20 * sinf(static_cast<float>(i));
        s.world.getBody(id)->velocity.x = 60;
        s.step();
        NX_EXPECT(s.world.getBody(id)->onGround && s.world.getBody(id)->onWall == 0 && fabsf(s.world.find(id)->position.y - 72) < 0.1f);
    }
    s.world.find(id)->rotation = 0;
    s.world.find(id)->position = { 100, 30 };
    s.step(10);
    // A wall that isn't a tile, and bouncing.
    const u32 wall = s.world.spawn({ 130, 32 });
    s.world.collider(wall)->shape = Shape::Box;
    s.world.collider(wall)->wall = true;
    s.world.body(id)->bounce = 0.5f;
    s.world.body(id)->velocity = { 200, 0 };
    s.step(6);
    NX_EXPECT(s.world.getBody(id)->velocity.x < -90 && s.world.find(id)->position.x < 114);
    s.world.setTileAt({ 90, 45 }, 0);
    s.world.setTileAt({ 110, 45 }, 0);
    NX_EXPECT(s.world.tileAt({ 90, 45 }) == 0 && s.world.tileAt({ 5, 85 }) == 1 && s.world.tileAt({ -5, -5 }) == 0 && s.world.mapSize() == Vec2(220, 100));
}

NX_TEST(rigidBodiesStackAndRest)
{
    Scene s;
    s.world.setGravity({ 0, 1000 });
    s.ground(500);
    u32 stack[8];
    for (u32 i = 0; i < 8; i++)
    {
        stack[i] = s.rigid({ 0, 480 - static_cast<float>(i) * 40.5f }, 40, 40, 1);
    }
    s.step(600);
    for (u32 i = 0; i < 8; i++)
    {
        const World::Entity* e = s.world.find(stack[i]);
        const Body* b = s.world.getBody(stack[i]);
        NX_EXPECT(e && fabsf(e->position.x) < 4 && fabsf(e->position.y - (480 - static_cast<float>(i) * 40)) < 5 && fabsf(e->rotation) < 3);
        NX_EXPECT(b->velocity.dot(b->velocity) < 25 && b->onGround);
    }
    NX_EXPECT(s.world.stats().contacts == 16);

    // A ball bounces as high as it should; one that isn't bouncy stays down.
    const auto ball = [&](float x, float bounce)
    {
        const u32 id = s.world.spawn({ x, 300 });
        s.world.collider(id)->halfWidth = 20;
        s.world.body(id)->mass = 1;
        s.world.body(id)->gravity = 1;
        s.world.body(id)->bounce = bounce;
        return id;
    };
    const u32 bouncy = ball(400, 0.8f), dead = ball(600, 0);
    float highest = 1000;
    for (int i = 0; i < 120; i++)
    {
        s.step();
        highest = i > 45 ? fminf(highest, s.world.find(bouncy)->position.y) : highest;
    }
    NX_EXPECT(highest > 340 && highest < 410);
    NX_EXPECT(fabsf(s.world.find(dead)->position.y - 480) < 2);

    // Friction: a push along the floor wears off on rubber and not on ice.
    const u32 rubber = s.rigid({ -300, 480 }, 40, 40, 1), ice = s.rigid({ -300, 380 }, 40, 40, 1);
    s.world.find(ice)->position = { -600, 480 };
    s.world.getBody(ice)->friction = 0;
    s.world.getBody(rubber)->friction = 1;
    s.step(10);
    s.world.push(rubber, { -200, 0 }, nullptr);
    s.world.push(ice, { -200, 0 }, nullptr);
    s.step(60);
    NX_EXPECT(fabsf(s.world.getBody(rubber)->velocity.x) < 5 && s.world.getBody(ice)->velocity.x < -190);
    // Pushed off-centre, it turns.
    const Vec2 at(-300, -20);
    const u32 free = s.rigid({ -300, 0 }, 40, 40, 2);
    s.world.getBody(free)->gravity = 0;
    s.world.push(free, { 100, 0 }, &at);
    NX_EXPECT(s.world.getBody(free)->velocity.x == 50 && s.world.getBody(free)->spin > 10);
}

NX_TEST(jointsHold)
{
    Scene s;
    s.world.setGravity({ 0, 1000 });
    u32 before = 0;
    u32 links[10];
    for (u32 i = 0; i < 10; i++)
    {
        links[i] = s.rigid({ 20 + static_cast<float>(i) * 40, 0 }, 36, 10, 1);
        s.world.getBody(links[i])->drag = 1.5f;
        NX_EXPECT(s.world.join(links[i], before, { static_cast<float>(i) * 40, 0 }, 0));
        before = links[i];
    }
    s.step(900);
    // It hangs straight down, and is no longer than it was.
    const Vec2 end = s.world.find(links[9])->position;
    if (fabsf(end.x) > 10 || fabsf(end.y - 380) > 10)
    {
        NX_ERROR("the chain ends at %g, %g", static_cast<double>(end.x), static_cast<double>(end.y));
    }
    NX_EXPECT(fabsf(end.x) < 60 && end.y > 360 && end.y < 395);
    NX_EXPECT(!s.world.join(links[0], links[0], {}, 0) && !s.world.join(0, links[0], {}, 0));
    // Cut in the middle, the rest falls.
    s.world.destroyEntity(links[5]);
    s.step(120);
    NX_EXPECT(s.world.find(links[9])->position.y > 1000 && s.world.find(links[4])->position.y < 250);
}

NX_TEST(physicsSurvivesNonsense)
{
    Scene s;
    s.world.setGravity({ 0, 1e9f });
    s.ground(500);
    for (u32 i = 0; i < 40; i++)
    {
        const u32 id = s.rigid({ 0, 0 }, i % 3 ? 10.0f : 0.0f, i % 4 ? 1e-6f : 1e6f, i % 2 ? 1e-9f : 1e9f);
        s.world.getBody(id)->velocity = { i % 5 ? 1e20f : sqrtf(-1.0f), 0 };
        s.world.getBody(id)->spin = 1e30f;
    }
    s.step(60);
    // Whatever is left can still be worked with.
    const u32 fine = s.rigid({ 0, 0 }, 10, 10, 1);
    s.step(2);
    NX_EXPECT(s.world.alive(fine) || !s.world.alive(fine));
}

NX_TEST(sceneFilesAreChecked)
{
    Scene s;
    NX_EXPECT(s.load(R"({
        "groups": ["player", "enemy", "shot"],
        "prefabs": {
            "enemy": { "image": "asset:missing.png", "size": [40, 30], "group": "enemy", "hits": ["shot", "player"], "box": [40, 30], "velocity": [0, 50], "layer": 3,
                "children": [ { "name": "gun", "rect": [4, 10], "position": [0, 20], "color": "#ff8000" } ] },
            "boss": { "base": "enemy", "scale": 3, "velocity": [0, 5], "children": [ { "disc": 4 } ] }
        },
        "entities": [ { "prefab": "boss", "position": [300, 100], "name": "big" }, { "rect": [10, 10], "position": [5, 6], "name": "plain" } ]
    })"));
    const u32 boss = s.world.findNamed("big");
    NX_EXPECT(boss && s.world.count() == 4 && s.world.getBody(boss)->velocity.y == 5 && s.world.find(boss)->scale.x == 3 && s.world.find(boss)->layer == 3);
    NX_EXPECT(s.world.getCollider(boss)->group == 1 && s.world.getCollider(boss)->hits == 5 && s.world.groupNamed("shot") == 2);
    const u32 gun = s.world.findNamed("gun");
    NX_EXPECT(s.world.parentOf(gun) == boss && s.world.worldPosition(gun) == Vec2(300, 160) && s.world.getVisual(gun)->color.r == 1);
    NX_EXPECT(s.world.find(s.world.findNamed("plain"))->position == Vec2(5, 6));
    NX_EXPECT(s.world.spawnPrefab("nothing", {}) == 0 && s.world.takeError() == "no prefab called nothing" && s.world.takeError().empty());

    const struct { const char* json; const char* error; } cases[] = {
        { "[", "scene.json:1: " },
        { "{ \"prefabs\": {\n \"a\": { \"velocty\": [1, 2] } } }", "scene.json:2: what is \"velocty\"?" },
        { "{ \"prefab\": {} }", "what is \"prefab\"?" },
        { "{ \"prefabs\": { \"a\": { \"group\": \"who\" } } }", "no group called who" },
        { "{ \"prefabs\": { \"a\": { \"base\": \"b\" } } }", "no prefab called b" },
        { "{ \"prefabs\": { \"a\": { \"color\": \"red\" } } }", "isn't a colour" },
        { "{ \"prefabs\": { \"a\": { \"color\": \"#12345g\" } } }", "isn't a colour" },
        { "{ \"prefabs\": { \"a\": { \"image\": \"x.png\" } } }", "asset names start" },
        { "{ \"prefabs\": { \"a\": { \"emitter\": \"smoke\" } } }", "no particles called smoke" },
        { "{ \"prefabs\": { \"a\": { \"blend\": \"glow\" } } }", "isn't a blend" },
        { "{ \"particles\": { \"a\": { \"sped\": 1 } } }", "what is \"sped\"?" },
        { "{ \"map\": { \"rows\": [\"#\"] } }", "the legend has no #" },
        { "{ \"map\": { \"rows\": [] } }", "needs rows" },
        { "{ \"map\": { \"rows\": [\"p\"], \"legend\": { \"p\": \"who\" } } }", "no prefab called who" },
        { "{ \"entities\": [ { \"prefab\": \"zz\" } ] }", "no prefab called zz" },
        { "{ \"entities\": [ 5 ] }", "expected { }" },
    };
    for (const auto& c : cases)
    {
        Scene t;
        const bool ok = t.load(c.json);
        const std::string error = t.world.takeError();
        NX_EXPECT(!ok && strstr(error.c_str(), c.error) != nullptr);
        if (strstr(error.c_str(), c.error) == nullptr)
        {
            NX_ERROR("wanted '%s', got '%s'", c.error, error.c_str());
        }
    }
    // Damaged in any way: read or refused, and everything given back.
    const char good[] = "{\"groups\":[\"a\"],\"particles\":{\"p\":{\"life\":[1,2]}},\"prefabs\":{\"q\":{\"rect\":[1,2],\"group\":\"a\",\"emitter\":\"p\",\"children\":[{\"text\":\"x\",\"font\":\"global:freesans.sfn\"}]}},\"map\":{\"rows\":[\"##\"],\"legend\":{\"#\":1}},\"entities\":[{\"prefab\":\"q\"}]}";
    u32 random = 5;
    for (u32 i = 0; i < 300; i++)
    {
        std::string text = good;
        random = random * 1664525 + 1013904223;
        text[(random >> 8) % text.size()] = static_cast<char>(32 + (random >> 24) % 95);
        Scene t;
        t.load(text.c_str());
        t.step(2);
    }
}

NX_TEST(behavioursAreToldWhatHappens)
{
    Rig r;
    open(r, R"(
        int order = 0;
        class Target : Behaviour
        {
            int hits = 0;
            void start() { Save::set("started", entity.name); }
            void onHit(Entity@ other) { hits++; Save::set("hits", "" + hits); Save::set("by", other.name); other.destroy(); }
            void onLeave(Entity@ other) { Save::set("left", "yes"); }
            void onGone() { Save::set("gone", "yes"); }
        }
        class Counter : Behaviour
        {
            float t = 0;
            void update(float dt) { t += dt; entity.position += Vec2(1, 0); if (entity.x > 30) entity.destroy(); }
        }
        class A : Screen
        {
            Entity@ target;
            Entity@ kept;
            void onEnter()
            {
                @target = Scene::spawn(Vec2(0, 0));
                target.name = "target";
                target.circle(10, 1, 1 << 2);
                target.add(Target());
                Entity@ shot = Scene::spawn(Vec2(100, 0));
                shot.name = "shot";
                shot.circle(10, 2);
                shot.velocity = Vec2(-300, 0);
                @kept = shot;
                Entity@ walker = Scene::spawn(Vec2(0, 500));
                walker.name = "walker";
                walker.add(Counter());
            }
            void update(float dt)
            {
                Save::set("alive", kept.alive ? "yes" : "no");
                kept.position += Vec2(0, 0);   // harmless once it has gone
                Save::set("walker", Scene::find("walker") is null ? "no" : "yes");
                Target@ t = cast<Target>(target.behaviour);
                Save::set("cast", t is null ? "no" : "" + t.hits);
                Save::set("count", "" + Scene::count() + "," + Scene::count(1) + "," + Scene::within(Vec2(), 5).length());
            }
        }
    )");
    NX_EXPECT(saved(r, "started", "target") && saved(r, "alive", "yes") && saved(r, "cast", "0"));
    r.step(60);
    NX_EXPECT(saved(r, "hits", "1") && saved(r, "by", "shot") && saved(r, "alive", "no") && saved(r, "cast", "1") && saved(r, "walker", "no"));
    NX_EXPECT(saved(r, "count", "1,1,1") && saved(r, "gone", "(nothing)"));
    // Closing the screen tidies up without calling back into it.
    r.screens.home();
    r.step(2);
    NX_EXPECT(!r.screens.failed() && r.screens.depth() == 1);
}

NX_TEST(behaviourMistakesStopOnlyTheirScreen)
{
    Rig r;
    open(r, "class A : Screen {}");
    const struct { const char* code; const char* error; } cases[] = {
        { "class B : Behaviour { void update(float dt) { int z = 0; z = 1 / z; } } class A : Screen { void onEnter() { Scene::spawn(Vec2()).add(B()); } }", "Divide by zero" },
        { "class B : Behaviour { void start() { while (true) {} } } class A : Screen { void onEnter() { Scene::spawn(Vec2()).add(B()); } }", "endless loop" },
        { "class B : Behaviour { void onGone() { int z = 0; z = 1 / z; } } class A : Screen { void onEnter() { Entity@ e = Scene::spawn(Vec2()); e.add(B()); e.destroy(); } }", "Divide by zero" },
        { "class B : Behaviour { void start() { while (true) Scene::spawn(Vec2()); } } class A : Screen { void onEnter() { Scene::spawn(Vec2()).add(B()); } }", "too many entities" },
        { "class A : Screen { void onEnter() { Scene::spawn(Vec2()).add(null); } }", "null behaviour" },
        { "class A : Screen { void onEnter() { Scene::spawn(\"what\", Vec2()); } }", "no prefab called what" },
        { "class A : Screen { void onEnter() { Scene::load(\"what\"); } }", "asset names start" },
        { "class A : Screen { void onEnter() { Scene::load(\"asset:none.json\"); } }", "cannot be read" },
        { "class A : Screen { void onEnter() { Scene::spawn(Vec2()).circle(5, 16); } }", "groups are 0 to 15" },
        { "class A : Screen { void onEnter() { Scene::burst(\"x\", Vec2(), 5); } }", "no particles called x" },
        { "class A : Screen { void onEnter() { Texture@ t; Scene::spawn(Vec2()).image(t); } }", "null texture" },
        { "class A : Screen { void onEnter() { Entity@ e; e.destroy(); } }", "Null pointer" },
    };
    for (const auto& c : cases)
    {
        r.platform.extraText = c.code;
        r.screens.home();
        r.step(2);
        r.screens.push("shapes");
        r.step(4);
        NX_EXPECT(r.screens.failed() && strstr(r.screens.error(), c.error) != nullptr);
        if (strstr(r.screens.error(), c.error) == nullptr)
        {
            NX_ERROR("wanted '%s', got '%s'", c.error, r.screens.error());
        }
    }
    r.screens.home();
    r.step(2);
    NX_EXPECT(r.screens.depth() == 1 && !r.screens.failed());
}

NX_TEST(behavioursCanDestroyEverything)
{
    Rig r;
    open(r, R"(
        class Bomb : Behaviour
        {
            float fuse = 0.05f;
            void update(float dt)
            {
                fuse -= dt;
                if (fuse > 0) return;
                array<Entity@>@ all = Scene::within(entity.position, 10000);
                for (uint i = 0; i < all.length(); i++) all[i].destroy();
            }
            void onGone() { if (Scene::count() < 400) { Entity@ e = Scene::spawn(Vec2()); e.circle(5); e.add(Bomb()); } }
        }
        class A : Screen
        {
            void onEnter() { for (int i = 0; i < 100; i++) { Entity@ e = Scene::spawn(Vec2(i, i)); e.circle(5, 0, 1); e.add(Bomb()); } }
            void update(float dt) { Save::set("count", "" + Scene::count()); }
            void draw(Canvas@ c) { c.design(1280, 720); Scene::draw(c); }
        }
    )");
    r.step(300);
    NX_EXPECT(!r.screens.failed() && atoi(r.save.get("count", "0")) > 0);
}

NX_TEST(prefabsBringTheirBehaviours)
{
    Rig r;
    open(r, "class A : Screen {}");
    r.screens.home();
    r.step(2);
    r.platform.extraText = R"(
        class Enemy : Behaviour { int health = 3; void onHit(Entity@ other) { health--; } }
        class A : Screen
        {
            void onEnter()
            {
                Scene::load("asset:scene.json");
                Entity@ e = Scene::spawn("enemy", Vec2(7, 8));
                Save::set("health", "" + cast<Enemy>(e.behaviour).health + " " + e.x);
            }
        }
    )";
    // Only one made-up file at a time: the scene is read from the real one, which isn't there.
    r.screens.push("shapes");
    r.step(3);
    NX_EXPECT(r.screens.failed() && strstr(r.screens.error(), "cannot be read"));
}

NX_TEST(camerasAndDrawing)
{
    Scene s;
    s.world.setCamera({ 1000, 500 }, 2, 30);
    Canvas& c = s.rig.canvas;
    s.rig.device->beginFrame({});
    c.begin();
    c.design(1280, 720);
    s.world.draw(c, -100, 100);
    c.end();
    s.rig.device->endFrame();
    const Vec2 middle = s.world.toScreen({ 1000, 500 });
    NX_EXPECT(fabsf(middle.x - 640) < 0.01f && fabsf(middle.y - 360) < 0.01f);
    const Vec2 back = s.world.toWorld(s.world.toScreen({ 1234, 432 }));
    NX_EXPECT(fabsf(back.x - 1234) < 0.1f && fabsf(back.y - 432) < 0.1f);

    // A thousand of one image and a thousand of another, spawned in turn: one draw each. What can't be
    // seen isn't drawn at all.
    Asset* ship = s.rig.assets.texture(kShip);
    Asset* stars = s.rig.assets.texture(kStars);
    s.rig.assets.finish();
    s.world.setCamera({ 0, 0 }, 1, 0);
    for (u32 i = 0; i < 2000; i++)
    {
        const u32 id = s.world.spawn({ i % 2 ? 0.0f : static_cast<float>(i), i < 1000 ? 0.0f : 5000.0f });
        s.world.setLook(id, Look::Image, i % 2 ? ship : stars);
        s.world.getVisual(id)->width = s.world.getVisual(id)->height = 20;
    }
    s.step();
    s.rig.device->beginFrame({});
    c.begin();
    c.design(1280, 720);
    s.world.draw(c, -100, 100);
    c.end();
    s.rig.device->endFrame();
    NX_EXPECT(c.draws() == 2 && s.world.stats().drawn > 700 && s.world.stats().drawn <= 1000);
    s.world.clear();
    s.rig.assets.release(ship);
    s.rig.assets.release(stars);
    NX_EXPECT(s.rig.assets.stats().inUse == 0);
}

NX_TEST(aScreenWithASceneLeavesNothingBehind)
{
    Rig r;
    open(r, "class A : Screen {}");
    r.platform.extraText = R"(
        class B : Behaviour
        {
            Entity@ friend;
            array<int> data(20);
            void update(float dt) { if (entity.y > 400) entity.destroy(); }
            void onHit(Entity@ other) { @friend = other; }
        }
        class A : Screen
        {
            array<Entity@> mine;
            void onEnter() { Scene::setGravity(Vec2(0, 2000)); }
            void update(float dt)
            {
                for (int i = 0; i < 5; i++)
                {
                    Entity@ e = Scene::spawn(Vec2(random(0, 100), 0));
                    e.rect(5, 5, White);
                    e.circle(6, 0, 1);
                    e.gravity = 1;
                    e.add(B());
                    if (mine.length() < 50) mine.insertLast(e);
                }
            }
            void draw(Canvas@ c) { c.design(1280, 720); Scene::draw(c); }
        }
    )";
    size_t settled = 0, scene = 0;
    for (int i = 0; i < 60; i++)
    {
        r.screens.home();
        r.step(2);
        r.screens.push("shapes");
        r.step(40);
        NX_EXPECT(!r.screens.failed());
        r.screens.home();
        r.step(2);
        if (i == 30)
        {
            settled = memStats(MemTag::Script).bytes;
            scene = memStats(MemTag::Scene).bytes;
        }
    }
    NX_EXPECT(memStats(MemTag::Script).bytes <= settled + 2048);
    NX_EXPECT(memStats(MemTag::Scene).bytes == scene);
}
