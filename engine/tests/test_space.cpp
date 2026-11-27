#include "test.h"
#include "rig.h"

#include "assets/geometry.h"
#include "scene/world.h"

#include <math.h>

using namespace nx;
using namespace nx::test;

namespace
{
    struct Place
    {
        Rig rig;
        World world{ rig.assets, rig.host, "none", 4096, 500 };
        Space& space = world.space();

        Place() { space.init(&rig.solids, &rig.audio); }

        void step(int count = 1)
        {
            for (int i = 0; i < count; i++)
            {
                NX_EXPECT(world.update(1.0f / 60));
            }
        }

        u32 thing(const Vec3& at, Shape3 shape, const Vec3& half, float mass)
        {
            const u32 id = world.spawn({});
            space.spatial(id)->place = at;
            Collider3* c = space.collider(id);
            c->shape = shape;
            c->half = half;
            c->wall = mass <= 0;
            if (mass > 0)
            {
                Body3* b = space.body(id);
                b->mass = mass;
                b->gravity = 1;
            }
            space.transform(id & 0xFFFF);
            return id;
        }

        u32 walker(const Vec3& feet)
        {
            const u32 id = thing(feet, Shape3::Capsule, { 0.4f, 0.9f, 0.4f }, 0);
            Collider3* c = space.collider(id);
            c->wall = false;
            c->middle = { 0, 0.9f, 0 };
            Body3* b = space.body(id);
            b->walker = true;
            b->gravity = 1;
            space.transform(id & 0xFFFF);
            return id;
        }

        Vec3 at(u32 id) { return space.getSpatial(id)->place; }

        void draw(u32 times = 1)
        {
            for (u32 i = 0; i < times; i++)
            {
                rig.device->beginFrame({});
                rig.canvas.begin();
                rig.canvas.design(1280, 720);
                space.draw(rig.canvas, 0, 0, 0, 0);
                rig.canvas.end();
                rig.device->endFrame();
            }
        }
    };

    bool close(const Vec3& a, const Vec3& b, float within = 0.001f)
    {
        return (a - b).length() < within;
    }
}

NX_TEST(matricesAndTurnsAgree)
{
    const Quat q = Quat::fromAngles({ 20, 70, -35 });
    const Mat4 m = compose({ 1, 2, 3 }, q, { 2, 1, 0.5f });
    NX_EXPECT(close(m.transformPoint({ 1, 1, 1 }), Vec3(1, 2, 3) + q.rotate({ 2, 1, 0.5f })));
    NX_EXPECT(close(m.inverse().transformPoint(m.transformPoint({ 3, -2, 5 })), { 3, -2, 5 }));
    NX_EXPECT(close(Quat::fromAngles(q.angles()).rotate({ 1, 2, 3 }), q.rotate({ 1, 2, 3 })));
    NX_EXPECT(close(Quat::slerp(Quat(), Quat::axisAngle({ 0, 1, 0 }, 1), 0.5f).rotate({ 1, 0, 0 }), Quat::axisAngle({ 0, 1, 0 }, 0.5f).rotate({ 1, 0, 0 })));
    // The camera looks along -z of its own.
    const Mat4 view = Mat4::lookAt({ 0, 0, 10 }, {}, { 0, 1, 0 });
    NX_EXPECT(close(view.transformPoint({ 1, 2, 0 }), { 1, 2, -10 }));
    NX_EXPECT(close(Mat4::lookAt({ 0, 5, 0 }, {}, { 0, 1, 0 }).transformPoint({}), { 0, 0, -5 }));  // straight down
}

NX_TEST(solidMeshesFaceOutwards)
{
    MeshData d;
    const gpu::Color white = { 1, 1, 1, 1 };
    d.box({}, { 0.5f, 0.5f, 0.5f }, white);
    d.sphere({}, 0.5f, 16, white);
    d.cylinder({ 0, -0.5f, 0 }, 1, 0.5f, 0, 12, white);
    d.capsule(0.5f, 0.5f, 12, white);
    for (u32 i = 0; i < d.indices.count(); i += 3)
    {
        const gpu::MeshVertex &a = d.vertices[d.indices[i]], &b = d.vertices[d.indices[i + 1]], &c = d.vertices[d.indices[i + 2]];
        const Vec3 pa(a.x, a.y, a.z), pb(b.x, b.y, b.z), pc(c.x, c.y, c.z);
        const Vec3 n = (pb - pa).cross(pc - pa);
        if (n.length() > 1e-6f)
        {
            NX_EXPECT(n.dot(pa + pb + pc) > 0);
            NX_EXPECT(n.dot(Vec3(a.nx, a.ny, a.nz)) > 0);
        }
    }
}

NX_TEST(thingsFallAndRest)
{
    Place p;
    p.space.setGround(0);
    const u32 box = p.thing({ 0, 3, 0 }, Shape3::Box, { 0.5f, 0.5f, 0.5f }, 1);
    const u32 ball = p.thing({ 5, 3, 0 }, Shape3::Sphere, { 0.5f, 0, 0 }, 1);
    const u32 capsule = p.thing({ 10, 3, 0 }, Shape3::Capsule, { 0.3f, 0.8f, 0 }, 1);
    p.step(240);
    NX_EXPECT(fabsf(p.at(box).y - 0.5f) < 0.02f);
    NX_EXPECT(fabsf(p.at(ball).y - 0.5f) < 0.02f);
    NX_EXPECT(p.at(capsule).y < 0.85f && p.at(capsule).y > 0.28f);
    NX_EXPECT(p.space.getBody(box)->onGround);
    const Vec3 before = p.at(box);
    p.step(300);
    NX_EXPECT(close(before, p.at(box), 0.002f));
}

NX_TEST(boxesStackInSpace)
{
    Place p;
    p.thing({ 0, -1, 0 }, Shape3::Box, { 20, 1, 20 }, 0);
    u32 boxes[5];
    for (u32 i = 0; i < 5; i++)
    {
        boxes[i] = p.thing({ 0.02f * static_cast<float>(i), 0.5f + static_cast<float>(i) * 1.01f, 0 }, Shape3::Box, { 0.5f, 0.5f, 0.5f }, 1);
    }
    p.step(400);
    for (u32 i = 0; i < 5; i++)
    {
        NX_EXPECT(fabsf(p.at(boxes[i]).y - (0.5f + static_cast<float>(i))) < 0.06f);
        NX_EXPECT(fabsf(p.at(boxes[i]).x) < 0.2f);
    }
    // Still, until what they stand on goes.
    NX_EXPECT(p.space.getBody(boxes[4])->still > 0.5f);
    p.world.destroyEntity(boxes[0]);
    p.step(120);
    NX_EXPECT(p.at(boxes[4]).y < 4);
    for (u32 i = 1; i < 5; i++)
    {
        NX_EXPECT(p.at(boxes[i]).y > 0.4f);
    }
}

NX_TEST(jointsHoldInSpace)
{
    Place p;
    // A pendulum: swings down and through, and is always as long as it was.
    const Vec3 pivot(0, 6, 0);
    const u32 bob = p.thing({ 2, 6, 0 }, Shape3::Sphere, { 0.2f, 0.2f, 0.2f }, 1);
    NX_EXPECT(p.space.pin(bob, 0, pivot));
    float lowest = 10, longest = 0, shortest = 10;
    for (int i = 0; i < 180; i++)
    {
        p.step();
        const float length = (p.at(bob) - pivot).length();
        longest = fmaxf(longest, length);
        shortest = fminf(shortest, length);
        lowest = fminf(lowest, p.at(bob).y);
    }
    NX_EXPECT(lowest < 4.2f && longest < 2.1f && shortest > 1.9f);

    // A chain of five, hanging from the world: as long as it was, and straight down once it settles.
    u32 links[5];
    for (u32 i = 0; i < 5; i++)
    {
        links[i] = p.thing({ 10 + 0.5f + static_cast<float>(i), 8, 0 }, Shape3::Box, { 0.4f, 0.1f, 0.1f }, 1);
        p.space.getBody(links[i])->drag = 0.6f;
        NX_EXPECT(p.space.pin(links[i], i ? links[i - 1] : 0, { 10 + static_cast<float>(i), 8, 0 }));
    }
    p.step(600);
    const Vec3 end = p.at(links[4]);
    NX_EXPECT(fabsf(end.x - 10) < 0.4f && end.y < 8 - 4 && end.y > 8 - 5.3f);

    // A rope lets a weight fall until it is taut, and no further.
    const u32 weight = p.thing({ 20, 7, 0 }, Shape3::Sphere, { 0.2f, 0.2f, 0.2f }, 1);
    NX_EXPECT(p.space.rope(weight, 0, { 20, 7, 0 }, { 20, 8, 0 }, 3));
    float deepest = 0;
    for (int i = 0; i < 240; i++)
    {
        p.step();
        deepest = fmaxf(deepest, 8 - p.at(weight).y);
    }
    NX_EXPECT(deepest < 3.15f && fabsf(8 - p.at(weight).y - 3) < 0.1f);

    // A spring stretches as far as its weight pulls it, and settles there.
    const u32 hanging = p.thing({ 30, 7, 0 }, Shape3::Sphere, { 0.2f, 0.2f, 0.2f }, 1);
    NX_EXPECT(p.space.spring(hanging, 0, { 30, 7, 0 }, { 30, 8, 0 }, 60, 4, 1));
    p.step(600);
    const float stretched = 8 - p.at(hanging).y - 1;
    NX_EXPECT(fabsf(stretched - 9.81f / 60) < 0.05f);

    // A door on a hinge turns about it, and only about it.
    const u32 door = p.thing({ 40.5f, 3, 0 }, Shape3::Box, { 0.5f, 1, 0.05f }, 1);
    p.space.getBody(door)->gravity = 0;
    NX_EXPECT(p.space.hinge(door, 0, { 40, 3, 0 }, { 0, 1, 0 }));
    p.step();
    p.space.getBody(door)->velocity = { 0, 0.5f, 2 };   // pushed, and a little upwards as well
    p.space.forget();
    p.step(30);
    const Spatial& d = *p.space.getSpatial(door);
    const Vec3 up = d.turn.rotate({ 0, 1, 0 });
    const Vec3 edge = d.place + d.turn.rotate({ -0.5f, 0, 0 });
    NX_EXPECT(up.y > 0.98f && close(edge, { 40, 3, 0 }, 0.08f) && fabsf(d.place.z) > 0.2f);

    // They go with what they hold.
    const u32 before = p.space.joints();
    p.world.destroyEntity(bob);
    p.step(2);
    NX_EXPECT(p.space.joints() == before - 1);
    NX_EXPECT(!p.space.pin(door, door, {}) && !p.space.pin(0, door, {}));
}

NX_TEST(limitsAndSweptBalls)
{
    Place p;
    // A door that opens one way to 90 degrees, pushed hard both ways, keeps between them.
    const u32 door = p.thing({ 0.5f, 3, 0 }, Shape3::Box, { 0.5f, 1, 0.05f }, 1);
    p.space.getBody(door)->gravity = 0;
    NX_EXPECT(p.space.hinge(door, 0, { 0, 3, 0 }, { 0, 1, 0 }, 0, 1.5707963f));
    p.step();
    float least = 10, most = -10;
    const auto turned = [&] {
        const Vec3 along = p.space.getSpatial(door)->turn.rotate({ 1, 0, 0 });
        return atan2f(-along.z, along.x);
    };
    for (int i = 0; i < 240; i++)
    {
        // Thrown open, then shut, then open again.
        if (i % 80 == 0)
        {
            p.space.getBody(door)->velocity = { 0, 0, i % 160 == 0 ? -6.0f : 6.0f };
            p.space.forget();
        }
        p.step();
        least = fminf(least, turned());
        most = fmaxf(most, turned());
    }
    NX_EXPECT(least > -0.12f && most < 1.5707963f + 0.12f && most > 1.2f);

    // A ball hanging from a pin, that may swing no more than 30 degrees from straight down, struck hard.
    const u32 bob = p.thing({ 10, 4, 0 }, Shape3::Sphere, { 0.2f, 0.2f, 0.2f }, 1);
    NX_EXPECT(p.space.pin(bob, 0, { 10, 6, 0 }, 0.5235988f));
    p.space.getBody(bob)->velocity = { 12, 0, 0 };
    p.space.forget();
    float widest = 0;
    for (int i = 0; i < 180; i++)
    {
        p.step();
        const Vec3 hang = (p.at(bob) - Vec3(10, 6, 0)).normalized();
        widest = fmaxf(widest, acosf(fminf(1, -hang.y)));
    }
    NX_EXPECT(widest > 0.4f && widest < 0.5235988f + 0.1f);

    // A ball along a line stops its own width short of a wall, facing back.
    const u32 wall = p.thing({ 25, 1, 0 }, Shape3::Box, { 0.5f, 0.5f, 0.5f }, 0);
    u32 id = 0;
    Vec3 at, normal;
    NX_EXPECT(p.space.sweep({ 20, 1, 0 }, { 30, 1, 0 }, 0.5f, 0xFFFF, 0, id, at, normal));
    NX_EXPECT(id == wall && fabsf(at.x - 24) < 0.03f && normal.x < -0.9f);
    // What a thin ray passes over, a thick ball catches.
    Vec3 hit;
    NX_EXPECT(!p.space.raycast({ 20, 1.8f, 0 }, { 30, 1.8f, 0 }, 0xFFFF, 0, id, hit, normal));
    NX_EXPECT(p.space.sweep({ 20, 1.8f, 0 }, { 30, 1.8f, 0 }, 0.5f, 0xFFFF, 0, id, at, normal) && id == wall && at.x < 25);
    NX_EXPECT(!p.space.sweep({ 20, 1.8f, 0 }, { 30, 1.8f, 0 }, 0.5f, 0xFFFF, wall, id, at, normal));
    // Down onto the ground.
    p.space.setGround(0);
    NX_EXPECT(p.space.sweep({ 40, 5, 0 }, { 40, -5, 0 }, 0.5f, 0xFFFF, 0, id, at, normal) && id == 0 && fabsf(at.y - 0.5f) < 0.03f && normal.y > 0.99f);
    // Nothing there.
    NX_EXPECT(!p.space.sweep({ 50, 5, 0 }, { 60, 5, 0 }, 0.5f, 0xFFFF, 0, id, at, normal) && close(at, { 60, 5, 0 }));
}

NX_TEST(soundsBendAsTheyPass)
{
    Place p;
    Asset* engine = p.rig.assets.sound("global/sound/engine.wav");
    p.rig.assets.finish();
    NX_EXPECT(engine->ready && engine->sound);
    p.space.setCamera({ 0, 0, 0 }, { 0, 0, -1 }, 60);
    const u32 car = p.world.spawn({});
    p.space.spatial(car)->place = { 0, 0, -120 };
    PlayOptions o;
    o.loop = true;
    p.rig.assets.addRef(engine);
    const Voice voice = p.rig.audio.play(*engine->sound, engine, o);
    NX_EXPECT(voice.value != 0);
    p.space.follow(voice, car, { 0, 0, -120 }, 1, 300, 1);
    // Without Doppler, as it is.
    for (int i = 0; i < 20; i++)
    {
        p.space.spatial(car)->place.z += 40.0f / 60;
        p.step();
    }
    NX_EXPECT(p.space.pitchOf(voice) == 1);
    // Coming at 40 metres a second, higher; going at that, lower.
    p.space.m_doppler = 1;
    for (int i = 0; i < 60; i++)
    {
        p.space.spatial(car)->place.z += 40.0f / 60;
        p.step();
    }
    const float coming = p.space.pitchOf(voice);
    NX_EXPECT(fabsf(coming - 343.0f / 303) < 0.03f);
    p.space.spatial(car)->place.z = 5;
    p.step();
    for (int i = 0; i < 60; i++)
    {
        p.space.spatial(car)->place.z += 40.0f / 60;
        p.step();
    }
    const float going = p.space.pitchOf(voice);
    NX_EXPECT(fabsf(going - 343.0f / 383) < 0.03f);
    // A jump is not a speed.
    p.space.spatial(car)->place.z = -200;
    p.step();
    NX_EXPECT(fabsf(p.space.pitchOf(voice) - going) < 0.02f);
    p.rig.audio.stop(voice, 0);
    p.rig.assets.release(engine);
}

NX_TEST(ballsBounceAndRoll)
{
    Place p;
    p.space.setGround(0);
    const u32 ball = p.thing({ 0, 4, 0 }, Shape3::Sphere, { 0.5f, 0, 0 }, 1);
    p.space.body(ball)->bounce = 0.8f;
    float highest = 0;
    bool down = false;
    for (u32 i = 0; i < 160; i++)
    {
        p.step();
        down = down || p.at(ball).y < 0.6f;
        highest = down ? fmaxf(highest, p.at(ball).y) : 0;
    }
    NX_EXPECT(highest > 1.8f && highest < 3.8f);
    // Pushed along, it begins to roll.
    const u32 rolling = p.thing({ 5, 0.5f, 0 }, Shape3::Sphere, { 0.5f, 0, 0 }, 1);
    p.space.push(rolling, { 3, 0, 0 }, nullptr);
    p.step(60);
    NX_EXPECT(p.at(rolling).x > 6.5f);
    NX_EXPECT(p.space.getBody(rolling)->spin.z < -1);
}

NX_TEST(frictionHoldsOnASlope)
{
    Place p;
    const u32 slope = p.thing({}, Shape3::Box, { 20, 0.5f, 20 }, 0);
    p.space.getSpatial(slope)->turn = Quat::fromAngles({ 0, 0, 15 });
    const Vec3 up = p.space.getSpatial(slope)->turn.rotate({ 0, 1, 0 });
    u32 boxes[2];
    for (u32 i = 0; i < 2; i++)
    {
        boxes[i] = p.thing(up * 1.02f + Vec3(0, 0, static_cast<float>(i) * 4), Shape3::Box, { 0.5f, 0.5f, 0.5f }, 1);
        p.space.getSpatial(boxes[i])->turn = p.space.getSpatial(slope)->turn;
        p.space.body(boxes[i])->friction = i == 0 ? 1.0f : 0.02f;
    }
    p.step(120);
    NX_EXPECT(fabsf(p.at(boxes[0]).x) < 1);
    NX_EXPECT(fabsf(p.at(boxes[1]).x) > 2);
}

NX_TEST(walkersWalk)
{
    Place p;
    p.space.setGround(0);
    const u32 w = p.walker({ 0, 0.5f, 0 });
    p.step(60);
    NX_EXPECT(p.space.getBody(w)->onGround);
    NX_EXPECT(fabsf(p.at(w).y) < 0.02f);
    // Into a wall, and along it.
    p.thing({ 5, 2, 0 }, Shape3::Box, { 0.5f, 2, 50 }, 0);
    for (u32 i = 0; i < 120; i++)
    {
        p.space.body(w)->velocity = { 4, p.space.body(w)->velocity.y, 2 };
        p.step();
    }
    NX_EXPECT(fabsf(p.at(w).x - 4.1f) < 0.05f);
    NX_EXPECT(p.at(w).z > 3.5f);
    NX_EXPECT(p.at(w).y > -0.02f && p.at(w).y < 0.1f);
}

NX_TEST(walkersClimbSteps)
{
    Place p;
    p.space.setGround(0);
    for (u32 i = 0; i < 4; i++)
    {
        p.thing({ 3 + static_cast<float>(i), 0.15f + 0.3f * static_cast<float>(i), 0 }, Shape3::Box, { 0.5f, 0.15f, 5 }, 0);
    }
    p.thing({ 12, 0.6f, 0 }, Shape3::Box, { 5.5f, 0.6f, 5 }, 0);
    const u32 w = p.walker({ 0, 0, 0 });
    for (u32 i = 0; i < 180; i++)
    {
        p.space.body(w)->velocity = { 3, p.space.body(w)->velocity.y, 0 };
        p.step();
    }
    NX_EXPECT(p.at(w).x > 7.5f);
    NX_EXPECT(fabsf(p.at(w).y - 1.2f) < 0.05f);
    // And down again without leaving the ground.
    bool stayed = true;
    for (u32 i = 0; i < 180; i++)
    {
        p.space.body(w)->velocity = { -3, p.space.body(w)->velocity.y, 0 };
        p.step();
        stayed = stayed && p.space.getBody(w)->onGround;
    }
    NX_EXPECT(fabsf(p.at(w).y) < 0.05f);
    NX_EXPECT(stayed);
    // But not a wall.
    p.thing({ -12, 0.5f, 0 }, Shape3::Box, { 1, 0.5f, 5 }, 0);
    for (u32 i = 0; i < 240; i++)
    {
        p.space.body(w)->velocity = { -3, p.space.body(w)->velocity.y, 0 };
        p.step();
    }
    NX_EXPECT(p.at(w).x > -10.8f);
}

NX_TEST(walkersPushCratesAndStandOnThem)
{
    Place p;
    p.space.setGround(0);
    const u32 crate = p.thing({ 2, 0.5f, 0 }, Shape3::Box, { 0.5f, 0.5f, 0.5f }, 10);
    const u32 w = p.walker({ 0, 0, 0 });
    for (u32 i = 0; i < 120; i++)
    {
        p.space.body(w)->velocity = { 2, p.space.body(w)->velocity.y, 0 };
        p.step();
    }
    NX_EXPECT(p.at(crate).x > 3);
    NX_EXPECT(p.at(w).x < p.at(crate).x - 0.6f);
    NX_EXPECT(fabsf(p.at(crate).y - 0.5f) < 0.1f);
    p.space.getSpatial(w)->place = p.at(crate) + Vec3(0, 1.5f, 0);
    p.space.body(w)->velocity = {};
    p.step(60);
    NX_EXPECT(fabsf(p.at(w).y - 1) < 0.06f);
    NX_EXPECT(p.space.getBody(w)->onGround);
}

NX_TEST(shellsAreTheShapeOfTheirModels)
{
    Place p;
    // A room: a floor, a ceiling and three walls, open towards z.
    std::string obj;
    const float c[8][3] = { { -4, 0, -4 }, { 4, 0, -4 }, { 4, 3, -4 }, { -4, 3, -4 }, { -4, 0, 4 }, { 4, 0, 4 }, { 4, 3, 4 }, { -4, 3, 4 } };
    for (const auto& v : c)
    {
        char line[64];
        snprintf(line, sizeof(line), "v %g %g %g\n", static_cast<double>(v[0]), static_cast<double>(v[1]), static_cast<double>(v[2]));
        obj += line;
    }
    obj += "f 1 2 6 5\nf 4 3 7 8\nf 1 2 3 4\nf 1 5 8 4\nf 2 6 7 3\n";
    // A step up, across the back.
    obj += "v -4 0.3 -1\nv 4 0.3 -1\nv -4 0.3 -4\nv 4 0.3 -4\nv -4 0 -1\nv 4 0 -1\nf 9 10 12 11\nf 13 14 10 9\n";
    p.rig.platform.extraPath = "global/room.obj";
    p.rig.platform.extraText = obj;
    JsonTree tree;
    const char text[] = "{ \"model\": \"global:room.obj\", \"exact\": true, \"wall\": true, \"place\": [10, 5, 0], \"angles\": 90, \"size\": [1, 1, 1.5] }";
    NX_EXPECT(tree.parse(text, sizeof(text) - 1));
    const u32 room = p.space.make(tree.root(), "", {});
    p.rig.assets.finish();
    p.space.setGround(0);
    p.step();
    // It is open towards x now, and six metres each way along that.
    u32 id = 0;
    Vec3 hit, normal;
    NX_EXPECT(p.space.raycast({ 10, 7, 0 }, { 10, 0, 0 }, kAllGroups, 0, id, hit, normal) && id == room && fabsf(hit.y - 5) < 0.01f && normal.y > 0.99f);
    NX_EXPECT(p.space.raycast({ 10, 7, 0 }, { 10, 20, 0 }, kAllGroups, 0, id, hit, normal) && id == room && fabsf(hit.y - 8) < 0.01f && normal.y < -0.99f);
    NX_EXPECT(p.space.raycast({ 30, 6, 0 }, { -30, 6, 0 }, kAllGroups, 0, id, hit, normal) && id == room && fabsf(hit.x - 4) < 0.01f);
    NX_EXPECT(!p.space.raycast({ 12, 6, 0 }, { 30, 6, 0 }, kAllGroups, 0, id, hit, normal));
    // Into it, over the step, as far as the back, and not through any of it.
    const u32 w = p.walker({ 14, 5.1f, 1 });
    for (u32 i = 0; i < 240; i++)
    {
        p.space.body(w)->velocity = { -5, p.space.body(w)->velocity.y, 0 };
        p.step();
        NX_EXPECT(p.at(w).y > 4.95f);
    }
    NX_EXPECT(p.at(w).x < 4.7f && p.at(w).x > 4 && fabsf(p.at(w).y - 5.3f) < 0.05f && p.space.body(w)->onGround);
    for (u32 i = 0; i < 120; i++)
    {
        p.space.body(w)->velocity = { 0, p.space.body(w)->velocity.y, 6 };
        p.step();
    }
    NX_EXPECT(p.at(w).z > 3 && p.at(w).z < 4 && p.at(w).y > 5.2f);
    // Things rest in it, whatever their shape, and stay.
    const u32 ball = p.thing({ 12, 6.5f, 0 }, Shape3::Sphere, { 0.4f, 0, 0 }, 2);
    const u32 box = p.thing({ 12, 6.5f, 2 }, Shape3::Box, { 0.4f, 0.4f, 0.4f }, 2);
    const u32 pill = p.thing({ 12, 6.8f, -2 }, Shape3::Capsule, { 0.3f, 0.7f, 0 }, 2);
    p.step(400);
    NX_EXPECT(fabsf(p.at(ball).y - 5.4f) < 0.05f && fabsf(p.at(box).y - 5.4f) < 0.05f && p.at(pill).y > 5.2f && p.at(pill).y < 6);
    p.space.body(ball)->still = 0;
    p.space.body(ball)->velocity = { -14, 3, 5 };
    p.step(200);
    NX_EXPECT(p.at(ball).y > 5.3f && p.at(ball).x > 4 && fabsf(p.at(ball).z) < 4);
}

NX_TEST(thereIsSomethingUnderTheLand)
{
    Place p;
    NX_EXPECT(!p.space.hole(0, 0, 4, true));
    NX_EXPECT(p.space.makeLand(7, 64, 2, 4, 0.5f, false));
    p.space.flatten(0, 0, 30, 2);
    p.thing({ 0, -10.5f, 0 }, Shape3::Box, { 30, 0.5f, 30 }, 0);
    NX_EXPECT(p.space.hole(0, 0, 4, true) && p.space.heightAt(0, 0) < -1000 && p.space.heightAt(8, 0) > 1.9f);
    u32 id = 0;
    Vec3 hit, normal;
    NX_EXPECT(p.space.raycast({ 0.5f, 10, 0.5f }, { 0.5f, -20, 0.5f }, kAllGroups, 0, id, hit, normal) && fabsf(hit.y + 10) < 0.05f);
    NX_EXPECT(p.space.raycast({ 9, 10, 0.5f }, { 9, -20, 0.5f }, kAllGroups, 0, id, hit, normal) && fabsf(hit.y - 2) < 0.05f);
    // Down through it, and then along underneath without being pushed up.
    const u32 w = p.walker({ 0, 3, 0 });
    p.step(160);
    NX_EXPECT(fabsf(p.at(w).y + 10) < 0.1f);
    for (u32 i = 0; i < 150; i++)
    {
        p.space.body(w)->velocity = { 5, p.space.body(w)->velocity.y, 0 };
        p.step();
    }
    NX_EXPECT(p.at(w).x > 9 && fabsf(p.at(w).y + 10) < 0.1f);
    const std::string kept = p.space.landEdits();
    NX_EXPECT(!kept.empty() && p.space.setLandEdits("") && p.space.heightAt(0, 0) > 1.9f);
    NX_EXPECT(p.space.setLandEdits(kept) && p.space.heightAt(0, 0) < -1000 && p.space.landEdits() == kept);
    // The last letter may hold no more than what is left over.
    for (size_t cut = 1; cut + 1 < kept.size(); cut++)
    {
        NX_EXPECT(!p.space.setLandEdits(kept.substr(0, cut)));
    }
    NX_EXPECT(p.space.hole(0, 0, 6, false) && p.space.heightAt(0, 0) > 1.9f && p.space.landEdits().empty());
}

NX_TEST(landHasWallsAtItsSides)
{
    Place p;
    NX_EXPECT(p.space.makeLand(7, 32, 2, 0.001f, 0.5f, false));
    const u32 w = p.walker({ 28, 0.5f, 0 }), ball = p.thing({ 0, 1, 25 }, Shape3::Sphere, { 0.5f, 0, 0 }, 1);
    for (const bool edges : { true, false })
    {
        p.space.setEdges(edges);
        for (u32 i = 0; i < 120; i++)
        {
            p.space.body(w)->velocity = { 6, p.space.body(w)->velocity.y, 1 };
            p.space.body(ball)->velocity = { 0, p.space.body(ball)->velocity.y, 10 };
            p.step();
        }
        NX_EXPECT(edges ? p.at(w).x < 32 && p.at(w).x > 31 && p.at(ball).z < 32 && p.at(w).y > -0.1f && p.at(ball).y > 0 : p.at(w).x > 33 && p.at(ball).z > 33);
    }
}

NX_TEST(whatMovesTakesWalkersAlong)
{
    Place p;
    JsonTree tree;
    const char text[] = "{ \"shape\": \"box\", \"size\": [4, 0.5, 4], \"cube\": [1, 1, 1], \"wall\": true, \"moves\": [12, 3, 0], \"seconds\": 4, \"place\": [0, 1, 0] }";
    NX_EXPECT(tree.parse(text, sizeof(text) - 1));
    const u32 lift = p.space.make(tree.root(), "", {});
    p.space.setGround(-20);
    const u32 w = p.walker({ 0.5f, 1.4f, 0 });
    float farthest = 0;
    for (u32 i = 0; i < 480; i++)
    {
        p.step();
        farthest = fmaxf(farthest, p.at(lift).x);
        NX_EXPECT(i < 5 || (fabsf(p.at(w).x - p.at(lift).x - 0.5f) < 0.4f && fabsf(p.at(w).y - p.at(lift).y - 0.25f) < 0.25f));
    }
    NX_EXPECT(farthest > 11.9f && fabsf(p.at(lift).x) < 0.2f);
}

NX_TEST(thingsMakeSounds)
{
    Place p;
    JsonTree tree;
    const char text[] = "{ \"sound\": \"global:sound/bell.wav\", \"reach\": 20, \"place\": [1, 0, 0] }";
    NX_EXPECT(tree.parse(text, sizeof(text) - 1));
    p.space.setCamera({ 0, 1, 0 }, { 0, 1, -5 }, 60);
    const u32 id = p.space.make(tree.root(), "", {});
    NX_EXPECT(id != 0);
    p.rig.assets.finish();
    p.step(2);
    NX_EXPECT(p.rig.audio.voicesInUse() == 1);
    // It goes with the thing.
    p.world.destroyEntity(id);
    p.step(2);
    i16 mixed[4800 * 2];
    for (u32 i = 0; i < 10; i++)
    {
        p.rig.audio.mix(mixed, 4800);
        p.rig.audio.update();
    }
    NX_EXPECT(p.rig.audio.voicesInUse() == 0);
    // Too far to hear; and gone before it arrived.
    const char far[] = "{ \"sound\": \"global:sound/bell.wav\", \"reach\": 5, \"place\": [100, 0, 0] }";
    JsonTree other;
    NX_EXPECT(other.parse(far, sizeof(far) - 1) && p.space.make(other.root(), "", {}) != 0);
    p.world.destroyEntity(p.space.make(tree.root(), "", {}));
    p.rig.assets.finish();
    p.step(2);
    NX_EXPECT(p.rig.audio.voicesInUse() == 0);
}

NX_TEST(landIsChangedByHand)
{
    Place p;
    NX_EXPECT(!p.space.raise(0, 0, 5, 1) && p.space.landEdits().empty() && !p.space.setLandEdits(""));
    NX_EXPECT(p.space.makeLand(7, 64, 2, 8, 0.5f, false) && p.space.landEdits().empty());
    const float was = p.space.heightAt(0, 0), beyond = p.space.heightAt(20, 20), edge = p.space.heightAt(64, 64);
    // Across the corner of four regions, and off the end.
    NX_EXPECT(p.space.raise(0, 0, 8, 3) && p.space.raise(64, 64, 6, 2) && !p.space.raise(500, 0, 6, 2) && !p.space.raise(0, 0, 0, 2) && !p.space.raise(NAN, 0, 5, 2));
    NX_EXPECT(fabsf(p.space.heightAt(0, 0) - was - 3) < 0.001f && p.space.heightAt(20, 20) == beyond && fabsf(p.space.heightAt(64, 64) - edge - 2) < 0.001f);
    for (float x = -7; x < 7; x += 0.9f)
    {
        u32 id = 1;
        Vec3 hit, normal;
        NX_EXPECT(p.space.raycast({ x, 100, 1 }, { x, -100, 1 }, kAllGroups, 0, id, hit, normal) && fabsf(hit.y - p.space.heightAt(x, 1)) < 0.01f);
    }
    for (u32 i = 0; i < 60; i++)
    {
        p.space.level(-30, 10, 10, 5, 0.3f);
        p.space.smooth(0, 0, 8, 1);
    }
    NX_EXPECT(fabsf(p.space.heightAt(-30, 10) - 5) < 0.01f && fabsf(p.space.heightAt(-27, 10) - 5) < 0.01f && p.space.heightAt(0, 0) < was + 2);
    p.space.paint(10, 10, 6, { 1, 0, 0, 1 }, 1);
    const std::string kept = p.space.landEdits();
    const float a = p.space.heightAt(-28, 12), b = p.space.heightAt(1, 1);
    NX_EXPECT(!kept.empty() && kept.size() < 2500);

    // Away, and back again, also on other land of the same size.
    NX_EXPECT(p.space.setLandEdits("") && p.space.heightAt(0, 0) == was && p.space.landEdits().empty());
    NX_EXPECT(p.space.setLandEdits(kept) && fabsf(p.space.heightAt(-28, 12) - a) < 0.01f && fabsf(p.space.heightAt(1, 1) - b) < 0.01f && p.space.landEdits() == kept);
    NX_EXPECT(p.space.makeLand(7, 64, 2, 8, 0.5f, false) && p.space.setLandEdits(kept) && p.space.landEdits() == kept);
    // Paint can be taken away.
    for (u32 i = 0; i < 300; i++)
    {
        p.space.paint(10, 10, 12, { 0, 0, 0, 0 }, 0.2f);
    }
    p.space.setLandEdits(p.space.landEdits());
    NX_EXPECT(p.space.landEdits().size() < kept.size());

    // What is damaged changes nothing.
    const std::string now = p.space.landEdits();
    for (size_t cut = 1; cut < kept.size(); cut += 3)
    {
        std::string bad = kept.substr(0, cut);
        NX_EXPECT(!p.space.setLandEdits(bad));
        bad = kept;
        bad[cut] = bad[cut] == 'A' ? '/' : 'A';
        p.space.setLandEdits(bad);
    }
    NX_EXPECT(!p.space.setLandEdits("no!") && !p.space.setLandEdits("AAAA"));
    NX_EXPECT(p.space.setLandEdits(now) && p.space.landEdits() == now);
    // By region, so they fit other land too.
    NX_EXPECT(p.space.makeLand(7, 32, 2, 8, 0.5f, false) && p.space.setLandEdits(kept));

    // What rested on it falls when it goes.
    const u32 ball = p.thing({ 0, p.space.heightAt(0, 0) + 0.5f, 0 }, Shape3::Sphere, { 0.5f, 0, 0 }, 1);
    p.step(200);
    NX_EXPECT(p.space.raise(0, 0, 14, -4));
    p.step(120);
    const Vec3 at = p.at(ball);
    NX_EXPECT(fabsf(at.y - p.space.heightAt(at.x, at.z) - 0.5f) < 0.3f);
    p.space.makeEndlessLand(9, 2, 30, 0.5f, 3);
    // Endless land too, and what was done stays when the region goes and comes back.
    NX_EXPECT(p.space.raise(0, 0, 5, 1) && p.space.paint(0, 0, 5, { 1, 1, 1, 1 }, 1));
    const float raised = p.space.heightAt(0, 0);
    NX_EXPECT(p.space.raise(2000, 2000, 5, 3) && !p.space.landEdits().empty());
    p.space.setCamera({ 3000, 50, 3000 }, { 3000, 0, 2990 }, 60);
    p.step(30);
    p.space.setCamera({ 0, 50, 0 }, { 0, 0, -10 }, 60);
    p.step(30);
    NX_EXPECT(fabsf(p.space.heightAt(0, 0) - raised) < 0.01f);
}

NX_TEST(landHoldsThingsUp)
{
    Place p;
    NX_EXPECT(p.space.makeLand(7, 64, 2, 20, 0.5f, false));
    NX_EXPECT(p.space.landSize() == 128);
    const u32 ball = p.thing({ 10, p.space.heightAt(10, 10) + 2, 10 }, Shape3::Sphere, { 0.5f, 0, 0 }, 1);
    const u32 w = p.walker({ -20, p.space.heightAt(-20, 5) + 0.5f, 5 });
    float lowest = 1e9f, highest = -1e9f;
    for (float x = -60; x < 60; x += 3.1f)
    {
        const float h = p.space.heightAt(x, x * 0.5f);
        lowest = fminf(lowest, h);
        highest = fmaxf(highest, h);
        // Where it is drawn is where a line meets it.
        u32 id = 1;
        Vec3 hit, normal;
        NX_EXPECT(p.space.raycast({ x, 100, x * 0.5f }, { x, -100, x * 0.5f }, kAllGroups, 0, id, hit, normal));
        NX_EXPECT(id == 0 && fabsf(hit.y - h) < 0.01f);
    }
    NX_EXPECT(highest - lowest > 3);
    for (u32 i = 0; i < 300; i++)
    {
        p.space.body(w)->velocity = { 4, p.space.body(w)->velocity.y, 1 };
        p.step();
        const Vec3 at = p.at(w);
        NX_EXPECT(at.y > p.space.heightAt(at.x, at.z) - 0.1f);
    }
    NX_EXPECT(p.at(w).x > -12);
    NX_EXPECT(p.at(ball).y > p.space.heightAt(p.at(ball).x, p.at(ball).z));
    p.space.flatten(0, 0, 10, 3);
    NX_EXPECT(fabsf(p.space.heightAt(2, 2) - 3) < 0.01f && fabsf(p.space.heightAt(-2, 1) - 3) < 0.01f);
}

NX_TEST(linesMeetColliders)
{
    Place p;
    const u32 box = p.thing({ 0, 0, 0 }, Shape3::Box, { 1, 1, 1 }, 0);
    const u32 ball = p.thing({ 0, 0, 10 }, Shape3::Sphere, { 1, 0, 0 }, 0);
    const u32 capsule = p.thing({ 0, 0, 20 }, Shape3::Capsule, { 0.5f, 2, 0 }, 0);
    u32 id = 0;
    Vec3 hit, normal;
    NX_EXPECT(p.space.raycast({ -5, 0.5f, 0 }, { 5, 0.5f, 0 }, kAllGroups, 0, id, hit, normal) && id == box && close(hit, { -1, 0.5f, 0 }) && close(normal, { -1, 0, 0 }));
    NX_EXPECT(p.space.raycast({ 0, 5, 10 }, { 0, -5, 10 }, kAllGroups, 0, id, hit, normal) && id == ball && close(hit, { 0, 1, 10 }));
    NX_EXPECT(p.space.raycast({ -5, 1, 20 }, { 5, 1, 20 }, kAllGroups, 0, id, hit, normal) && id == capsule && close(hit, { -0.5f, 1, 20 }));
    NX_EXPECT(p.space.raycast({ 0, 5, 20 }, { 0, -5, 20 }, kAllGroups, 0, id, hit, normal) && id == capsule && close(hit, { 0, 2, 20 }));
    NX_EXPECT(!p.space.raycast({ -5, 3, 0 }, { 5, 3, 0 }, kAllGroups, 0, id, hit, normal) && id == 0);
    NX_EXPECT(!p.space.raycast({ -5, 0, 0 }, { 5, 0, 0 }, kAllGroups, box, id, hit, normal));
    u32 found = 0;
    p.space.within({ 0, 0, 5 }, 4.5f, kAllGroups, [&](u32) { found++; });
    NX_EXPECT(found == 2);
    NX_EXPECT(!p.space.touching(box, ball));
}

NX_TEST(meltedShapesBecomeTriangles)
{
    gpu::Field f;
    f.count = 2;
    f.shapes[0].size = { 1, 0, 0 };
    f.shapes[1].place = { 0, 1.2f, 0 };
    f.shapes[1].size = { 0.7f, 0, 0 };
    f.shapes[1].soft = 0.3f;
    NX_EXPECT(Space::fieldDistance(f, {}, nullptr) < 0);
    NX_EXPECT(Space::fieldDistance(f, { 3, 0, 0 }, nullptr) > 1.9f);
    MeshData d;
    Space::fieldMesh(f, { 0, 0.5f, 0 }, 2, 24, d);
    NX_EXPECT(d.indices.count() > 600);
    for (u32 i = 0; i < d.indices.count(); i += 3)
    {
        const gpu::MeshVertex &a = d.vertices[d.indices[i]], &b = d.vertices[d.indices[i + 1]], &c = d.vertices[d.indices[i + 2]];
        NX_EXPECT(fabsf(Space::fieldDistance(f, { a.x, a.y, a.z }, nullptr)) < 0.06f);
        const Vec3 pa(a.x, a.y, a.z), pb(b.x, b.y, b.z), pc(c.x, c.y, c.z);
        const Vec3 n = (pb - pa).cross(pc - pa);
        if (n.length() > 1e-4f)
        {
            NX_EXPECT(n.dot(Vec3(a.nx, a.ny, a.nz)) > 0);
        }
    }
    // What is too far to matter is left out, and that doesn't show.
    f.count = 8;
    for (u32 i = 2; i < 8; i++)
    {
        f.shapes[i].place = { sinf(static_cast<float>(i)) * 2, cosf(static_cast<float>(i)), 1 };
        f.shapes[i].size = { 0.5f, 0, 0 };
        f.shapes[i].soft = 0.25f;
    }
    f.shapes[4].kind = gpu::FieldKind::Box;
    f.shapes[4].size = { 0.4f, 0.4f, 0.4f };
    MeshData whole;
    Space::fieldMesh(f, {}, 3, 32, whole);
    float lowest = 1e9f, highest = 0;
    for (const gpu::MeshVertex& v : whole.vertices)
    {
        const float far = Space::fieldDistance(f, { v.x, v.y, v.z }, nullptr);
        lowest = fminf(lowest, far);
        highest = fmaxf(highest, far);
    }
    NX_EXPECT(lowest > -0.1f && highest < 0.1f);
    NX_EXPECT(whole.indices.count() > 2000);
}

NX_TEST(onlyWhatIsSeenIsDrawn)
{
    Place p;
    for (int i = -10; i <= 10; i++)
    {
        const u32 id = p.world.spawn({});
        p.space.spatial(id)->place = { 0, 0, static_cast<float>(i) * 10 };
        p.space.setSolid(id, Solid::Sphere);
    }
    p.step();
    p.space.setCamera({ 0, 2, 3 }, { 0, 0, -10 }, 60);
    p.draw(2);
    NX_EXPECT(p.space.stats().drawn >= 9 && p.space.stats().drawn <= 11);
    NX_EXPECT(p.space.stats().hidden >= 10);
    // Those nearer are drawn better.
    NX_EXPECT(p.space.stats().details[0] >= 1 && p.space.stats().details[2] >= 3);
    NX_EXPECT(p.rig.device->stats().rejected == 0);
    NX_EXPECT(p.rig.device->stats().triangles > 100);
    Vec2 screen;
    NX_EXPECT(p.space.toScreen({ 0, 0, -10 }, screen) && fabsf(screen.x - 640) < 1 && fabsf(screen.y - 360) < 1);
    NX_EXPECT(!p.space.toScreen({ 0, 0, 30 }, screen));
    NX_EXPECT(close(p.space.rayThrough({ 640, 360 }), (Vec3(0, 0, -10) - Vec3(0, 2, 3)).normalized()));
}

NX_TEST(everythingInSpaceCanBeDrawn)
{
    Place p;
    NX_EXPECT(p.space.makeLand(3, 64, 1, 8, 0.5f, true));
    p.space.setWater(0, { 0.1f, 0.2f, 0.3f, 0.7f });
    p.space.setSky({ 0, 0, 1, 1 }, { 1, 1, 1, 1 });
    p.space.field().count = 1;
    for (u32 s = 1; s < static_cast<u32>(Solid::Count); s++)
    {
        const u32 id = p.world.spawn({});
        p.space.spatial(id)->place = { static_cast<float>(s), 10, 0 };
        p.space.setSolid(id, static_cast<Solid>(s));
        p.space.appearance(id)->blend = s % 2 ? gpu::Blend::Alpha : gpu::Blend::None;
    }
    const u32 glow = p.world.spawn({});
    p.space.appearance(glow)->glowBall = true;
    p.space.light(glow)->color = { 1, 0, 0, 1 };
    p.step();
    p.space.setCamera({ 3, 12, 8 }, { 3, 9, 0 }, 60);
    p.draw(3);
    NX_EXPECT(p.rig.device->stats().rejected == 0);
    NX_EXPECT(p.rig.device->stats().draws > 10);
}

NX_TEST(childrenMoveWithTheirParentsInSpace)
{
    Place p;
    const u32 parent = p.world.spawn({}), child = p.world.spawn({});
    p.space.spatial(parent)->place = { 10, 0, 0 };
    p.space.spatial(parent)->turn = Quat::fromAngles({ 0, 90, 0 });
    p.space.spatial(child)->place = { 0, 0, 2 };
    p.world.setParent(child, parent);
    NX_EXPECT(close(p.space.worldPlace(child), { 12, 0, 0 }));
    Collider3* c = p.space.collider(child);
    c->shape = Shape3::Sphere;
    c->half = { 0.5f, 0, 0 };
    p.step();
    u32 found = 0;
    p.space.within({ 12, 0, 0 }, 0.1f, kAllGroups, [&](u32 id) { found = id; });
    NX_EXPECT(found == child);
    p.world.destroyEntity(parent);
    p.step();
    NX_EXPECT(p.world.count() == 0);
    NX_EXPECT(p.space.getSpatial(child) == nullptr);
}

NX_TEST(grassImpostorsAndSprites)
{
    Rig r;
    NX_EXPECT(r.ok);
    r.platform.extraPath = "showcase/shapes/main.as";
    r.platform.extraText = R"(
        class A : Screen
        {
            Entity@ ghost;
            void onEnter()
            {
                Space::land(3, 64, 2, 4, 0.3f);
                Space::grass(Assets::model("global:model/grass.glb"), 2, 20);
                Space::impostors = 30;
                for (int i = 0; i < 6; i++)
                {
                    Entity@ tree = Space::spawn(Vec3(-20 + i * 8, Space::height(-20 + i * 8, -50), -50));
                    tree.model(Assets::model("global:model/pine.glb"));
                    tree.impostor = true;
                }
                @ghost = Space::spawn(Vec3(0, 1, -5));
                ghost.sprite(Assets::texture("global:picture/sprite-wisp.png"), 4, 2);
                ghost.spriteFrame = 3;
                Space::camera(Vec3(0, Space::height(0, 0) + 2, 0), Vec3(0, Space::height(0, -50), -50));
            }
            void draw(Canvas@ c) { Space::draw(c); }
        }
    )";
    NX_EXPECT(r.screens.init(r.services, "showcase", "main"));
    r.step(2);
    r.screens.push("shapes");
    r.step(30);
    NX_EXPECT(!r.screens.failed());
    NX_EXPECT(r.device->stats().rejected == 0);
    Space& space = r.services.world->space();
    // Squares of grass round the camera, the six far trees and the sprite as pictures.
    NX_EXPECT(space.grassCells() > 4);
    NX_EXPECT(space.cardsDrawn() == 7);
    r.screens.home();
    r.step(3);
}

NX_TEST(thingsAreHeldByBones)
{
    Rig r;
    NX_EXPECT(r.ok);
    r.platform.extraPath = "showcase/shapes/main.as";
    r.platform.extraText = R"(
        class A : Screen
        {
            Entity@ robot;
            Entity@ held;
            int frames = 0;
            array<Vec3> seen;
            // A mistake stops the screen, which the test sees.
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            void onEnter()
            {
                @robot = Space::spawn(Vec3(3, 0, -2));
                robot.model(Assets::model("global:model/robot.glb"));
                @held = Space::spawn(Vec3());
                check(held.attach(robot, "hand.r"));
                held.sphere(0.1f, 2);
            }
            void update(float dt)
            {
                frames++;
                if (frames == 5)
                {
                    check(robot.bones > 4);
                    bool hand = false;
                    for (uint i = 0; i < robot.bones; i++)
                        hand = hand || robot.bone(i) == "hand.r";
                    check(hand);
                    check(robot.animate("wave"));
                }
                if (frames > 10 && frames % 6 == 0)
                    seen.insertLast(held.worldPlace);
                if (frames == 70)
                {
                    // In the hand: about as high as a hand is, beside the robot, and moving as it waves.
                    Vec3 at = seen[0];
                    check(at.y > 0.5f && at.y < 2.5f && (Vec3(at.x, 0, at.z) - Vec3(3, 0, -2)).length() < 1.2f);
                    float moved = 0;
                    for (uint i = 1; i < seen.length(); i++)
                        moved = max(moved, (seen[i] - seen[0]).length());
                    check(moved > 0.05f);
                    // Attached as a whole again, it is where it is put, from the robot's middle.
                    held.attach(robot);
                    held.place = Vec3(0, 3, 0);
                }
                if (frames == 72)
                    check(abs(held.worldPlace.y - 3) < 0.01f && abs(held.worldPlace.x - 3) < 0.01f);
                if (frames == 74)
                {
                    array<string> names = { "idle", "walk", "run" };
                    array<float> at = { 0, 1.5f, 4 };
                    check(robot.animateBlend(names, at));
                    array<string> wrong = { "idle", "fly" };
                    array<float> two = { 0, 1 };
                    check(!robot.animateBlend(wrong, two) && !robot.animateBlend(names, two));
                    robot.blendBy = 2;
                    check(robot.blendBy == 2 && !robot.animated);
                    check(robot.animateLayer("wave", "chest") && !robot.layerPlayed);
                    check(!robot.animateLayer("wave", "tail") && !robot.animateLayer("fly", "chest"));
                    check(robot.lookAt("head", Vec3(10, 1, 0)) && !robot.lookAt("tail", Vec3()));
                    robot.rootMotion = true;
                    check(robot.rootMotion);
                }
                if (frames == 84)
                {
                    robot.stopLayer(0.1f);
                    robot.lookAway();
                    robot.rootMotion = false;
                }
                if (frames == 96)
                {
                    check(robot.layerPlayed && !robot.ragdolled);
                    check(robot.ragdoll(true, Vec3(0, 0, 2)) && robot.ragdolled);
                }
                if (frames == 99)
                    check(robot.ragdoll(false) && !robot.ragdolled);
            }
            void draw(Canvas@ c) { Space::draw(c); }
        }
    )";
    NX_EXPECT(r.screens.init(r.services, "showcase", "main"));
    r.step(2);
    r.screens.push("shapes");
    r.step(104);
    NX_EXPECT(!r.screens.failed());
    r.screens.home();
    r.step(3);
}

NX_TEST(animationsBlendLayerTravelAndLook)
{
    Place p;
    Asset* asset = p.rig.assets.model("global/model/robot.glb");
    p.rig.assets.finish();
    NX_EXPECT(asset->ready && asset->model);
    auto robot = [&](const Vec3& at) {
        const u32 id = p.world.spawn({});
        p.space.spatial(id)->place = at;
        p.space.setModel(id, asset);
        p.space.transform(id & 0xFFFF);
        return id;
    };
    auto holder = [&](u32 parent, const char* bone) {
        const u32 id = p.world.spawn({});
        NX_EXPECT(p.space.attachToBone(id, parent, bone));
        return id;
    };
    auto where = [&](u32 id) {
        p.space.transform(id & 0xFFFF);
        const Mat4& w = p.space.getSpatial(id)->world;
        return Vec3(w.m[12], w.m[13], w.m[14]);
    };
    auto facing = [&](u32 id) {
        p.space.transform(id & 0xFFFF);
        const Mat4& w = p.space.getSpatial(id)->world;
        return Vec3(w.m[8], w.m[9], w.m[10]).normalized();
    };
    const Vec3 apart(5, 0, 0);

    // All the way along a blend is the last animation, in step with it.
    const char* names[] = { "idle", "walk", "run" };
    const float points[] = { 0, 1.5f, 4 };
    const char* wrong[] = { "idle", "fly" };
    const u32 mixed = robot({ 0, 0, 0 }), ran = robot(apart);
    NX_EXPECT(!p.space.blend(mixed, wrong, points, 2, 0));
    NX_EXPECT(p.space.blend(mixed, names, points, 3, 0));
    p.space.moves(mixed)->blendBy = 4;
    NX_EXPECT(p.space.play(ran, "run", true, 0));
    const u32 mixedHand = holder(mixed, "hand.r"), ranHand = holder(ran, "hand.r");
    p.step(20);
    NX_EXPECT(close(where(mixedHand) + apart, where(ranHand), 0.01f));
    NX_EXPECT(!p.space.played(mixed));
    // Between two it is neither, and still moves.
    p.space.moves(mixed)->blendBy = 2.75f;
    const Vec3 between = where(mixedHand);
    p.step(7);
    NX_EXPECT(!close(where(mixedHand) + apart, where(ranHand), 0.01f) && !close(where(mixedHand), between, 0.01f));

    // A layer moves only the bones under it.
    const u32 waving = robot({ 10, 0, 0 }), walking = robot(Vec3(10, 0, 0) + apart);
    NX_EXPECT(p.space.play(waving, "walk", true, 0) && p.space.play(walking, "walk", true, 0));
    NX_EXPECT(p.space.playLayer(waving, "wave", "chest", true, 0.25f));
    NX_EXPECT(!p.space.playLayer(waving, "wave", "tail", true, 0.25f));
    const u32 hands[] = { holder(waving, "hand.r"), holder(walking, "hand.r") };
    const u32 feet[] = { holder(waving, "foot.l"), holder(walking, "foot.l") };
    p.step(30);
    NX_EXPECT(where(hands[0]).y > where(hands[1]).y + 0.3f);
    NX_EXPECT(close(where(feet[0]) + apart, where(feet[1]), 0.001f));
    NX_EXPECT(!p.space.layerPlayed(waving));
    p.space.stopLayer(waving, 0.25f);
    p.step(30);
    NX_EXPECT(p.space.layerPlayed(waving));
    NX_EXPECT(close(where(hands[0]) + apart, where(hands[1]), 0.001f));

    // Root motion: the stride carries it 1.3 metres a step, round the loop, its hips staying over it.
    const u32 striding = robot({ 0, 0, 10 }), still = robot({ 20, 0, 10 }), turned = robot({ 40, 0, 10 });
    p.space.spatial(turned)->turn = Quat::axisAngle({ 0, 1, 0 }, 1.5707963f);
    p.space.moves(striding)->root = p.space.moves(turned)->root = true;
    for (const u32 id : { striding, still, turned })
    {
        NX_EXPECT(p.space.play(id, "stride", true, 0));
    }
    const u32 hips = holder(striding, "hips");
    p.step(60);
    const float expected = 1.3f / 0.86f;
    NX_EXPECT(fabsf(p.at(striding).z - 10 - expected) < 0.05f && fabsf(p.at(striding).x) < 0.001f);
    NX_EXPECT(close(p.at(still), { 20, 0, 10 }));
    NX_EXPECT(fabsf(p.at(turned).x - 40 - expected) < 0.05f && fabsf(p.at(turned).z - 10) < 0.01f);
    const Vec3 h = where(hips);
    NX_EXPECT(fabsf(h.x - p.at(striding).x) < 0.05f && fabsf(h.z - p.at(striding).z) < 0.05f);

    // A walker so carried stops at a wall.
    p.thing({ 0, -0.5f, 30 }, Shape3::Box, { 20, 0.5f, 20 }, 0);
    p.thing({ 0, 1, 33.5f }, Shape3::Box, { 3, 1, 0.5f }, 0);
    const u32 walker = p.walker({ 0, 0, 30 });
    p.space.setModel(walker, asset);
    p.space.moves(walker)->root = true;
    NX_EXPECT(p.space.play(walker, "stride", true, 0));
    p.step(240);
    NX_EXPECT(p.at(walker).z > 32 && p.at(walker).z < 32.65f);

    // Looking: the head turns toward a place to its left, and back.
    const u32 looker = robot({ 0, 0, -20 });
    NX_EXPECT(p.space.play(looker, "idle", true, 0));
    const u32 head = holder(looker, "head");
    p.step(2);
    NX_EXPECT(fabsf(facing(head).x) < 0.3f);
    NX_EXPECT(p.space.lookAt(looker, "head", { 10, 1.5f, -20 }, 1));
    NX_EXPECT(!p.space.lookAt(looker, "tail", { 10, 1.5f, -20 }, 1));
    p.step(60);
    NX_EXPECT(facing(head).x > 0.85f);
    p.space.getAppearance(looker)->moves->lookWanted = 0;
    p.step(60);
    NX_EXPECT(fabsf(facing(head).x) < 0.3f);

    // Reaching: the end of the forearm goes to a place it can reach, and lets go again.
    const u32 reacher = robot({ 0, 0, -30 });
    NX_EXPECT(p.space.play(reacher, "idle", true, 0));
    const u32 wrist = p.world.spawn({});
    NX_EXPECT(p.space.attachToBone(wrist, reacher, "hand.r"));
    p.space.spatial(wrist)->place = { -0.02f, -0.3f, 0 };
    p.step(2);
    const Vec3 resting = where(wrist);
    const Vec3 target(-0.5f, 1.1f, -29.6f);
    NX_EXPECT(!close(resting, target, 0.2f));
    NX_EXPECT(p.space.reach(reacher, "hand.r", target, 1));
    NX_EXPECT(!p.space.reach(reacher, "tail", target, 1) && !p.space.reach(reacher, "hips", target, 1));
    p.step(40);
    NX_EXPECT(close(where(wrist), target, 0.04f));
    NX_EXPECT(p.space.reach(reacher, "hand.r", target, 0));
    p.step(40);
    NX_EXPECT(close(where(wrist), resting, 0.1f));

    p.space.setCamera({ 0, 2, 8 }, { 0, 1, 0 }, 60);
    p.draw(2);
    NX_EXPECT(p.rig.device->stats().rejected == 0);
    p.world.clear();
    p.rig.assets.release(asset);
}

NX_TEST(ragdollsFallAndGetUp)
{
    Place p;
    Asset* asset = p.rig.assets.model("global/model/robot.glb");
    p.rig.assets.finish();
    NX_EXPECT(asset->ready && asset->model);
    p.thing({ 0, -0.5f, 0 }, Shape3::Box, { 30, 0.5f, 30 }, 0);
    const u32 robot = p.walker({ 0, 0, 0 });
    p.space.setModel(robot, asset);
    NX_EXPECT(p.space.play(robot, "run", true, 0));
    const u32 head = p.world.spawn({});
    NX_EXPECT(p.space.attachToBone(head, robot, "head"));
    p.step(20);
    auto where = [&](u32 id) {
        p.space.transform(id & 0xFFFF);
        const Mat4& w = p.space.getSpatial(id)->world;
        return Vec3(w.m[12], w.m[13], w.m[14]);
    };
    const float standing = where(head).y;
    NX_EXPECT(standing > 1.3f);
    const u32 before = p.world.count();

    // Limp, sent backwards: a body for each bone that moves skin, and down it goes.
    NX_EXPECT(!p.space.ragdolled(robot));
    NX_EXPECT(p.space.ragdoll(robot, true, 60, { 0, 1, -3 }, 0.5f));
    NX_EXPECT(p.space.ragdolled(robot));
    const u32 bodies = p.world.count() - before;
    NX_EXPECT(bodies >= 10 && bodies <= 12);
    NX_EXPECT(p.space.joints() == bodies - 1);
    p.step(180);
    NX_EXPECT(where(head).y < 0.7f && where(head).y > 0);
    // It went where it was sent, and lies on the ground rather than under it.
    NX_EXPECT(p.at(robot).z < -0.3f && p.at(robot).y > -0.2f && p.at(robot).y < 0.4f);
    const Ragdoll& limp = *p.space.getAppearance(robot)->moves->ragdoll;
    for (const u32 part : limp.parts)
    {
        if (part)
        {
            NX_EXPECT(p.space.getCollider(part)->together == robot && p.space.getSpatial(part)->place.y > -0.05f);
        }
    }
    // What hits a body hits it.
    u32 hit = 0;
    Vec3 at, normal;
    const Vec3 lying = where(head);
    NX_EXPECT(p.space.raycast(lying + Vec3(0, 3, 0), lying - Vec3(0, 1, 0), 0xFFFF, 0, hit, at, normal) && hit == robot);
    NX_EXPECT(!p.space.raycast(lying + Vec3(0, 3, 0), lying + Vec3(0, -0.1f, 0), 0xFFFF, robot, hit, at, normal) || hit != robot);
    // A blow moves it.
    const Vec3 was = p.at(robot);
    p.space.push(robot, Vec3(60 * 4, 0, 0), nullptr);
    p.step(30);
    NX_EXPECT(p.at(robot).x > was.x + 0.3f);

    // Up again, into what it plays: the bodies go and it stands.
    NX_EXPECT(p.space.ragdoll(robot, false, 60, {}, 0.5f));
    NX_EXPECT(!p.space.ragdolled(robot));
    p.step(2);
    NX_EXPECT(p.world.count() == before && p.space.joints() == 0);
    NX_EXPECT(where(head).y < standing - 0.3f);
    p.step(40);
    NX_EXPECT(where(head).y > standing - 0.25f);

    // Down again, and gone with it.
    NX_EXPECT(p.space.ragdoll(robot, true, 60, {}, 0.5f));
    NX_EXPECT(p.world.count() > before);
    p.step(10);
    p.space.setCamera({ 0, 2, 8 }, { 0, 1, 0 }, 60);
    p.draw(2);
    NX_EXPECT(p.rig.device->stats().rejected == 0);
    p.world.destroyEntity(robot);
    p.step(2);
    NX_EXPECT(p.world.count() == before - 2 && p.space.joints() == 0);

    // A shape with no bones can't.
    const u32 box = p.thing({ 5, 1, 0 }, Shape3::Box, { 0.5f, 0.5f, 0.5f }, 1);
    NX_EXPECT(!p.space.ragdoll(box, true, 60, {}, 0.5f));
    p.rig.assets.release(asset);
}

NX_TEST(decalsLieOverWhatIsThere)
{
    Place p;
    p.space.setGround(0);
    // On the ground: a square, just above it.
    const u32 mark = p.space.decal(nullptr, { 0, 0, 0 }, { 0, 1, 0 }, 1, 0, { 1, 1, 1, 1 });
    NX_EXPECT(mark != 0);
    const CustomMesh* m = p.space.getAppearance(mark)->custom;
    NX_EXPECT(m && m->data.indices.count() == 6);
    for (const gpu::MeshVertex& v : m->data.vertices)
    {
        NX_EXPECT(fabsf(v.x) <= 0.501f && fabsf(v.z) <= 0.501f && v.y > 0 && v.y < 0.02f && v.ny > 0.99f);
        NX_EXPECT(v.u >= -0.001f && v.u <= 1.001f && v.v >= -0.001f && v.v <= 1.001f);
    }
    // Over the corner of a box: only its front, cut at its edges.
    CustomMesh* box = create<CustomMesh>(MemTag::General);
    box->device = p.rig.device;
    box->data.box({ 0, 0, 0 }, { 1, 1, 1 }, { 1, 1, 1, 1 });
    const u32 cube = p.world.spawn({});
    p.space.spatial(cube)->place = { 10, 1, 0 };
    p.space.transform(cube & 0xFFFF);
    p.space.setCustom(cube, box);
    box->release();
    const u32 corner = p.space.decal(nullptr, { 11, 2, 1 }, { 0, 0, 1 }, 1, 30, { 1, 0, 0, 1 });
    NX_EXPECT(corner != 0);
    const CustomMesh* c = p.space.getAppearance(corner)->custom;
    NX_EXPECT(c && c->data.indices.count() >= 3);
    for (const gpu::MeshVertex& v : c ? c->data.vertices : m->data.vertices)
    {
        NX_EXPECT(v.x <= 11.001f && v.y <= 2.001f && v.z > 1 && v.z < 1.02f && v.nz > 0.99f);
    }
    // Nowhere: nothing.
    NX_EXPECT(p.space.decal(nullptr, { 100, 50, 100 }, { 0, 1, 0 }, 1, 0, { 1, 1, 1, 1 }) == 0);
    // Past the limit, the oldest go.
    p.space.setDecalLimit(3);
    u32 made[5];
    for (u32 i = 0; i < 5; i++)
    {
        made[i] = p.space.decal(nullptr, { static_cast<float>(i) * 2, 0, 5 }, { 0, 1, 0 }, 1, 0, { 1, 1, 1, 1 });
    }
    p.step(2);
    NX_EXPECT(!p.world.alive(mark) && !p.world.alive(made[0]) && !p.world.alive(made[1]) && p.world.alive(made[2]) && p.world.alive(made[4]));
    p.space.setCamera({ 0, 3, 8 }, { 0, 0, 0 }, 60);
    p.draw(2);
    NX_EXPECT(p.rig.device->stats().rejected == 0);
}

NX_TEST(animationStatesTravel)
{
    Rig r;
    NX_EXPECT(r.ok);
    r.platform.extraPath = "showcase/shapes/main.as";
    // The script's include, put together so that the check for what the engine includes doesn't take it for one of its own.
    r.platform.extraText = std::string("#") + R"(include "global:states.as"
        class A : Screen
        {
            Entity@ robot;
            AnimationStates@ moves;
            array<string> been;
            int frames = 0;
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            void came(const string &in state) { been.insertLast(state); }
            void onEnter()
            {
                @robot = Space::spawn(Vec3());
                robot.model(Assets::model("global:model/robot.glb"));
                @moves = AnimationStates(robot);
                @moves.entered = AnimationEntered(this.came);
                moves.add("idle");
                moves.add("walk");
                moves.add("leap", "jump", false, "idle");
                moves.link("idle", "walk", 0.2f, true);
                moves.link("walk", "leap", 0.1f);
                moves.start("idle");
                check(!moves.travel("nowhere"));
            }
            void update(float dt)
            {
                frames++;
                moves.update(dt);
                if (frames == 10)
                    check(moves.travel("leap") && moves.route.length() == 2);
                if (frames == 120)
                {
                    // Idle, through walk to the leap, and back to idle once it was done.
                    check(been.length() == 4 && been[0] == "idle" && been[1] == "walk" && been[2] == "leap" && been[3] == "idle");
                    check(moves.state == "idle" && !moves.finished);
                    Screens::home();
                }
            }
            void draw(Canvas@ c) { Space::draw(c); }
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

NX_TEST(scriptsDrawIntoPictures)
{
    Rig r;
    NX_EXPECT(r.ok);
    r.platform.extraPath = "showcase/shapes/main.as";
    r.platform.extraText = R"(
        class A : Screen
        {
            Texture@ map = Assets::picture(128, 96);
            int drawn = 0;
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            void onEnter()
            {
                check(map !is null && map.width == 128 && map.height == 96 && map.ready);
                check(Assets::picture(0, 5) is null && Assets::picture(5000, 5) is null);
                Space::ground(0);
                Space::spawn(Vec3(0, 1, 0)).shape(SolidBox, Red);
            }
            void draw(Canvas@ c)
            {
                c.designHeight(720);
                check(c.beginPicture(map, Color(0, 0, 0, 1)));
                check(c.width == 128 && c.height == 96);
                Space::camera(Vec3(0, 10, 0.1f), Vec3(0, 0, 0));
                Space::draw(c);
                c.endPicture();
                check(abs(c.height - 720) < 0.01f);
                Space::camera(Vec3(0, 3, 6), Vec3(0, 1, 0));
                Space::draw(c);
                c.image(map, 10, 10, 128, 96);
                drawn++;
            }
        }
    )";
    NX_EXPECT(r.screens.init(r.services, "showcase", "main"));
    r.step(2);
    r.screens.push("shapes");
    r.step(10);
    NX_EXPECT(!r.screens.failed());
    NX_EXPECT(r.device->stats().rejected == 0);
    r.screens.home();
    r.step(3);
}

NX_TEST(lightsOnThePlane)
{
    Rig r;
    NX_EXPECT(r.ok);
    r.platform.extraPath = "showcase/shapes/main.as";
    r.platform.extraText = R"(
        class A : Screen
        {
            Entity@ lamp;
            Entity@ crate;
            void check(bool good) { if (!good) { array<int> nope; nope[1] = 0; } }
            void onEnter()
            {
                check(Scene::ambient.r == 1 && Scene::lightLayer == 32767);   // off, as it starts
                Scene::ambient = Color(0.1f, 0.1f, 0.2f);
                Scene::lightLayer = 5;
                @lamp = Scene::spawn(Vec2(200, 200));
                lamp.disc(8, Yellow);
                lamp.light(Color(1, 0.8f, 0.5f), 250);
                @crate = Scene::spawn(Vec2(300, 200));
                crate.rect(40, 40, Gray);
                check(!crate.occluder);
                crate.box(40, 40);
                crate.occluder = true;
                check(crate.occluder);
                Entity@ spark = Scene::spawn(Vec2(250, 260));
                spark.disc(4, White);
                spark.layer = 10;                  // above the light: drawn as it is
            }
            void update(float dt)
            {
                lamp.position = lamp.position + Vec2(30 * dt, 0);
            }
            void draw(Canvas@ c)
            {
                c.design(1280, 720);
                Scene::draw(c);
                Scene::draw(c, 6, 32767);          // only what is above the light: no lighting again
            }
            void onExit()
            {
                lamp.light(White, 0);              // put out
            }
        }
    )";
    NX_EXPECT(r.screens.init(r.services, "showcase", "main"));
    r.step(2);
    r.screens.push("shapes");
    r.step(10);
    NX_EXPECT(!r.screens.failed());
    NX_EXPECT(r.device->stats().rejected == 0);
    r.screens.home();
    r.step(3);
}

NX_TEST(scriptsUseSpace)
{
    Rig r;
    NX_EXPECT(r.ok);
    r.platform.extraPath = "showcase/shapes/main.as";
    r.platform.extraText = R"(
        class Coin : Behaviour
        {
            void update(float dt) { entity.turn(Vec3(0, 1, 0), 90 * dt); }
            void onHit(Entity@ other)
            {
                print("got by " + other.name);
                entity.destroy();
            }
        }
        class A : Screen
        {
            Entity@ hero;
            Entity@ crate;
            Mesh@ roof = Mesh();
            void onEnter()
            {
                Space::ground(0);
                Space::sun(Vec3(1, 2, 1), White);
                Space::shadows(0.5, 30);
                roof.box(Vec3(), Vec3(1, 2, 3), Red);
                uint first = roof.vertices;
                roof.cylinder(Vec3(), 1, 1, 0);
                roof.move(first, Vec3(0, 1, 0), Vec3(0, 45, 0));
                @hero = Space::spawn(Vec3(0, 0, 0));
                hero.name = "hero";
                hero.mesh(roof, Blue);
                hero.capsule(0.4, 1.8, 1);
                hero.colliderAt = Vec3(0, 0.9, 0);
                hero.walker = true;
                Entity@ coin = Space::spawn(Vec3(3, 1, 0));
                coin.shape(SolidTorus, Yellow);
                coin.sphere(0.5, 2, 1 << 1);
                coin.add(Coin());
                @crate = Space::spawn(Vec3(0, 4, 5));
                crate.shape(SolidBox);
                crate.cube(Vec3(1, 1, 1));
                crate.mass = 2;
                crate.gravity = 1;
                crate.shine = 0.5;
            }
            void update(float dt)
            {
                hero.motion = Vec3(3, hero.motion.y, 0);
                hero.face(hero.motion);
                Space::camera(hero.place + Vec3(0, 3, 6), hero.place);
                Vec3 hit;
                Vec3 normal;
                if (Space::raycast(Vec3(0, 5, 0), Vec3(0, -5, 0), hit, normal) && abs(hit.y) > 1.9) print("wrong");
                Space::clearField();
                Space::field(FieldSphere, Vec3(0, 1, 0), Vec3(1, 1, 1), Green, 0.2);
                Vec3 f = hero.forward;
                if (f.x < 0.9) print("facing wrong");
            }
            void draw(Canvas@ c)
            {
                c.design(1280, 720);
                Space::draw(c, 0, 0, 640, 720);
                Space::draw(c, 640, 0, 640, 720);
                c.text(Assets::font("global:freesans.sfn", 20), "x", 10, 10);
            }
        }
    )";
    NX_EXPECT(r.screens.init(r.services, "showcase", "main"));
    r.step(2);
    r.screens.push("shapes");
    r.step(120);
    NX_EXPECT(!r.screens.failed());
    NX_EXPECT(r.device->stats().rejected == 0);
    World* world = r.services.world;
    NX_EXPECT(world && world->count() == 2);  // the coin has gone
    r.screens.home();
    r.step(3);
}

NX_TEST(regionsComeAndGo)
{
    Place p;
    NX_EXPECT(p.space.makeEndlessLand(9, 2, 30, 0.5f, 3));
    p.space.setCamera({ 0, 10, 0 }, { 0, 0, -10 }, 60);
    const float before = p.space.heightAt(500, 500);  // before there is a region there
    p.step();
    const u32 first = p.space.regions();
    NX_EXPECT(first >= 12);
    u32 arrived = 0, left = 0;
    int x = 0, z = 0;
    bool came = false;
    while (p.space.nextRegion(x, z, came))
    {
        arrived += came;
    }
    NX_EXPECT(arrived == first);
    const u32 tree = p.world.spawn({});
    p.space.spatial(tree)->place = { 5, 0, 5 };
    p.space.setStreamed(tree, true);
    const u32 kept = p.world.spawn({});
    p.space.spatial(kept)->place = { 5, 0, 5 };
    for (u32 i = 0; i < 100; i++)
    {
        p.space.setCamera({ 0, 10, 0 }, { 0, 0, -10 }, 60);
        p.step();
    }
    NX_EXPECT(p.space.regions() > first && p.space.regionsWaiting() == 0);
    // Far away, and back.
    for (u32 i = 0; i < 400; i++)
    {
        p.space.setCamera({ 500, 10, 500 }, { 0, 0, -10 }, 60);
        p.step();
    }
    NX_EXPECT(p.space.heightAt(500, 500) == before);
    NX_EXPECT(!p.world.alive(tree) && p.world.alive(kept));
    while (p.space.nextRegion(x, z, came))
    {
        left += !came;
    }
    NX_EXPECT(left >= first);
    NX_EXPECT(p.space.regions() < 70);
    // Their edges meet, whichever of them is there.
    for (int i = -3; i < 3; i++)
    {
        const float edge = static_cast<float>(i) * p.space.regionSize();
        NX_EXPECT(fabsf(p.space.heightAt(edge - 0.001f, 3) - p.space.heightAt(edge + 0.001f, 3)) < 0.02f);
    }
    const size_t settled = memStats(MemTag::Scene).bytes;
    for (u32 i = 0; i < 2000; i++)
    {
        p.space.setCamera({ static_cast<float>(i) * 2, 10, 500 }, { 0, 0, -10 }, 60);
        p.step();
        while (p.space.nextRegion(x, z, came))
        {
        }
    }
    NX_EXPECT(memStats(MemTag::Scene).bytes < settled + 64 * 1024);
}

NX_TEST(thingsComeFromFiles)
{
    Place p;
    p.space.setGround(0);
    p.rig.platform.extraPath = "region.json";
    p.rig.platform.extraText = R"({ "entities": [
        { "shape": "box", "color": "#ff0000", "place": [1, 2, 3], "cube": [1, 1, 1], "mass": 2, "name": "crate", "sway": 0.2 },
        { "model": "global:model/rock.glb", "place": [4, 0, 0], "onLand": true, "angles": 90, "size": 2, "sphere": 0.8, "wall": true, "group": 2 },
        { "light": [1, 1, 0.5], "range": 6, "glow": 2 } ] })";
    NX_EXPECT(p.space.load("region.json", "showcase/shapes", { 10, 0, 0 }, true) == 3);
    const u32 crate = p.world.findNamed("crate");
    NX_EXPECT(crate && close(p.at(crate), { 11, 2, 3 }));
    NX_EXPECT(p.space.getBody(crate)->mass == 2 && p.space.getBody(crate)->gravity == 1);
    NX_EXPECT(p.space.getAppearance(crate) && fabsf(p.space.getAppearance(crate)->sway - 0.2f) < 1e-5f);
    // Leaning in the wind is only how it is drawn: where it is stays the same.
    p.space.setWind({ 3, 0, 0 });
    p.step(120);
    NX_EXPECT(fabsf(p.at(crate).y - 0.5f) < 0.02f);
    p.rig.platform.extraText = R"({ "entities": [
        { "shape": "blob" } ] })";
    NX_EXPECT(p.space.load("region.json", "showcase/shapes", {}, false) == -1);
    NX_EXPECT(p.world.takeError().find("region.json:2") != std::string::npos);
    p.rig.platform.extraText = R"({ "entities": [ { "shap": "box" } ] })";
    NX_EXPECT(p.space.load("region.json", "showcase/shapes", {}, false) == -1);
    NX_EXPECT(p.space.load("none.json", "showcase/shapes", {}, false) == -1);
}

NX_TEST(waysRoundWhatIsInTheWay)
{
    Place p;
    p.space.setGround(0);
    // A wall across the middle, with a gap only at one end; and a low step that can be climbed.
    p.rig.platform.extraPath = "walls.json";
    p.rig.platform.extraText = R"({ "entities": [
        { "shape": "box", "place": [-6, 1.5, 0], "size": [32, 3, 1], "cube": [1, 1, 1], "wall": true, "group": 2 },
        { "shape": "box", "place": [12, 0.15, 8], "size": [4, 0.3, 4], "cube": [1, 1, 1], "wall": true, "group": 2 } ] })";
    NX_EXPECT(p.space.load("walls.json", "showcase/shapes", {}, false) == 2);
    p.step(2);
    NX_EXPECT(p.space.navigate({ 0, 0, 0 }, 40, 1, 0.6f, 0xFFFD));
    NX_EXPECT(!p.space.walkable({ 0, 0, 0 }) && p.space.walkable({ 0, 0, 5 }) && p.space.walkable({ 12, 0.3f, 8 }));
    GrowArray<Vec3> way;
    NX_EXPECT(p.space.findPath({ 0, 0, -6 }, { 0, 0, 6 }, way));
    // Round the end of the wall, at x 10 or so, never through it.
    bool round = false;
    for (u32 i = 0; i + 1 < way.count(); i++)
    {
        round = round || way[i + 1].x > 9.5f;
        for (u32 k = 0; k <= 10; k++)
        {
            const Vec3 at = way[i] + (way[i + 1] - way[i]) * (static_cast<float>(k) / 10);
            NX_EXPECT(!(at.x < 10.2f && fabsf(at.z) < 0.9f));
        }
    }
    NX_EXPECT(round && way.count() >= 3 && way.count() < 8);
    NX_EXPECT(fabsf(way[way.count() - 1].z - 6) < 0.01f);
    // Straight, where nothing is in the way.
    NX_EXPECT(p.space.findPath({ -15, 0, 5 }, { 15, 0, 5 }, way) && way.count() == 2);
    // Somewhere that can't be reached: as near as can be got.
    p.rig.platform.extraText = R"({ "entities": [
        { "shape": "box", "place": [0, 1.5, -15], "size": [40, 3, 1], "cube": [1, 1, 1], "wall": true, "group": 2 } ] })";
    NX_EXPECT(p.space.load("walls.json", "showcase/shapes", {}, false) == 1);
    p.step(2);
    NX_EXPECT(p.space.navigate({ 0, 0, 0 }, 40, 1, 0.6f, 0xFFFD));
    NX_EXPECT(!p.space.findPath({ 0, 0, -6 }, { 0, 0, -18 }, way) && way.count() >= 2 && way[way.count() - 1].z > -15);
}

NX_TEST(mistsComeAndGo)
{
    Place p;
    const u32 a = p.space.addMist({ 0, 2, 0 }, { 20, 4, 20 }, { 1, 1, 1, 0.6f }, 1, 1, 6);
    const u32 b = p.space.addMist({ 5, 1, 5 }, { 2, 2, 2 }, { 1, 0.5f, 0.1f, 0.9f }, 1, 5, 1);
    NX_EXPECT(a && b && a != b && p.space.mists() == 2);
    NX_EXPECT(p.space.moveMist(b, { 6, 1, 6 }) && !p.space.moveMist(999, {}));
    p.space.removeMist(a);
    NX_EXPECT(p.space.mists() == 1);
    p.space.clearMists();
    NX_EXPECT(p.space.mists() == 0);
}

NX_TEST(blocksAreDugAndBuiltAndStoodOn)
{
    Place p;
    NX_EXPECT(!p.space.makeVoxels({}, 4096, 16, 16, 1) && !p.space.hasVoxels());
    NX_EXPECT(p.space.makeVoxels({ -16, 0, -16 }, 32, 16, 32, 1));
    // A floor four high, with a wall on it.
    NX_EXPECT(p.space.fillVoxels(0, 0, 0, 31, 3, 31, 3) == 32 * 4 * 32);
    NX_EXPECT(p.space.fillVoxels(24, 4, 0, 24, 8, 31, 7) == 5 * 32);
    NX_EXPECT(p.space.voxel(5, 3, 5) == 3 && p.space.voxel(5, 4, 5) == 0 && p.space.voxel(-1, 0, 0) == 0 && p.space.voxelTop(5, 5) == 3);
    NX_EXPECT(close(p.space.voxelCell({ 0.5f, 3.5f, -15.5f }), { 16, 3, 0 }) && close(p.space.voxelPlace(16, 3, 0), { 0.5f, 3.5f, -15.5f }));
    // Seen from above, and the face it meets looks up.
    Vec3 cell, face, hit;
    NX_EXPECT(p.space.voxelRay({ 0.5f, 10, 0.5f }, { 0.5f, -10, 0.5f }, cell, face, hit) && close(cell, { 16, 3, 16 }) && close(face, { 0, 1, 0 }) && fabsf(hit.y - 4) < 0.01f);
    // Sideways into the wall, whose near face looks back.
    NX_EXPECT(p.space.voxelRay({ 0, 5.5f, 0 }, { 20, 5.5f, 0 }, cell, face, hit) && cell.x == 24 && close(face, { -1, 0, 0 }) && fabsf(hit.x - 8) < 0.01f);
    u32 id = 1;
    Vec3 normal;
    NX_EXPECT(p.space.raycast({ 0.5f, 10, 0.5f }, { 0.5f, -10, 0.5f }, 1, 0, id, hit, normal) && id == 0 && fabsf(hit.y - 4) < 0.01f);
    // Walked on, and stopped by the wall.
    const u32 w = p.walker({ 0, 4.5f, 0 });
    p.step(60);
    NX_EXPECT(p.space.getBody(w)->onGround && fabsf(p.at(w).y - 4) < 0.05f);
    for (u32 i = 0; i < 150; i++)
    {
        p.space.body(w)->velocity = { 4, p.space.body(w)->velocity.y, 0 };
        p.step();
    }
    NX_EXPECT(fabsf(p.at(w).x - 7.6f) < 0.06f && fabsf(p.at(w).y - 4) < 0.05f);
    // Dug out from under it, it falls into the hole.
    NX_EXPECT(p.space.voxelBall(p.at(w) + Vec3(-0.5f, -1, 0), 2.2f, 0) > 10);
    p.step(60);
    NX_EXPECT(p.at(w).y < 3.1f);
    // Something dropped rests on top.
    const u32 box = p.thing({ -8, 8, -8 }, Shape3::Box, { 0.4f, 0.4f, 0.4f }, 1);
    p.step(120);
    NX_EXPECT(fabsf(p.at(box).y - 4.4f) < 0.06f);
    // Made into triangles as they are drawn, and changed when they change.
    p.draw(30);
    NX_EXPECT(p.space.voxelChunks() == 4 && p.space.voxelsWaiting() == 0);
    NX_EXPECT(p.space.setVoxel(2, 4, 2, 9) && !p.space.setVoxel(2, 4, 2, 9) && p.space.voxelsWaiting() > 0);
    p.draw();
    NX_EXPECT(p.space.voxelsWaiting() == 0);
    // Hills, and they go away.
    p.space.clearVoxels();
    NX_EXPECT(p.space.makeVoxels({}, 64, 32, 64, 0.5f));
    p.space.voxelHills(3, 12, 0.5f, 6, 0.6f);
    NX_EXPECT(p.space.voxelTop(10, 10) > 2 && p.space.voxelTop(10, 10) < 32 && p.space.voxel(10, 0, 10) == 10);
    p.space.clearVoxels();
    NX_EXPECT(!p.space.hasVoxels() && p.space.voxel(10, 0, 10) == 0);
}

NX_TEST(everyModelCanBeUsed)
{
    Place p;
    for (const char* name : { "robot", "house", "well", "lantern", "pine", "oak", "bush", "rock", "crystal", "mushroom", "grass" })
    {
        char path[96];
        snprintf(path, sizeof(path), "global/model/%s.glb", name);
        Asset* asset = p.rig.assets.model(path);
        p.rig.assets.finish();
        NX_EXPECT(asset->ready && !asset->failed && asset->model && asset->model->radius > 0.1f);
        if (asset->model)
        {
            // Each level is simpler than the one before.
            u32 counts[kModelDetails] = {};
            for (const ModelPart& part : asset->model->parts)
            {
                for (u32 d = 0; d < kModelDetails; d++)
                {
                    counts[d] += part.count[d];
                }
            }
            NX_EXPECT(counts[0] > 100 && counts[1] <= counts[0] && counts[2] <= counts[1] && counts[2] > 0);
        }
        const u32 id = p.world.spawn({});
        p.space.setModel(id, asset);
        if (strcmp(name, "robot") == 0)
        {
            NX_EXPECT(asset->model->animations.count() == 7 && asset->model->weights.count() > 0);
            NX_EXPECT(p.space.play(id, "walk", true, 0.2f));
            p.step(10);
            NX_EXPECT(p.space.play(id, "jump", false, 0.2f));
            NX_EXPECT(!p.space.play(id, "fly", true, 0.2f));
            p.step(60);
            NX_EXPECT(p.space.played(id));
        }
        p.rig.assets.release(asset);
    }
    p.space.setCamera({ 0, 2, 8 }, { 0, 1, 0 }, 60);
    p.draw(2);
    NX_EXPECT(p.rig.device->stats().rejected == 0);
    // One that isn't a model.
    Asset* bad = p.rig.assets.model("showcase/main/main.as");
    p.rig.assets.finish();
    NX_EXPECT(bad->failed);
    p.rig.assets.release(bad);
}

NX_TEST(particlesFlyInSpace)
{
    Place p;
    p.rig.platform.extraPath = "scene.json";
    p.rig.platform.extraText = R"({ "particles": {
        "spark": { "life": [0.5, 1], "speed": [4, 5], "size": [0.2, 0], "spread": 60, "color": "#ffff00", "gravity": 1 },
        "smoke": { "life": 2, "speed": 1, "size": [0.5, 2], "spread": 20, "color": "#808080", "blend": "alpha", "fadeIn": 0.2, "spin": 90 } } })";
    NX_EXPECT(p.world.load("scene.json", "showcase/shapes"));
    NX_EXPECT(p.space.burst("spark", { 0, 1, 0 }, 100, { 0, 1, 0 }));
    NX_EXPECT(!p.space.burst("none", { 0, 1, 0 }, 100, { 0, 1, 0 }));
    NX_EXPECT(p.space.particles() == 100);
    const u32 chimney = p.world.spawn({});
    p.space.spatial(chimney)->place = { 3, 0, 0 };
    NX_EXPECT(p.space.emit(chimney, "smoke", 30));
    p.step(60);
    NX_EXPECT(p.space.particles() > 20 && p.space.particles() < 60);
    p.space.setCamera({ 0, 2, 8 }, { 0, 1, 0 }, 60);
    p.draw(3);
    NX_EXPECT(p.rig.device->stats().rejected == 0 && p.rig.device->stats().triangles >= 40);
    p.world.destroyEntity(chimney);
    p.step(200);
    NX_EXPECT(p.space.particles() == 0);
    // Never more than there is room for.
    p.space.burst("smoke", {}, 100000, { 0, 1, 0 });
    NX_EXPECT(p.space.particles() <= 500);
    p.world.clear();
    NX_EXPECT(p.space.particles() == 0);
}

NX_TEST(soundsComeFromPlaces)
{
    Place p;
    p.space.setCamera({ 0, 0, 0 }, { 0, 0, -1 }, 60);
    const u32 source = p.world.spawn({});
    p.space.spatial(source)->place = { 10, 0, 0 };
    p.step();
    float gain, pan;
    p.space.hear({ 10, 0, 0 }, 1, 20, gain, pan);
    NX_EXPECT(fabsf(gain - 0.25f) < 0.01f && pan > 0.8f);
    p.space.hear({ -10, 0, 0 }, 1, 20, gain, pan);
    NX_EXPECT(pan < -0.8f);
    p.space.hear({ 0, 0, -10 }, 1, 20, gain, pan);
    NX_EXPECT(fabsf(pan) < 0.01f);
    p.space.hear({ 30, 0, 0 }, 1, 20, gain, pan);
    NX_EXPECT(gain == 0);
    // It goes with its entity, and ends with it.
    Asset* asset = p.rig.assets.sound("global/sound/bell.wav");
    p.rig.assets.finish();
    PlayOptions o;
    o.loop = true;
    p.rig.assets.addRef(asset);
    const Voice voice = p.rig.audio.play(*asset->sound, asset, o);
    p.space.follow(voice, source, {}, 1, 20);
    i16 heard[800 * 2];
    const auto loudness = [&]()
    {
        i32 left = 0, right = 0;
        for (u32 i = 0; i < 30; i++)
        {
            p.space.setCamera({ 0, 0, 0 }, { 0, 0, -1 }, 60);
            p.step();
            p.rig.audio.update();
            p.rig.audio.mix(heard, 800);
        }
        for (u32 i = 0; i < 800; i++)
        {
            left += heard[i * 2] < 0 ? -heard[i * 2] : heard[i * 2];
            right += heard[i * 2 + 1] < 0 ? -heard[i * 2 + 1] : heard[i * 2 + 1];
        }
        return Vec2(static_cast<float>(left), static_cast<float>(right));
    };
    const Vec2 near = loudness();
    NX_EXPECT(near.y > near.x * 2 && near.y > 1000);
    p.space.spatial(source)->place = { 100, 0, 0 };
    const Vec2 far = loudness();
    NX_EXPECT(far.y < near.y * 0.1f);
    p.world.destroyEntity(source);
    loudness();
    NX_EXPECT(!p.rig.audio.playing(voice));
    p.rig.assets.release(asset);
}
