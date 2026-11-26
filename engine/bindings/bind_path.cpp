#include "base/noise.h"
#include "base/path.h"
#include "bindings/script_refs.h"

#include <math.h>
#include <scriptarray/scriptarray.h>

// Ways through points, and smooth noise: for things to follow, and for worlds made by script.
namespace nx
{
    namespace
    {
        Path* makePath() { return create<Path>(MemTag::Script); }
        Path* makePathFrom(const CScriptArray& points, bool loop)
        {
            Path* p = makePath();
            for (asUINT i = 0; p && i < points.GetSize(); i++)
            {
                p->add(*static_cast<const Vec3*>(points.At(i)));
            }
            if (p)
            {
                p->setLoop(loop);
            }
            return p;
        }
        void pathAdd(const Vec3& point, Path* p) { p->add(point); }
        void pathAdd2(const Vec2& point, Path* p) { p->add(Vec3(point.x, point.y, 0)); }
        Vec2 pathAt2(float distance, Path* p) { const Vec3 v = p->at(distance); return Vec2(v.x, v.y); }
        Vec2 pathDirection2(float distance, Path* p)
        {
            const Vec3 v = p->direction(distance);
            const float far = sqrtf(v.x * v.x + v.y * v.y);
            return far > 1e-6f ? Vec2(v.x / far, v.y / far) : Vec2(1, 0);
        }
        float pathNearest2(const Vec2& place, Path* p) { return p->nearest(Vec3(place.x, place.y, 0)); }

        float noise1(float x, float y, float z) { return noise(x, y, z); }
        float noise2(const Vec2& v) { return noise(v.x, v.y, 0.5f); }
        float noise3(const Vec3& v) { return noise(v.x, v.y, v.z); }
        float fractal(float x, float y, float z, u32 octaves, float roughness) { return fractalNoise(x, y, z, octaves, roughness); }
        void noiseSeed(u32 seed) { setNoiseSeed(seed); }
    }

    void bindPath(Binder& b)
    {
        b.section("Paths and noise", "Ways for things to follow, and smooth randomness to make worlds with.");
        b.refType("Path", 0, "A smooth way through points, passing through each one. Places on it are asked for by how far along it they are, in the same units as the points, so that going along at a steady speed is steady. In 2D its points are Vec2s, in space Vec3s.");
        b.behaviour(asBEHAVE_FACTORY, "Path@ f()", NX_FN(makePath));
        b.behaviour(asBEHAVE_FACTORY, "Path@ f(const array<Vec3> &in points, bool loop = false)", NX_FN(makePathFrom));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Path, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Path, release));
        b.method("void add(const Vec3 &in point)", NX_OBJ_LAST(pathAdd), "Another point at the end.");
        b.method("void add(const Vec2 &in point)", NX_OBJ_LAST(pathAdd2), "The same, on the plane.");
        b.method("void clear()", NX_METHOD(Path, clear), "No points.");
        b.method("uint get_count() const property", NX_METHOD(Path, count), "How many points.");
        b.method("Vec3 point(uint index) const", NX_METHOD(Path, point), "One of them.");
        b.method("void setPoint(uint index, const Vec3 &in point)", NX_METHOD(Path, setPoint), "Moves one.");
        b.method("bool get_loop() const property", NX_METHOD(Path, loop), "Whether it goes back from the last point to the first.");
        b.method("void set_loop(bool) property", NX_METHOD(Path, setLoop), "Whether it goes back from the last point to the first.");
        b.method("float get_length() property", NX_METHOD(Path, length), "All the way along.");
        b.method("Vec3 at(float distance)", NX_METHOD(Path, at), "The place so far along: round and round on a loop, and kept to the ends otherwise.");
        b.method("Vec3 direction(float distance)", NX_METHOD(Path, direction), "Which way it goes there, a unit long: for turning what follows it.");
        b.method("float nearest(const Vec3 &in place)", NX_METHOD(Path, nearest), "How far along it the nearest place to this one is.");
        b.method("Vec2 at2(float distance)", NX_OBJ_LAST(pathAt2), "at(), on the plane.");
        b.method("Vec2 direction2(float distance)", NX_OBJ_LAST(pathDirection2), "direction(), on the plane.");
        b.method("float nearest(const Vec2 &in place)", NX_OBJ_LAST(pathNearest2), "nearest(), on the plane.");

        b.function("float noise(float x, float y = 0, float z = 0)", NX_FN(noise1), "Smooth noise, -1 to 1, with hills and dips about 1 apart: the same for the same place every time. Scale the place for bigger or smaller ones; move through z for noise that changes with time.");
        b.function("float noise(const Vec2 &in place)", NX_FN(noise2), "The same, on the plane.");
        b.function("float noise(const Vec3 &in place)", NX_FN(noise3), "The same, in space.");
        b.function("float fractalNoise(float x, float y, float z = 0, uint octaves = 4, float roughness = 0.5f)", NX_FN(fractal), "Layers of it, each twice as fine and roughness as strong as the last, still -1 to 1: land, clouds, fire.");
        b.function("void noiseSeed(uint seed)", NX_FN(noiseSeed), "Another pattern, the same every time for the same seed.");
    }
}
