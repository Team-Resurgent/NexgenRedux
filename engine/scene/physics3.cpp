#include "scene/space.h"
#include "scene/world.h"

#include <algorithm>
#include <math.h>
#include <string.h>

// What moves in space: rigid bodies, which push, turn and stack, and walkers, which slide along things.
namespace nx
{
    namespace
    {
        constexpr u32 kSubsteps = 2;
        constexpr u32 kIterations = 8;
        constexpr float kSlop = 0.005f;
        constexpr float kMargin = 0.01f;    // contacts are made a little before touching
        constexpr float kStep = 0.45f;      // a walker steps up this much
        constexpr float kSteepest = 0.65f;  // the cosine of the steepest slope that can be stood on
        constexpr float kStillSpeed = 0.12f;
        constexpr float kStillTime = 0.6f;

        float clampFloat(float v, float low, float high)
        {
            return v < low ? low : v > high ? high : v;
        }

        float axis(const Vec3& v, u32 i)
        {
            return i == 0 ? v.x : i == 1 ? v.y : v.z;
        }

        Vec3 absolute(const Vec3& v)
        {
            return { fabsf(v.x), fabsf(v.y), fabsf(v.z) };
        }

        Vec3 times(const Vec3& a, const Vec3& b)
        {
            return { a.x * b.x, a.y * b.y, a.z * b.z };
        }

        Vec3 alongSegment(const Vec3& a, const Vec3& b, const Vec3& p)
        {
            const Vec3 d = b - a;
            const float length = d.dot(d);
            return a + d * (length > 1e-9f ? clampFloat((p - a).dot(d) / length, 0, 1) : 0);
        }

        // Two directions across one.
        void across(const Vec3& n, Vec3& a, Vec3& b)
        {
            a = (fabsf(n.x) > 0.7f ? Vec3(0, 1, 0) : Vec3(1, 0, 0)).cross(n).normalized();
            b = n.cross(a);
        }
    }

    // Where a collider is, with what it is part of.
    void Space::place(const Collider3& c, Vec3& middle, Quat& turn, Vec3& half) const
    {
        u32 at = c.owner;
        Vec3 size(1, 1, 1);
        turn = Quat();
        while (at != kNone)
        {
            const World::Entity& e = m_world.m_entities[at];
            if (e.parts3[0] != kNone)
            {
                const Spatial& s = m_spatials[e.parts3[0]];
                turn = s.turn * turn;
                size = times(size, s.size);
            }
            at = e.parent;
        }
        size = absolute(size);
        half = times(c.half, size);
        if (c.shape == Shape3::Sphere)
        {
            half.x = c.half.x * fmaxf(size.x, fmaxf(size.y, size.z));
        }
        else if (c.shape == Shape3::Capsule)
        {
            half.x = c.half.x * fmaxf(size.x, size.z);
            half.y = fmaxf(half.y, half.x);
        }
        middle = m_spatials[m_world.m_entities[c.owner].parts3[0]].world.transformPoint(c.middle);
    }

    void Space::makeRigid(const Collider3& c, Rigid& r) const
    {
        refresh(c);
        r = {};
        r.entity = c.owner;
        r.body = kNone;
        r.shape = c.shape;
        r.place = c.at;
        r.turn = c.turn;
        r.half = c.halves;
        r.friction = 0.6f;
        r.axes[0] = c.axes[0];
        r.axes[1] = c.axes[1];
        r.axes[2] = c.axes[2];
        r.low = c.at - c.reach;
        r.high = c.at + c.reach;
    }

    // Works out where a collider is, if it isn't known already.
    void Space::refresh(const Collider3& collider) const
    {
        Collider3& c = const_cast<Collider3&>(collider);
        const u32 version = m_spatials[m_world.m_entities[c.owner].parts3[0]].version;
        if (c.version == version)
        {
            return;
        }
        Rigid r = {};
        r.shape = c.shape;
        place(c, r.place, r.turn, r.half);
        shapeRigid(r);
        c.version = version;
        c.at = r.place;
        c.turn = r.turn;
        c.halves = r.half;
        c.reach = r.high - r.place;
        c.axes[0] = r.axes[0];
        c.axes[1] = r.axes[1];
        c.axes[2] = r.axes[2];
    }

    void Space::shapeRigid(Rigid& r)
    {
        r.axes[0] = r.turn.rotate({ 1, 0, 0 });
        r.axes[1] = r.turn.rotate({ 0, 1, 0 });
        r.axes[2] = r.turn.rotate({ 0, 0, 1 });
        Vec3 reach;
        if (r.shape == Shape3::Sphere)
        {
            reach = Vec3(1, 1, 1) * r.half.x;
        }
        else if (r.shape == Shape3::Box)
        {
            reach = absolute(r.axes[0]) * r.half.x + absolute(r.axes[1]) * r.half.y + absolute(r.axes[2]) * r.half.z;
        }
        else
        {
            reach = absolute(r.axes[1]) * (r.half.y - r.half.x) + Vec3(1, 1, 1) * r.half.x;
        }
        reach = reach + Vec3(kMargin, kMargin, kMargin);
        r.low = r.place - reach;
        r.high = r.place + reach;
    }

    void Space::addRigid(const Collider3& c, u32 body)
    {
        Rigid r;
        makeRigid(c, r);
        if (body != kNone)
        {
            const Body3& b = m_bodies[body];
            r.body = body;
            r.velocity = b.velocity;
            r.spin = b.spin;
            r.friction = b.friction;
            r.bounce = b.bounce;
            if (b.walker)
            {
                r.inverseMass = 1 / (b.mass > 0 ? b.mass : 80);
                r.friction = 0;
            }
            else if (b.mass > 0)
            {
                r.inverseMass = 1 / b.mass;
                Vec3 square(r.half.x, r.half.x, r.half.x);
                if (r.shape == Shape3::Sphere)
                {
                    square = square * r.half.x;
                    r.inverseInertia = Vec3(1, 1, 1) * (1 / (0.4f * b.mass * square.x));
                }
                else
                {
                    square = r.shape == Shape3::Box ? times(r.half, r.half) : Vec3(r.half.x * r.half.x * 0.8f, r.half.y * r.half.y * 0.8f, r.half.x * r.half.x * 0.8f);
                    const float third = b.mass / 3;
                    r.inverseInertia = { 1 / (third * (square.y + square.z)), 1 / (third * (square.x + square.z)), 1 / (third * (square.x + square.y)) };
                }
            }
        }
        m_rigid.push(r);
    }

    Vec3 Space::inertia(const Rigid& r, const Vec3& v)
    {
        return r.axes[0] * (r.inverseInertia.x * r.axes[0].dot(v)) + r.axes[1] * (r.inverseInertia.y * r.axes[1].dot(v)) + r.axes[2] * (r.inverseInertia.z * r.axes[2].dot(v));
    }

    // --- Touching: normals point from the first to the second ---

    u32 Space::touchBall(const Vec3& at, float radius, const Rigid& box, Contact& contact)
    {
        const Vec3 d = at - box.place;
        const Vec3 local(d.dot(box.axes[0]), d.dot(box.axes[1]), d.dot(box.axes[2]));
        const Vec3 near(clampFloat(local.x, -box.half.x, box.half.x), clampFloat(local.y, -box.half.y, box.half.y), clampFloat(local.z, -box.half.z, box.half.z));
        const Vec3 off = local - near;
        const float far = off.dot(off);
        Vec3 out;
        float depth = 0;
        Vec3 surface = near;
        if (far > 1e-10f)
        {
            const float length = sqrtf(far);
            depth = radius - length;
            if (depth < -kMargin)
            {
                return 0;
            }
            out = off / length;
        }
        else
        {
            // Inside: out through the nearest face.
            float least = 1e9f;
            u32 face = 0;
            for (u32 i = 0; i < 3; i++)
            {
                const float gap = axis(box.half, i) - fabsf(axis(local, i));
                if (gap < least)
                {
                    least = gap;
                    face = i;
                }
            }
            const float sign = axis(local, face) < 0 ? -1.0f : 1.0f;
            out = face == 0 ? Vec3(sign, 0, 0) : face == 1 ? Vec3(0, sign, 0) : Vec3(0, 0, sign);
            surface = near + out * least;
            depth = radius + least;
        }
        contact = {};
        contact.place = box.place + box.axes[0] * surface.x + box.axes[1] * surface.y + box.axes[2] * surface.z;
        contact.normal = -(box.axes[0] * out.x + box.axes[1] * out.y + box.axes[2] * out.z);
        contact.depth = depth;
        // On the edge of a face that could be stood on.
        u32 up = 0;
        for (u32 i = 1; i < 3; i++)
        {
            up = fabsf(box.axes[i].y) > fabsf(box.axes[up].y) ? i : up;
        }
        const float over = axis(local, up) * (box.axes[up].y < 0 ? -1.0f : 1.0f) - axis(box.half, up);
        contact.edge = fabsf(box.axes[up].y) > kSteepest && over > 0 && far > over * over * 1.0001f;
        return 1;
    }

    u32 Space::touchBalls(const Vec3& a, float ra, const Vec3& b, float rb, Contact& contact)
    {
        const Vec3 d = b - a;
        const float far = d.length();
        const float depth = ra + rb - far;
        if (depth < -kMargin)
        {
            return 0;
        }
        contact = {};
        contact.normal = far > 1e-6f ? d / far : Vec3(0, 1, 0);
        contact.place = a + contact.normal * (ra - depth * 0.5f);
        contact.depth = depth;
        return 1;
    }

    u32 Space::touchPair(const Rigid& a, const Rigid& b, Contact (&contacts)[4])
    {
        if (a.shape == Shape3::Box && b.shape == Shape3::Box)
        {
            return touchBoxes(a, b, contacts);
        }
        if (a.shape == Shape3::Box || (a.shape == Shape3::Capsule && b.shape == Shape3::Sphere))
        {
            const u32 count = touchPair(b, a, contacts);
            for (u32 i = 0; i < count; i++)
            {
                contacts[i].normal = -contacts[i].normal;
            }
            return count;
        }
        if (a.shape == Shape3::Sphere)
        {
            if (b.shape == Shape3::Sphere)
            {
                return touchBalls(a.place, a.half.x, b.place, b.half.x, contacts[0]);
            }
            if (b.shape == Shape3::Box)
            {
                return touchBall(a.place, a.half.x, b, contacts[0]);
            }
            const Vec3 reach = b.axes[1] * (b.half.y - b.half.x);
            return touchBalls(a.place, a.half.x, alongSegment(b.place - reach, b.place + reach, a.place), b.half.x, contacts[0]);
        }
        // A capsule, with a box or another.
        const Vec3 reach = a.axes[1] * (a.half.y - a.half.x);
        const Vec3 ends[2] = { a.place - reach, a.place + reach };
        u32 count = 0;
        const auto add = [&](const Vec3& at)
        {
            Contact c;
            u32 found = 0;
            if (b.shape == Shape3::Box)
            {
                found = touchBall(at, a.half.x, b, c);
            }
            else
            {
                const Vec3 other = b.axes[1] * (b.half.y - b.half.x);
                found = touchBalls(at, a.half.x, alongSegment(b.place - other, b.place + other, at), b.half.x, c);
            }
            if (found && count < 4)
            {
                contacts[count++] = c;
            }
        };
        add(ends[0]);
        add(ends[1]);
        // Where between its ends it comes nearest.
        Vec3 at = a.place;
        for (u32 i = 0; i < 6; i++)
        {
            Vec3 near;
            if (b.shape == Shape3::Box)
            {
                const Vec3 d = at - b.place;
                near = b.place;
                for (u32 k = 0; k < 3; k++)
                {
                    near = near + b.axes[k] * clampFloat(d.dot(b.axes[k]), -axis(b.half, k), axis(b.half, k));
                }
            }
            else
            {
                const Vec3 other = b.axes[1] * (b.half.y - b.half.x);
                near = alongSegment(b.place - other, b.place + other, at);
            }
            at = alongSegment(ends[0], ends[1], near);
        }
        if ((at - ends[0]).length() > 0.1f * a.half.x && (at - ends[1]).length() > 0.1f * a.half.x)
        {
            add(at);
        }
        return count;
    }

    // Separating axes, then the face of one clipped by the sides of the other.
    u32 Space::touchBoxes(const Rigid& a, const Rigid& b, Contact (&contacts)[4])
    {
        const Vec3 d = b.place - a.place;
        float best = 1e9f;
        Vec3 normal;
        u32 kind = 0;
        const auto half = [](const Rigid& r, const Vec3& n) { return fabsf(r.axes[0].dot(n)) * r.half.x + fabsf(r.axes[1].dot(n)) * r.half.y + fabsf(r.axes[2].dot(n)) * r.half.z; };
        const auto test = [&](const Vec3& n, u32 which, float favour)
        {
            const float overlap = half(a, n) + half(b, n) - fabsf(d.dot(n));
            if (overlap < -kMargin)
            {
                return false;
            }
            if (overlap < best - favour)
            {
                best = overlap;
                normal = d.dot(n) < 0 ? -n : n;
                kind = which;
            }
            return true;
        };
        for (u32 i = 0; i < 3; i++)
        {
            if (!test(a.axes[i], i, 0))
            {
                return 0;
            }
        }
        for (u32 i = 0; i < 3; i++)
        {
            if (!test(b.axes[i], 3 + i, 0.002f))
            {
                return 0;
            }
        }
        for (u32 i = 0; i < 3; i++)
        {
            for (u32 k = 0; k < 3; k++)
            {
                const Vec3 cross = a.axes[i].cross(b.axes[k]);
                const float length = cross.length();
                // A face is steadier than an edge when there is little in it.
                if (length > 0.05f && !test(cross / length, 6 + i * 3 + k, 0.01f))
                {
                    return 0;
                }
            }
        }
        if (kind >= 6)
        {
            // Edge against edge: the nearest points of the two.
            const u32 i = (kind - 6) / 3, k = (kind - 6) % 3;
            Vec3 pa = a.place, pb = b.place;
            for (u32 n = 0; n < 3; n++)
            {
                if (n != i)
                {
                    pa = pa + a.axes[n] * (a.axes[n].dot(normal) > 0 ? axis(a.half, n) : -axis(a.half, n));
                }
                if (n != k)
                {
                    pb = pb + b.axes[n] * (b.axes[n].dot(normal) > 0 ? -axis(b.half, n) : axis(b.half, n));
                }
            }
            const Vec3 &u = a.axes[i], &v = b.axes[k], w = pa - pb;
            const float uv = u.dot(v), bottom = 1 - uv * uv;
            const float s = bottom > 1e-6f ? clampFloat((uv * v.dot(w) - u.dot(w)) / bottom, -axis(a.half, i), axis(a.half, i)) : 0;
            const float t = clampFloat(v.dot(w) + uv * s, -axis(b.half, k), axis(b.half, k));
            contacts[0] = {};
            contacts[0].place = (pa + u * s + pb + v * t) * 0.5f;
            contacts[0].normal = normal;
            contacts[0].depth = best;
            return 1;
        }
        // The face of one, and the face of the other that is most against it.
        const bool fromA = kind < 3;
        const Rigid& reference = fromA ? a : b;
        const Rigid& incident = fromA ? b : a;
        const u32 face = kind % 3;
        const Vec3 out = fromA ? normal : -normal;
        u32 other = 0;
        float most = 0;
        for (u32 i = 0; i < 3; i++)
        {
            const float n = fabsf(incident.axes[i].dot(out));
            if (n > most)
            {
                most = n;
                other = i;
            }
        }
        const float sign = incident.axes[other].dot(out) > 0 ? -1.0f : 1.0f;
        const u32 o1 = (other + 1) % 3, o2 = (other + 2) % 3;
        const Vec3 middle = incident.place + incident.axes[other] * (sign * axis(incident.half, other));
        const Vec3 e1 = incident.axes[o1] * axis(incident.half, o1), e2 = incident.axes[o2] * axis(incident.half, o2);
        Vec3 points[2][12];
        u32 count = 4, current = 0;
        points[0][0] = middle + e1 + e2;
        points[0][1] = middle - e1 + e2;
        points[0][2] = middle - e1 - e2;
        points[0][3] = middle + e1 - e2;
        for (u32 side = 0; side < 4; side++)
        {
            const u32 along = (face + 1 + side / 2) % 3;
            const Vec3 n = reference.axes[along] * (side % 2 ? -1.0f : 1.0f);
            const float limit = axis(reference.half, along);
            u32 kept = 0;
            for (u32 i = 0; i < count; i++)
            {
                const Vec3 &p = points[current][i], &q = points[current][(i + 1) % count];
                const float dp = (p - reference.place).dot(n) - limit, dq = (q - reference.place).dot(n) - limit;
                if (dp <= 0 && kept < 12)
                {
                    points[1 - current][kept++] = p;
                }
                if ((dp < 0) != (dq < 0) && dp != dq && kept < 12)
                {
                    points[1 - current][kept++] = p + (q - p) * (dp / (dp - dq));
                }
            }
            count = kept;
            current = 1 - current;
            if (count == 0)
            {
                return 0;
            }
        }
        Contact found[12];
        u32 total = 0;
        for (u32 i = 0; i < count; i++)
        {
            const Vec3& p = points[current][i];
            const float depth = axis(reference.half, face) - (p - reference.place).dot(out);
            if (depth > -kMargin)
            {
                found[total] = {};
                found[total].place = p + out * (depth * 0.5f);
                found[total].normal = normal;
                found[total++].depth = depth;
            }
        }
        // The deepest, and then those farthest from what has been taken.
        u32 taken = 0;
        while (taken < 4 && total)
        {
            u32 pick = 0;
            float score = -1;
            for (u32 i = 0; i < total; i++)
            {
                float s = found[i].depth + 1;
                if (taken)
                {
                    s = 1e9f;
                    for (u32 k = 0; k < taken; k++)
                    {
                        const Vec3 gap = found[i].place - contacts[k].place;
                        s = fminf(s, gap.dot(gap));
                    }
                }
                if (s > score)
                {
                    score = s;
                    pick = i;
                }
            }
            if (taken && score < 1e-5f)
            {
                break;
            }
            contacts[taken++] = found[pick];
            found[pick] = found[--total];
        }
        return taken;
    }

    u32 Space::touchLand(const Rigid& a, Contact (&contacts)[4]) const
    {
        u32 count = 0;
        const auto add = [&](const Vec3& at, float radius)
        {
            const float height = heightAt(at.x, at.z);
            if (height < -9e5f || at.y - radius * 1.5f > height + 1)
            {
                return;
            }
            const Vec3 n = normalAt(at.x, at.z);
            const float depth = radius - (at.y - height) * n.y;
            if (depth < -kMargin)
            {
                return;
            }
            Contact c = {};
            c.place = at - n * (radius - depth * 0.5f);
            c.normal = -n;
            c.depth = depth;
            if (count < 4)
            {
                contacts[count++] = c;
                return;
            }
            u32 least = 0;
            for (u32 i = 1; i < 4; i++)
            {
                least = contacts[i].depth < contacts[least].depth ? i : least;
            }
            if (contacts[least].depth < depth)
            {
                contacts[least] = c;
            }
        };
        if (a.low.y > (hasLand() ? 1e5f : m_ground + 0.1f))
        {
            return 0;
        }
        if (a.shape == Shape3::Sphere)
        {
            add(a.place, a.half.x);
        }
        else if (a.shape == Shape3::Capsule)
        {
            const Vec3 reach = a.axes[1] * (a.half.y - a.half.x);
            add(a.place - reach, a.half.x);
            add(a.place + reach, a.half.x);
        }
        else
        {
            for (u32 i = 0; i < 8; i++)
            {
                add(a.place + a.axes[0] * (i & 1 ? a.half.x : -a.half.x) + a.axes[1] * (i & 2 ? a.half.y : -a.half.y) + a.axes[2] * (i & 4 ? a.half.z : -a.half.z), 0);
            }
        }
        return count;
    }

    bool Space::overlap(const Collider3& a, const Collider3& b) const
    {
        Rigid ra, rb;
        makeRigid(a, ra);
        makeRigid(b, rb);
        Contact contacts[4];
        const u32 count = touchPair(ra, rb, contacts);
        // Walls and what they stop rest exactly against each other: that is touching too.
        return count > 0;
    }

    bool Space::touching(u32 a, u32 b) const
    {
        Space& self = const_cast<Space&>(*this);
        const Collider3* ca = self.getCollider(a);
        const Collider3* cb = self.getCollider(b);
        return ca && cb && overlap(*ca, *cb);
    }

    bool Space::touchesSphere(const Collider3& c, const Vec3& at, float radius) const
    {
        Rigid ball = {}, r;
        makeRigid(c, r);
        ball.shape = Shape3::Sphere;
        ball.place = at;
        ball.half.x = radius;
        Contact contacts[4];
        return touchPair(ball, r, contacts) > 0;
    }

    bool Space::visibleCollider(const Collider3& c) const
    {
        const World::Entity& e = m_world.m_entities[c.owner];
        return e.live && !e.dying && e.parts3[0] != kNone;
    }

    u32 Space::idOf(u32 index) const
    {
        return m_world.idOf(index);
    }

    void Space::collectPairs(GrowArray<u64>& pairs)
    {
        m_hitters.clear();
        for (u32 i = 0; i < m_colliders.count(); i++)
        {
            if (m_colliders[i].hits && visibleCollider(m_colliders[i]))
            {
                m_hitters.push(i);
            }
        }
        if (m_hitters.count() == 0)
        {
            return;
        }
        if (m_hitters.count() < 48)
        {
            // A few that want to know, among any number that don't.
            for (const u32 h : m_hitters)
            {
                const Collider3& ca = m_colliders[h];
                refresh(ca);
                const Vec3 low = ca.at - ca.reach, high = ca.at + ca.reach;
                for (u32 k = 0; k < m_colliders.count(); k++)
                {
                    const Collider3& cb = m_colliders[k];
                    // Two that both want to know are one pair.
                    if (k == h || !((ca.hits >> cb.group) & 1 || (cb.hits >> ca.group) & 1) || (cb.hits && k < h) || !visibleCollider(cb))
                    {
                        continue;
                    }
                    refresh(cb);
                    if (cb.at.x - cb.reach.x > high.x || cb.at.x + cb.reach.x < low.x || cb.at.y - cb.reach.y > high.y || cb.at.y + cb.reach.y < low.y || cb.at.z - cb.reach.z > high.z || cb.at.z + cb.reach.z < low.z || !overlap(ca, cb))
                    {
                        continue;
                    }
                    const u32 x = idOf(ca.owner), y = idOf(cb.owner);
                    pairs.push((static_cast<u64>(x < y ? x : y) << 32) | (x < y ? y : x));
                }
            }
            return;
        }
        m_boxes.clear();
        m_rigidOrder.clear();
        for (const Collider3& c : m_colliders)
        {
            Rigid r;
            makeRigid(c, r);
            m_rigidOrder.push(m_boxes.count());
            m_boxes.push({ r.low, r.high });
        }
        std::sort(m_rigidOrder.begin(), m_rigidOrder.end(), [this](u32 a, u32 b) { return m_boxes[a].low.x < m_boxes[b].low.x; });
        for (u32 i = 0; i < m_rigidOrder.count(); i++)
        {
            const Collider3& ca = m_colliders[m_rigidOrder[i]];
            const Box3& a = m_boxes[m_rigidOrder[i]];
            for (u32 k = i + 1; k < m_rigidOrder.count() && m_boxes[m_rigidOrder[k]].low.x <= a.high.x; k++)
            {
                const Collider3& cb = m_colliders[m_rigidOrder[k]];
                const Box3& b = m_boxes[m_rigidOrder[k]];
                if (b.low.y > a.high.y || b.high.y < a.low.y || b.low.z > a.high.z || b.high.z < a.low.z || !((ca.hits >> cb.group) & 1 || (cb.hits >> ca.group) & 1))
                {
                    continue;
                }
                if (visibleCollider(ca) && visibleCollider(cb) && overlap(ca, cb))
                {
                    const u32 x = idOf(ca.owner), y = idOf(cb.owner);
                    pairs.push((static_cast<u64>(x < y ? x : y) << 32) | (x < y ? y : x));
                }
            }
        }
    }

    // --- Walkers ---

    // Moves it out of whatever it is in. Returns what it met: 1 ground, 2 a wall, 4 a ceiling.
    u32 Space::pushOut(Rigid& r, Body3* b)
    {
        u32 met = 0;
        for (u32 round = 0; round < 4; round++)
        {
            float deepest = 0.0005f;
            Vec3 out;
            u32 against = kNone;
            bool edge = false;
            shapeRigid(r);
            const auto consider = [&](const Contact& c, u32 other)
            {
                if (c.depth > deepest)
                {
                    deepest = c.depth;
                    out = -c.normal;
                    against = other;
                    edge = c.edge && c.place.y < r.place.y - r.half.y + kStep;
                }
            };
            Contact contacts[4];
            for (u32 i = 0, count = touchLand(r, contacts); i < count; i++)
            {
                consider(contacts[i], kLand);
            }
            for (u32 i = 0; i < m_rigid.count(); i++)
            {
                const Rigid& o = m_rigid[i];
                if (o.entity == r.entity || o.low.x > r.high.x || o.high.x < r.low.x || o.low.y > r.high.y || o.high.y < r.low.y || o.low.z > r.high.z || o.high.z < r.low.z)
                {
                    continue;
                }
                for (u32 k = 0, count = touchPair(r, o, contacts); k < count; k++)
                {
                    consider(contacts[k], i);
                }
            }
            if (against == kNone)
            {
                break;
            }
            if (out.y > kSteepest || (edge && out.y > 0.12f))
            {
                // Straight up, so that standing on a slope isn't sliding down it.
                met |= 1;
                r.place.y += deepest / out.y;
                if (b)
                {
                    b->ground = out.y > kSteepest ? out : Vec3(0, 1, 0);
                    b->velocity.y = fmaxf(b->velocity.y, 0);
                }
                continue;
            }
            met |= out.y < -0.5f ? 4u : 2u;
            Vec3 way = out;
            if (out.y >= -0.5f && out.y < kSteepest)
            {
                // Too steep to climb: along the level, or it would be carried up.
                const Vec3 level(out.x, 0, out.z);
                const float length = level.length();
                way = length > 0.2f ? level / length : out;
                deepest = length > 0.2f ? fminf(deepest / length, deepest * 2) : deepest;
            }
            r.place = r.place + way * deepest;
            if (b)
            {
                const float into = b->velocity.dot(way);
                if (into < 0)
                {
                    b->velocity = b->velocity - way * into;
                }
            }
        }
        return met;
    }

    void Space::walkOne(u32 index, float dt)
    {
        Body3& b = m_bodies[index];
        const World::Entity& e = m_world.m_entities[b.owner];
        if (e.dying || e.parts3[0] == kNone || e.parts3[3] == kNone || e.parent != kNone)
        {
            return;
        }
        Rigid r;
        makeRigid(m_colliders[e.parts3[3]], r);
        // Always upright, whichever way it faces.
        r.turn = Quat();
        shapeRigid(r);
        const Vec3 offset = r.place - m_spatials[e.parts3[0]].place;
        const bool stood = b.onGround && b.velocity.y < 0.5f;
        b.velocity = b.velocity + m_gravity * (b.gravity * dt);
        if (b.drag > 0)
        {
            b.velocity = b.velocity * fmaxf(0, 1 - b.drag * dt);
        }
        const Vec3 wanted = b.velocity;
        const float far = wanted.length() * dt;
        const u32 steps = far > 0 ? static_cast<u32>(clampFloat(far / (r.half.x * 0.4f), 0, 15)) + 1 : 1;
        u32 met = 0;
        for (u32 s = 0; s < steps; s++)
        {
            r.place = r.place + b.velocity * (dt / static_cast<float>(steps));
            met |= pushOut(r, &b);
        }
        if (stood && !(met & 1) && b.velocity.y <= 0.5f)
        {
            // Down a slope or a step, rather than into the air.
            Rigid lower = r;
            for (float down = 0; down < kStep; down += 0.05f)
            {
                lower.place.y -= 0.05f;
                if (pushOut(lower, nullptr) & 1)
                {
                    r = lower;
                    met |= 1;
                    b.velocity.y = 0;
                    break;
                }
            }
        }
        b.onGround = met & 1;
        if (!b.onGround)
        {
            b.ground = { 0, 1, 0 };
        }
        m_spatials[e.parts3[0]].place = r.place - offset;
        // What it walks into feels how it wanted to move.
        for (Rigid& other : m_rigid)
        {
            if (other.entity == b.owner)
            {
                other.place = r.place;
                other.turn = r.turn;
                other.velocity = { wanted.x, b.velocity.y, wanted.z };
                shapeRigid(other);
            }
        }
    }

    void Space::walk(float dt)
    {
        m_rigid.clear();
        bool moves = false;
        for (const Body3& b : m_bodies)
        {
            moves = moves || b.walker || b.mass > 0;
        }
        if (moves)
        {
            for (const Collider3& c : m_colliders)
            {
                if (!visibleCollider(c))
                {
                    continue;
                }
                const u32 body = m_world.m_entities[c.owner].parts3[4];
                if (body != kNone && (m_bodies[body].mass > 0 || m_bodies[body].walker))
                {
                    addRigid(c, body);
                }
            }
            const u32 movers = m_rigid.count();
            for (const Collider3& c : m_colliders)
            {
                const u32 body = m_world.m_entities[c.owner].parts3[4];
                if (!c.wall || !visibleCollider(c) || (body != kNone && (m_bodies[body].mass > 0 || m_bodies[body].walker)))
                {
                    continue;
                }
                refresh(c);
                for (u32 i = 0; i < movers; i++)
                {
                    const Rigid& r = m_rigid[i];
                    const float reach = kStep + 0.5f + (fabsf(r.velocity.x) + fabsf(r.velocity.y) + fabsf(r.velocity.z)) * dt * 2;
                    if (fabsf(r.place.x - c.at.x) < r.high.x - r.place.x + c.reach.x + reach && fabsf(r.place.z - c.at.z) < r.high.z - r.place.z + c.reach.z + reach && fabsf(r.place.y - c.at.y) < r.high.y - r.place.y + c.reach.y + reach)
                    {
                        addRigid(c, body);
                        break;
                    }
                }
            }
        }
        for (u32 i = 0; i < m_bodies.count(); i++)
        {
            Body3& b = m_bodies[i];
            const World::Entity& e = m_world.m_entities[b.owner];
            if (b.walker)
            {
                walkOne(i, dt);
            }
            else if ((b.mass <= 0 || e.parts3[3] == kNone) && e.parts3[0] != kNone)
            {
                // Nothing stops it.
                Spatial& s = m_spatials[e.parts3[0]];
                b.velocity = (b.velocity + m_gravity * (b.gravity * dt)) * fmaxf(0, 1 - b.drag * dt);
                s.place = s.place + b.velocity * dt;
                const float spin = b.spin.length();
                if (spin > 1e-5f)
                {
                    s.turn = (Quat::axisAngle(b.spin / spin, spin * dt) * s.turn).normalized();
                }
            }
        }
    }

    // --- Rigid bodies ---

    void Space::physics(float dt)
    {
        m_stats.contacts = 0;
        bool any = false;
        for (Rigid& r : m_rigid)
        {
            any = any || (r.body != kNone && m_bodies[r.body].mass > 0 && !m_bodies[r.body].walker);
        }
        if (!any)
        {
            m_touches.clear();
            return;
        }
        for (u32 i = 0; i < kSubsteps; i++)
        {
            solve(dt / static_cast<float>(kSubsteps));
        }
        for (const Rigid& r : m_rigid)
        {
            if (r.body == kNone || m_bodies[r.body].walker || m_bodies[r.body].mass <= 0)
            {
                continue;
            }
            Body3& b = m_bodies[r.body];
            b.velocity = r.velocity;
            b.spin = r.spin;
            b.onGround = r.onGround;
            const bool still = r.velocity.dot(r.velocity) < kStillSpeed * kStillSpeed && r.spin.dot(r.spin) < kStillSpeed * kStillSpeed * 9;
            b.still = still && r.onGround ? b.still + dt : 0;
            const World::Entity& e = m_world.m_entities[r.entity];
            if (e.parent == kNone)
            {
                Spatial& s = m_spatials[e.parts3[0]];
                s.turn = r.turn;
                s.place = r.place - r.turn.rotate(times(m_colliders[e.parts3[3]].middle, s.size));
            }
        }
        m_stats.contacts = 0;
        for (const Touch& t : m_touches)
        {
            m_stats.contacts += t.count;
        }
    }

    void Space::solve(float dt)
    {
        const auto moves = [this](const Rigid& r) { return r.body != kNone && m_bodies[r.body].mass > 0 && !m_bodies[r.body].walker; };
        for (Rigid& r : m_rigid)
        {
            r.onGround = false;
            if (!moves(r))
            {
                continue;
            }
            const Body3& b = m_bodies[r.body];
            r.still = b.still > kStillTime;
            if (!r.still)
            {
                r.velocity = (r.velocity + m_gravity * (b.gravity * dt)) * fmaxf(0, 1 - b.drag * dt);
                r.spin = r.spin * fmaxf(0, 1 - (0.4f + b.drag) * dt);
            }
            shapeRigid(r);
        }

        // Who touches whom.
        m_touchesBefore.swap(m_touches);
        m_touches.clear();
        m_rigidOrder.clear();
        for (u32 i = 0; i < m_rigid.count(); i++)
        {
            m_rigidOrder.push(i);
        }
        std::sort(m_rigidOrder.begin(), m_rigidOrder.end(), [this](u32 a, u32 b) { return m_rigid[a].low.x < m_rigid[b].low.x; });
        const auto add = [&](u32 a, u32 b, u32 count, const Contact (&contacts)[4])
        {
            if (count == 0)
            {
                return;
            }
            Touch t = {};
            t.key = (static_cast<u64>(m_rigid[a].entity) << 32) | (b == kLand ? kLand : m_rigid[b].entity);
            t.a = a;
            t.b = b;
            t.count = count;
            t.friction = b == kLand ? m_rigid[a].friction : sqrtf(m_rigid[a].friction * m_rigid[b].friction);
            t.bounce = b == kLand ? m_rigid[a].bounce : fmaxf(m_rigid[a].bounce, m_rigid[b].bounce);
            for (u32 i = 0; i < count; i++)
            {
                t.contacts[i] = contacts[i];
            }
            // The same contacts as last time push as hard to begin with.
            u32 low = 0, high = m_touchesBefore.count();
            while (low < high)
            {
                const u32 middle = (low + high) / 2;
                (m_touchesBefore[middle].key < t.key ? low : high) = m_touchesBefore[middle].key < t.key ? middle + 1 : middle;
            }
            if (low < m_touchesBefore.count() && m_touchesBefore[low].key == t.key)
            {
                const Touch& before = m_touchesBefore[low];
                for (u32 i = 0; i < count; i++)
                {
                    float nearest = 0.04f * 0.04f;
                    for (u32 k = 0; k < before.count; k++)
                    {
                        const Vec3 d = before.contacts[k].place - t.contacts[i].place;
                        if (d.dot(d) < nearest)
                        {
                            nearest = d.dot(d);
                            t.contacts[i].normalImpulse = before.contacts[k].normalImpulse;
                            t.contacts[i].tangentImpulse[0] = before.contacts[k].tangentImpulse[0];
                            t.contacts[i].tangentImpulse[1] = before.contacts[k].tangentImpulse[1];
                        }
                    }
                }
            }
            m_touches.push(t);
        };
        Contact contacts[4];
        for (u32 i = 0; i < m_rigidOrder.count(); i++)
        {
            const u32 ia = m_rigidOrder[i];
            Rigid& a = m_rigid[ia];
            if (moves(a))
            {
                add(ia, kLand, touchLand(a, contacts), contacts);
            }
            for (u32 k = i + 1; k < m_rigidOrder.count() && m_rigid[m_rigidOrder[k]].low.x <= a.high.x; k++)
            {
                const u32 ib = m_rigidOrder[k];
                Rigid& b = m_rigid[ib];
                if (b.low.y > a.high.y || b.high.y < a.low.y || b.low.z > a.high.z || b.high.z < a.low.z || (!moves(a) && !moves(b)))
                {
                    continue;
                }
                const u32 count = touchPair(a, b, contacts);
                // In the order of who they are, so the keys match next time.
                if (a.entity < b.entity)
                {
                    add(ia, ib, count, contacts);
                }
                else
                {
                    for (u32 n = 0; n < count; n++)
                    {
                        contacts[n].normal = -contacts[n].normal;
                    }
                    add(ib, ia, count, contacts);
                }
            }
        }
        std::sort(m_touches.begin(), m_touches.end(), [](const Touch& a, const Touch& b) { return a.key < b.key; });

        // What is still is woken by what moves, or by losing what it rested on.
        for (Rigid& r : m_rigid)
        {
            r.touching = 0;
        }
        for (const Touch& t : m_touches)
        {
            m_rigid[t.a].touching++;
            if (t.b != kLand)
            {
                m_rigid[t.b].touching++;
            }
        }
        for (Rigid& r : m_rigid)
        {
            if (moves(r))
            {
                Body3& b = m_bodies[r.body];
                if (r.still && b.touched != r.touching)
                {
                    r.still = false;
                    b.still = 0;
                }
                b.touched = r.touching;
            }
        }
        for (u32 round = 0; round < 2; round++)
        {
            for (const Touch& t : m_touches)
            {
                if (t.b == kLand)
                {
                    continue;
                }
                Rigid &a = m_rigid[t.a], &b = m_rigid[t.b];
                const auto wakes = [&](const Rigid& r) { return !r.still && (r.velocity.dot(r.velocity) > 0.25f || r.spin.dot(r.spin) > 1); };
                if (a.still && wakes(b))
                {
                    a.still = false;
                    m_bodies[a.body].still = 0;
                }
                if (b.still && wakes(a))
                {
                    b.still = false;
                    m_bodies[b.body].still = 0;
                }
            }
        }

        // How hard it is to move them at each contact.
        Rigid ground = {};
        const auto mass = [](const Rigid& r) { return r.still ? 0.0f : r.inverseMass; };
        const auto turns = [](const Rigid& r, const Vec3& v) { return r.still ? Vec3() : inertia(r, v); };
        const auto speedAt = [](const Rigid& r, const Vec3& from) { return r.velocity + r.spin.cross(from); };
        const auto push = [&](Rigid& a, Rigid& b, const Contact& c, const Vec3& impulse)
        {
            a.velocity = a.velocity - impulse * mass(a);
            a.spin = a.spin - turns(a, c.fromA.cross(impulse));
            b.velocity = b.velocity + impulse * mass(b);
            b.spin = b.spin + turns(b, c.fromB.cross(impulse));
        };
        for (Touch& t : m_touches)
        {
            Rigid &a = m_rigid[t.a], &b = t.b == kLand ? ground : m_rigid[t.b];
            for (u32 i = 0; i < t.count; i++)
            {
                Contact& c = t.contacts[i];
                c.fromA = c.place - a.place;
                c.fromB = c.place - b.place;
                across(c.normal, c.tangents[0], c.tangents[1]);
                const auto resist = [&](const Vec3& n)
                {
                    const float k = mass(a) + mass(b) + turns(a, c.fromA.cross(n)).cross(c.fromA).dot(n) + turns(b, c.fromB.cross(n)).cross(c.fromB).dot(n);
                    return k > 0 ? 1 / k : 0.0f;
                };
                c.normalMass = resist(c.normal);
                c.tangentMass[0] = resist(c.tangents[0]);
                c.tangentMass[1] = resist(c.tangents[1]);
                const float speed = (speedAt(b, c.fromB) - speedAt(a, c.fromA)).dot(c.normal);
                c.bias = c.depth > kSlop ? fminf(0.2f / dt * (c.depth - kSlop), 3) : c.depth < 0 ? c.depth / dt : 0;
                if (speed < -1)
                {
                    c.bias = fmaxf(c.bias, -speed * t.bounce);
                }
                push(a, b, c, c.normal * c.normalImpulse + c.tangents[0] * c.tangentImpulse[0] + c.tangents[1] * c.tangentImpulse[1]);
                // Something under it.
                if (c.normal.y < -0.5f)
                {
                    a.onGround = true;
                }
                else if (c.normal.y > 0.5f)
                {
                    b.onGround = true;
                }
            }
        }
        for (u32 iteration = 0; iteration < kIterations; iteration++)
        {
            for (Touch& t : m_touches)
            {
                Rigid &a = m_rigid[t.a], &b = t.b == kLand ? ground : m_rigid[t.b];
                for (u32 i = 0; i < t.count; i++)
                {
                    Contact& c = t.contacts[i];
                    for (u32 k = 0; k < 2; k++)
                    {
                        const float speed = (speedAt(b, c.fromB) - speedAt(a, c.fromA)).dot(c.tangents[k]);
                        const float limit = t.friction * c.normalImpulse;
                        const float before = c.tangentImpulse[k];
                        c.tangentImpulse[k] = clampFloat(before - speed * c.tangentMass[k], -limit, limit);
                        push(a, b, c, c.tangents[k] * (c.tangentImpulse[k] - before));
                    }
                    const float speed = (speedAt(b, c.fromB) - speedAt(a, c.fromA)).dot(c.normal);
                    const float before = c.normalImpulse;
                    c.normalImpulse = fmaxf(0, before + (c.bias - speed) * c.normalMass);
                    push(a, b, c, c.normal * (c.normalImpulse - before));
                }
            }
        }
        for (Rigid& r : m_rigid)
        {
            if (!moves(r) || r.still)
            {
                if (r.still)
                {
                    r.velocity = r.spin = {};
                    r.onGround = true;
                }
                continue;
            }
            const float spin = r.spin.length();
            if (spin > 40)
            {
                r.spin = r.spin * (40 / spin);
            }
            r.place = r.place + r.velocity * dt;
            const Quat w(r.spin.x * 0.5f * dt, r.spin.y * 0.5f * dt, r.spin.z * 0.5f * dt, 0);
            const Quat d = w * r.turn;
            r.turn = Quat(r.turn.x + d.x, r.turn.y + d.y, r.turn.z + d.z, r.turn.w + d.w).normalized();
        }
    }

    // --- Along a line ---

    bool Space::raycast(const Vec3& from, const Vec3& to, u32 groups, u32 except, u32& id, Vec3& hit, Vec3& normal) const
    {
        const Vec3 way = to - from;
        float best = 2;
        id = 0;
        const auto ball = [&](const Vec3& at, float radius, float& t)
        {
            const Vec3 d = from - at;
            const float a = way.dot(way), b = d.dot(way), k = d.dot(d) - radius * radius;
            const float root = b * b - a * k;
            if (k <= 0)
            {
                t = 0;
                return true;
            }
            if (a <= 0 || root < 0)
            {
                return false;
            }
            t = (-b - sqrtf(root)) / a;
            return t >= 0;
        };
        for (const Collider3& c : m_colliders)
        {
            if (!(groups & (1u << c.group)) || !visibleCollider(c) || idOf(c.owner) == except)
            {
                continue;
            }
            Rigid r;
            makeRigid(c, r);
            float t = 2;
            Vec3 n(0, 1, 0);
            if (r.shape == Shape3::Sphere)
            {
                if (ball(r.place, r.half.x, t))
                {
                    n = (from + way * t - r.place).normalized();
                }
            }
            else if (r.shape == Shape3::Capsule)
            {
                const Vec3 reach = r.axes[1] * (r.half.y - r.half.x);
                float s = 2;
                for (const Vec3& end : { r.place - reach, r.place + reach })
                {
                    if (ball(end, r.half.x, s) && s < t)
                    {
                        t = s;
                        n = (from + way * t - end).normalized();
                    }
                }
                // Its side.
                const Vec3 d = from - r.place;
                const Vec3 flat = way - r.axes[1] * way.dot(r.axes[1]), off = d - r.axes[1] * d.dot(r.axes[1]);
                const float a = flat.dot(flat), b = off.dot(flat), k = off.dot(off) - r.half.x * r.half.x, root = b * b - a * k;
                if (a > 1e-9f && root >= 0)
                {
                    s = (-b - sqrtf(root)) / a;
                    const Vec3 p = d + way * s;
                    if (s >= 0 && s < t && fabsf(p.dot(r.axes[1])) <= r.half.y - r.half.x)
                    {
                        t = s;
                        n = (p - r.axes[1] * p.dot(r.axes[1])).normalized();
                    }
                }
            }
            else
            {
                const Vec3 d = from - r.place;
                float low = 0, high = 1;
                for (u32 i = 0; i < 3; i++)
                {
                    const float start = d.dot(r.axes[i]), speed = way.dot(r.axes[i]), half = axis(r.half, i);
                    if (fabsf(speed) < 1e-7f)
                    {
                        low = fabsf(start) > half ? 3 : low;
                        continue;
                    }
                    const float enter = (speed > 0 ? -half - start : half - start) / speed, leave = (speed > 0 ? half - start : -half - start) / speed;
                    if (enter > low)
                    {
                        low = enter;
                        n = r.axes[i] * (speed > 0 ? -1.0f : 1.0f);
                    }
                    high = fminf(high, leave);
                }
                t = low <= high ? low : 2;
            }
            if (t <= 1 && t < best)
            {
                best = t;
                id = idOf(c.owner);
                normal = n;
            }
        }
        // The land, a little at a time.
        if ((hasLand() || m_hasGround) && (groups & 1))
        {
            const float length = way.length(), step = hasLand() ? m_landSpacing * 0.5f : 1;
            const u32 count = static_cast<u32>(clampFloat(length / step, 0, 2000)) + 1;
            for (u32 i = 1; i <= count; i++)
            {
                float t = static_cast<float>(i) / static_cast<float>(count);
                Vec3 p = from + way * t;
                if (static_cast<float>(i - 1) / static_cast<float>(count) > best)
                {
                    break;
                }
                if (p.y < heightAt(p.x, p.z))
                {
                    float low = static_cast<float>(i - 1) / static_cast<float>(count), high = t;
                    for (u32 k = 0; k < 12; k++)
                    {
                        t = (low + high) * 0.5f;
                        p = from + way * t;
                        (p.y < heightAt(p.x, p.z) ? high : low) = t;
                    }
                    if (t < best)
                    {
                        best = t;
                        id = 0;
                        normal = normalAt(p.x, p.z);
                    }
                    break;
                }
            }
        }
        hit = from + way * fminf(best, 1);
        return best <= 1;
    }

    void Space::push(u32 id, const Vec3& impulse, const Vec3* at)
    {
        Body3* b = getBody(id);
        if (b == nullptr)
        {
            return;
        }
        b->still = 0;
        b->velocity = b->velocity + impulse * (b->mass > 0 ? 1 / b->mass : 1);
        const Collider3* c = getCollider(id);
        if (at && c && b->mass > 0)
        {
            Rigid r;
            makeRigid(*c, r);
            const float reach = r.shape == Shape3::Box ? (r.half.x + r.half.y + r.half.z) / 3 : r.half.x;
            b->spin = b->spin + (*at - r.place).cross(impulse) * (1 / (0.5f * b->mass * reach * reach));
        }
    }
}
