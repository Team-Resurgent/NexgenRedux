#pragma once

#include "base/grow_array.h"
#include "base/math.h"
#include "base/types.h"

// A Bezier curve on the plane: it starts at its first control point and ends at its last, and is pulled towards
// those between without passing through them. Asked for by t, 0 at the start to 1 at the end.
namespace nx
{
    class Curve : NonCopyable
    {
    public:
        static constexpr u32 kMostPoints = 32;

        void addRef() { m_refs++; }
        void release();

        bool add(const Vec2& point);
        bool insert(u32 index, const Vec2& point);
        void remove(u32 index);
        void clear() { m_points.clear(); }
        void setPoint(u32 index, const Vec2& point);
        Vec2 point(u32 index) const { return index < m_points.count() ? m_points[index] : Vec2(); }
        u32 count() const { return m_points.count(); }
        // One less than its control points: 2 is quadratic, 3 cubic.
        u32 degree() const { return m_points.count() ? m_points.count() - 1 : 0; }

        Vec2 at(float t) const;
        // How fast and which way it goes at t.
        Vec2 slope(float t) const;
        // So many places, evenly in t, from start to end.
        u32 points(u32 count, Vec2* out) const;
        // Long, measured along so many straight pieces.
        float length(u32 pieces = 64) const;
        // The part between two values of t, as a curve of the same degree.
        void part(float from, float to, Curve& out) const;

    private:
        int m_refs = 1;
        GrowArray<Vec2, MemTag::Script> m_points;
    };
}
