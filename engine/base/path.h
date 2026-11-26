#pragma once

#include "base/grow_array.h"
#include "base/math.h"
#include "base/types.h"

// A smooth way through points: a Catmull-Rom curve, which passes through every one. Places on it are asked for by how
// far along it they are, so that something going along it at a steady speed does.
namespace nx
{
    class Path : NonCopyable
    {
    public:
        void addRef() { m_refs++; }
        void release();

        void add(const Vec3& point);
        void clear();
        void setPoint(u32 index, const Vec3& point);
        u32 count() const { return m_points.count(); }
        Vec3 point(u32 index) const { return index < m_points.count() ? m_points[index] : Vec3(); }
        // A loop goes back from the last point to the first.
        void setLoop(bool loop);
        bool loop() const { return m_loop; }

        float length();
        // At a distance along it, which wraps round on a loop and stops at the ends otherwise.
        Vec3 at(float distance);
        // Which way it goes there, a unit long.
        Vec3 direction(float distance);
        // How far along it the nearest place to this one is.
        float nearest(const Vec3& place);

    private:
        static constexpr u32 kSteps = 16;   // of each piece between two points, to measure it

        Vec3 curve(u32 piece, float t) const;
        u32 pieces() const;
        void measure();
        // Which piece, and how far into it, from a distance along the whole.
        void find(float distance, u32& piece, float& t);

        int m_refs = 1;
        GrowArray<Vec3, MemTag::Script> m_points;
        GrowArray<float, MemTag::Script> m_lengths;   // along the whole, at each step
        bool m_loop = false;
        bool m_measured = false;
    };
}
