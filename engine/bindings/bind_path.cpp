#include "base/curve.h"
#include "base/noise.h"
#include "base/shapes2d.h"
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

        CScriptArray* vec2Array(u32 count)
        {
            return CScriptArray::Create(asGetActiveContext()->GetEngine()->GetTypeInfoByDecl("array<Vec2>"), count);
        }

        Curve* makeCurve() { return create<Curve>(MemTag::Script); }
        Curve* makeCurveFrom(const CScriptArray& points)
        {
            Curve* c = makeCurve();
            for (asUINT i = 0; c && i < points.GetSize(); i++)
            {
                if (!c->add(*static_cast<const Vec2*>(points.At(i))))
                {
                    scriptError("a curve has up to 32 control points");
                    break;
                }
            }
            return c;
        }
        void curveAdd(const Vec2& point, Curve* c)
        {
            if (!c->add(point))
            {
                scriptError("a curve has up to 32 control points");
            }
        }
        void curveInsert(u32 index, const Vec2& point, Curve* c)
        {
            if (!c->insert(index, point))
            {
                scriptError("a curve has up to 32 control points");
            }
        }
        CScriptArray* curvePoints(u32 count, const Curve* c)
        {
            count = count < 2 ? 2 : count > 4096 ? 4096 : count;
            CScriptArray* out = vec2Array(c->count() ? count : 0);
            for (u32 i = 0; out && i < out->GetSize(); i++)
            {
                *static_cast<Vec2*>(out->At(i)) = c->at(static_cast<float>(i) / static_cast<float>(count - 1));
            }
            return out;
        }
        Curve* curvePart(float from, float to, const Curve* c)
        {
            Curve* out = makeCurve();
            if (out)
            {
                c->part(from, to, *out);
            }
            return out;
        }

        CScriptArray* triangulateCorners(const CScriptArray& corners)
        {
            CScriptArray* out = CScriptArray::Create(asGetActiveContext()->GetEngine()->GetTypeInfoByDecl("array<uint>"));
            const u32 n = corners.GetSize();
            if (out == nullptr || n < 3)
            {
                return out;
            }
            if (n > 4096)
            {
                scriptError("up to 4096 corners can be cut into triangles");
                return out;
            }
            GrowArray<Vec2, MemTag::Script> points;
            GrowArray<u16, MemTag::Script> made;
            if (!points.reserve(n) || !made.reserve(3 * (n - 2)))
            {
                return out;
            }
            points.resize(n);
            copyArray(&corners, &points[0], n);
            const u32 triangles = triangulate(&points[0], n, &made[0]);
            out->Resize(triangles * 3);
            for (u32 i = 0; i < triangles * 3; i++)
            {
                *static_cast<u32*>(out->At(i)) = made[i];
            }
            return out;
        }

        bool convexCorners(const CScriptArray& corners)
        {
            const u32 n = corners.GetSize();
            GrowArray<Vec2, MemTag::Script> points;
            if (n < 3 || !points.reserve(n))
            {
                return false;
            }
            points.resize(n);
            copyArray(&corners, &points[0], n);
            return isConvex(&points[0], n);
        }
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

        b.refType("Curve", 0, "A Bezier curve on the plane: it starts at its first control point and ends at its last, and is pulled towards those between without passing through them, as drawing programs' curves are. Places on it are asked for by t, 0 at the start to 1 at the end. Up to 32 control points: 3 make it quadratic, 4 cubic.");
        b.behaviour(asBEHAVE_FACTORY, "Curve@ f()", NX_FN(makeCurve));
        b.behaviour(asBEHAVE_FACTORY, "Curve@ f(const array<Vec2> &in points)", NX_FN(makeCurveFrom));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Curve, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Curve, release));
        b.method("void add(const Vec2 &in point)", NX_OBJ_LAST(curveAdd), "Another control point at the end.");
        b.method("void insert(uint index, const Vec2 &in point)", NX_OBJ_LAST(curveInsert), "One before another, or at the end.");
        b.method("void remove(uint index)", NX_METHOD(Curve, remove), "Takes one out.");
        b.method("void clear()", NX_METHOD(Curve, clear), "No control points.");
        b.method("uint get_count() const property", NX_METHOD(Curve, count), "How many control points.");
        b.method("uint get_degree() const property", NX_METHOD(Curve, degree), "One less than that.");
        b.method("Vec2 point(uint index) const", NX_METHOD(Curve, point), "One of them.");
        b.method("void setPoint(uint index, const Vec2 &in point)", NX_METHOD(Curve, setPoint), "Moves one.");
        b.method("Vec2 at(float t) const", NX_METHOD(Curve, at), "The place on it at t.");
        b.method("Vec2 slope(float t) const", NX_METHOD(Curve, slope), "Which way it goes at t, and how fast as t goes up: its normalized() is the direction, for turning what follows it.");
        b.method("array<Vec2>@ points(uint count = 32) const", NX_OBJ_LAST(curvePoints), "So many places along it, from start to end, evenly in t: for Canvas.polyline, a road, a path to follow.");
        b.method("float length(uint pieces = 64) const", NX_METHOD(Curve, length), "How long it is, measured along so many straight pieces.");
        b.method("Curve@ part(float from, float to) const", NX_OBJ_LAST(curvePart), "The part between two values of t, as a curve of its own: to split one, or draw it bit by bit.");

        b.function("array<uint>@ triangulate(const array<Vec2> &in corners)", NX_FN(triangulateCorners), "Cuts a polygon, either way round and concave too, into triangles that fill it: three corner indices for each. For Canvas.mesh, or shapes for physics, which want convex pieces. As long as its sides don't cross.");
        b.function("bool isConvex(const array<Vec2> &in corners)", NX_FN(convexCorners), "Whether a polygon bulges out at every corner, so a line between any two places inside stays inside.");

        b.function("float noise(float x, float y = 0, float z = 0)", NX_FN(noise1), "Smooth noise, -1 to 1, with hills and dips about 1 apart: the same for the same place every time. Scale the place for bigger or smaller ones; move through z for noise that changes with time.");
        b.function("float noise(const Vec2 &in place)", NX_FN(noise2), "The same, on the plane.");
        b.function("float noise(const Vec3 &in place)", NX_FN(noise3), "The same, in space.");
        b.function("float fractalNoise(float x, float y, float z = 0, uint octaves = 4, float roughness = 0.5f)", NX_FN(fractal), "Layers of it, each twice as fine and roughness as strong as the last, still -1 to 1: land, clouds, fire.");
        b.function("void noiseSeed(uint seed)", NX_FN(noiseSeed), "Another pattern, the same every time for the same seed.");
    }
}
