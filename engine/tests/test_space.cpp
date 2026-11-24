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
        { "shape": "box", "color": "#ff0000", "place": [1, 2, 3], "cube": [1, 1, 1], "mass": 2, "name": "crate" },
        { "model": "global:model/rock.glb", "place": [4, 0, 0], "onLand": true, "angles": 90, "size": 2, "sphere": 0.8, "wall": true, "group": 2 },
        { "light": [1, 1, 0.5], "range": 6, "glow": 2 } ] })";
    NX_EXPECT(p.space.load("region.json", "showcase/shapes", { 10, 0, 0 }, true) == 3);
    const u32 crate = p.world.findNamed("crate");
    NX_EXPECT(crate && close(p.at(crate), { 11, 2, 3 }));
    NX_EXPECT(p.space.getBody(crate)->mass == 2 && p.space.getBody(crate)->gravity == 1);
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

NX_TEST(everyModelCanBeUsed)
{
    Place p;
    for (const char* name : { "robot", "house", "well", "lantern", "pine", "oak", "bush", "rock", "crystal", "mushroom" })
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
            NX_EXPECT(asset->model->animations.count() == 6 && asset->model->weights.count() > 0);
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
