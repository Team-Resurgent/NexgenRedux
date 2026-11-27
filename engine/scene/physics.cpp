#include "scene/world.h"

#include <algorithm>
#include <math.h>

// Rigid bodies in two dimensions: circles and boxes that push, stack and slide, and joints that hold
// them together. Contacts are solved one after another, several times over, starting from the answer
// of the step before, which is what lets stacks stand still.
namespace nx
{
    namespace
    {
        constexpr float kDegrees = 3.14159265f / 180.0f;
        constexpr u32 kIterations = 10;
        constexpr float kSlop = 0.5f;          // pixels that bodies may sink in, so contacts last
        constexpr float kCorrection = 0.2f;    // part of the rest put right each step
        constexpr float kBounceAbove = 60;     // pixels a second; slower contacts don't bounce

        float cross(Vec2 a, Vec2 b)
        {
            return a.x * b.y - a.y * b.x;
        }

        Vec2 cross(float s, Vec2 a)
        {
            return { -s * a.y, s * a.x };
        }

        float clampFloat(float v, float low, float high)
        {
            return v < low ? low : v > high ? high : v;
        }

        // A corner of a box, with the edges that met there.
        struct Corner
        {
            Vec2 at;
            u32 feature;
        };

        // Keeps the part of an edge on the inside of a line.
        u32 clip(Corner (&out)[2], const Corner (&in)[2], Vec2 normal, float offset, u32 side)
        {
            u32 count = 0;
            const float d0 = normal.dot(in[0].at) - offset, d1 = normal.dot(in[1].at) - offset;
            if (d0 <= 0)
            {
                out[count++] = in[0];
            }
            if (d1 <= 0)
            {
                out[count++] = in[1];
            }
            if (d0 * d1 < 0 && count < 2)
            {
                const float part = d0 / (d0 - d1);
                out[count].at = in[0].at + (in[1].at - in[0].at) * part;
                out[count].feature = (in[d0 > 0 ? 0 : 1].feature & 0xFF) | (side << 8);
                count++;
            }
            return count;
        }
    }

    void World::addRigid(const Collider& c, u32 body)
    {
        const Entity& e = m_entities[c.owner];
        Rigid r = {};
        r.id = idOf(c.owner);
        r.body = body;
        r.entity = c.owner;
        r.shape = c.shape;
        const float* m = e.world;
        const float sx = sqrtf(m[0] * m[0] + m[1] * m[1]), sy = sqrtf(m[2] * m[2] + m[3] * m[3]);
        r.position = { m[4], m[5] };
        r.angle = body == kNone ? atan2f(m[1], m[0]) : e.rotation * kDegrees;
        r.halfWidth = c.halfWidth * sx;
        r.halfHeight = c.halfHeight * sy;
        r.friction = 0.6f;
        r.oneWay = c.oneWay && c.wall;
        // A polygon's corners, scaled with it, around its place.
        Vec2 local[kPolygonPoints];
        u32 corners = 0;
        if (c.shape == Shape::Polygon && c.points)
        {
            corners = c.count < kPolygonPoints ? c.count : kPolygonPoints;
            for (u32 i = 0; i < corners; i++)
            {
                local[i] = { c.points[i].x * sx, c.points[i].y * sy };
            }
        }
        float inertiaPart = c.shape == Shape::Circle ? r.halfWidth * r.halfWidth * 0.5f : (r.halfWidth * r.halfWidth + r.halfHeight * r.halfHeight) / 3;
        if (corners >= 3)
        {
            float area = 0, sum = 0;
            for (u32 i = 0; i < corners; i++)
            {
                const Vec2 p = local[i], q = local[(i + 1) % corners];
                const float a = cross(p, q);
                area += a;
                sum += a * (p.dot(p) + p.dot(q) + q.dot(q));
            }
            inertiaPart = area != 0 ? sum / (6 * area) : inertiaPart;
        }
        if (body != kNone)
        {
            const Body& b = m_bodies[body];
            r.position = e.position;
            r.velocity = b.velocity;
            r.turning = b.spin * kDegrees;
            r.friction = b.friction;
            r.bounce = b.bounce;
            r.inverseMass = 1 / b.mass;
            const float inertia = b.mass * inertiaPart;
            r.inverseInertia = inertia > 0 ? 1 / inertia : 0;
        }
        r.c = cosf(r.angle);
        r.s = sinf(r.angle);
        if (c.shape == Shape::Box)
        {
            const Vec2 box[4] = { { -r.halfWidth, -r.halfHeight }, { r.halfWidth, -r.halfHeight }, { r.halfWidth, r.halfHeight }, { -r.halfWidth, r.halfHeight } };
            for (u32 i = 0; i < 4; i++)
            {
                local[i] = box[i];
            }
            corners = 4;
        }
        if (corners >= 3)
        {
            r.count = static_cast<u8>(corners);
            for (u32 i = 0; i < corners; i++)
            {
                r.points[i] = r.position + Vec2(local[i].x * r.c - local[i].y * r.s, local[i].x * r.s + local[i].y * r.c);
            }
            for (u32 i = 0; i < corners; i++)
            {
                const Vec2 edge = r.points[(i + 1) % corners] - r.points[i];
                const float length = sqrtf(edge.dot(edge));
                r.normals[i] = length > 0 ? Vec2(edge.y / length, -edge.x / length) : Vec2(0, -1);
            }
        }
        m_rigid.push(r);
    }

    namespace
    {
        // The lowest of a polygon's corners along a direction, from a place.
        float deepest(const Vec2* points, u32 count, Vec2 normal, Vec2 from)
        {
            float least = 1e30f;
            for (u32 i = 0; i < count; i++)
            {
                least = fminf(least, normal.dot(points[i] - from));
            }
            return least;
        }

        // How far apart two convex shapes are along a's faces, and which face is furthest.
        float apartAlong(const Vec2* ap, const Vec2* an, u32 ac, const Vec2* bp, u32 bc, u32& face)
        {
            float best = -1e30f;
            face = 0;
            for (u32 i = 0; i < ac; i++)
            {
                const float s = deepest(bp, bc, an[i], ap[i]);
                if (s > best)
                {
                    best = s;
                    face = i;
                }
            }
            return best;
        }
    }

    // Convex against convex: the face that separates them least is the reference, and the other's edge that faces it most
    // is cut to its sides, as with boxes.
    u32 World::touchPolygons(const Rigid& a, const Rigid& b, Contact (&contacts)[2])
    {
        u32 faceA = 0, faceB = 0;
        const float sa = apartAlong(a.points, a.normals, a.count, b.points, b.count, faceA);
        if (sa > 0)
        {
            return 0;
        }
        const float sb = apartAlong(b.points, b.normals, b.count, a.points, a.count, faceB);
        if (sb > 0)
        {
            return 0;
        }
        const bool flip = sb > sa * 0.98f + 0.001f;
        const Rigid& r = flip ? b : a;
        const Rigid& o = flip ? a : b;
        const u32 face = flip ? faceB : faceA;
        const Vec2 normal = r.normals[face];
        // The other's edge most against it.
        u32 incident = 0;
        float most = 1e30f;
        for (u32 i = 0; i < o.count; i++)
        {
            const float d = o.normals[i].dot(normal);
            if (d < most)
            {
                most = d;
                incident = i;
            }
        }
        Corner edge[2] = { { o.points[incident], incident * 2 }, { o.points[(incident + 1) % o.count], incident * 2 + 1 } };
        const Vec2 v1 = r.points[face], v2 = r.points[(face + 1) % r.count];
        Vec2 side = v2 - v1;
        const float length = sqrtf(side.dot(side));
        side = length > 0 ? side / length : Vec2(1, 0);
        Corner once[2], twice[2];
        if (clip(once, edge, -side, -side.dot(v1), 1) < 2 || clip(twice, once, side, side.dot(v2), 2) < 2)
        {
            return 0;
        }
        const float front = normal.dot(v1);
        u32 count = 0;
        for (const Corner& corner : twice)
        {
            const float separation = normal.dot(corner.at) - front;
            if (separation > 0)
            {
                continue;
            }
            Contact& c = contacts[count++];
            c = {};
            c.separation = separation;
            c.normal = flip ? -normal : normal;
            c.position = corner.at - normal * (separation * 0.5f);
            c.feature = corner.feature | (face << 8) | (flip ? 1u << 18 : 0u) | (1u << 19);
        }
        return count;
    }

    u32 World::touchCirclePolygon(const Rigid& circle, const Rigid& polygon, Contact& contact)
    {
        // The face it is furthest out from.
        const Vec2 at = circle.position;
        float most = -1e30f;
        u32 face = 0;
        for (u32 i = 0; i < polygon.count; i++)
        {
            const float s = polygon.normals[i].dot(at - polygon.points[i]);
            if (s > most)
            {
                most = s;
                face = i;
            }
        }
        const float radius = circle.halfWidth;
        if (most > radius)
        {
            return 0;
        }
        const Vec2 v1 = polygon.points[face], v2 = polygon.points[(face + 1) % polygon.count];
        contact = {};
        if (most < 1e-4f)
        {
            // Its middle inside.
            contact.normal = polygon.normals[face];
            contact.separation = most - radius;
            contact.position = at - contact.normal * (radius + contact.separation * 0.5f);
            return 1;
        }
        // Beyond a corner, or over the face.
        const Vec2 along = v2 - v1;
        const float t = along.dot(along) > 0 ? (at - v1).dot(along) / along.dot(along) : 0;
        const Vec2 nearest = t <= 0 ? v1 : t >= 1 ? v2 : v1 + along * t;
        const Vec2 d = at - nearest;
        const float distance = sqrtf(d.dot(d));
        if (distance > radius)
        {
            return 0;
        }
        contact.normal = distance > 1e-5f ? d / distance : polygon.normals[face];
        contact.separation = distance - radius;
        contact.position = nearest;
        contact.feature = (face << 8) | (t <= 0 ? 1u : t >= 1 ? 2u : 0u) | (1u << 20);
        return 1;
    }

    u32 World::touchCircleBox(const Rigid& circle, const Rigid& box, Contact& contact)
    {
        // In the box's own directions.
        const Vec2 d = circle.position - box.position;
        const Vec2 at(d.x * box.c + d.y * box.s, -d.x * box.s + d.y * box.c);
        Vec2 near(clampFloat(at.x, -box.halfWidth, box.halfWidth), clampFloat(at.y, -box.halfHeight, box.halfHeight));
        Vec2 out = at - near;
        float distance = sqrtf(out.dot(out));
        if (distance > circle.halfWidth)
        {
            return 0;
        }
        Vec2 normal;  // from the box to the circle
        float separation;
        if (distance > 1e-4f)
        {
            normal = out / distance;
            separation = distance - circle.halfWidth;
        }
        else
        {
            // The middle is inside: out by the shortest way.
            const float x = box.halfWidth - fabsf(at.x), y = box.halfHeight - fabsf(at.y);
            normal = x < y ? Vec2(at.x < 0 ? -1.0f : 1.0f, 0) : Vec2(0, at.y < 0 ? -1.0f : 1.0f);
            near = x < y ? Vec2(normal.x * box.halfWidth, at.y) : Vec2(at.x, normal.y * box.halfHeight);
            separation = -(x < y ? x : y) - circle.halfWidth;
        }
        contact = {};
        contact.normal = { normal.x * box.c - normal.y * box.s, normal.x * box.s + normal.y * box.c };
        contact.position = box.position + Vec2(near.x * box.c - near.y * box.s, near.x * box.s + near.y * box.c);
        contact.separation = separation;
        return 1;
    }

    u32 World::touchBoxes(const Rigid& a, const Rigid& b, Contact (&contacts)[2])
    {
        const Rigid* boxes[2] = { &a, &b };
        // The direction in which they overlap least, of the four their sides face.
        float best = -1e30f;
        u32 reference = 0, axis = 0;
        for (u32 i = 0; i < 2; i++)
        {
            const Rigid& r = *boxes[i];
            const Rigid& o = *boxes[1 - i];
            const Vec2 d = o.position - r.position;
            for (u32 k = 0; k < 2; k++)
            {
                const Vec2 n = k == 0 ? Vec2(r.c, r.s) : Vec2(-r.s, r.c);
                const float reach = (k == 0 ? r.halfWidth : r.halfHeight) + fabsf(n.x * o.c + n.y * o.s) * o.halfWidth + fabsf(-n.x * o.s + n.y * o.c) * o.halfHeight;
                const float separation = fabsf(d.dot(n)) - reach;
                if (separation > 0)
                {
                    return 0;
                }
                // Prefers the one before when they are nearly the same, so it doesn't flicker between them.
                if (separation > best * 0.95f + 0.01f * (k == 0 ? r.halfWidth : r.halfHeight) || best < -1e29f)
                {
                    best = separation;
                    reference = i;
                    axis = k;
                }
            }
        }
        const Rigid& r = *boxes[reference];
        const Rigid& o = *boxes[1 - reference];
        const Vec2 axes[2] = { { r.c, r.s }, { -r.s, r.c } };
        const float halves[2] = { r.halfWidth, r.halfHeight };
        Vec2 normal = axes[axis];
        const bool flipped = (o.position - r.position).dot(normal) < 0;
        normal = flipped ? -normal : normal;

        // The other box's edge that faces it most.
        const Vec2 local(normal.x * o.c + normal.y * o.s, -normal.x * o.s + normal.y * o.c);
        Corner edge[2];
        Vec2 ends[2];
        u32 face;
        if (fabsf(local.x) > fabsf(local.y))
        {
            const float x = local.x > 0 ? -o.halfWidth : o.halfWidth;
            ends[0] = { x, -o.halfHeight };
            ends[1] = { x, o.halfHeight };
            face = local.x > 0 ? 1 : 2;
        }
        else
        {
            const float y = local.y > 0 ? -o.halfHeight : o.halfHeight;
            ends[0] = { -o.halfWidth, y };
            ends[1] = { o.halfWidth, y };
            face = local.y > 0 ? 3 : 4;
        }
        for (u32 i = 0; i < 2; i++)
        {
            edge[i].at = o.position + Vec2(ends[i].x * o.c - ends[i].y * o.s, ends[i].x * o.s + ends[i].y * o.c);
            edge[i].feature = face * 2 + i;
        }
        // Cut to the sides of the first.
        const Vec2 side = axes[1 - axis];
        const float along = side.dot(r.position);
        Corner once[2], twice[2];
        if (clip(once, edge, side, along + halves[1 - axis], 1) < 2 || clip(twice, once, -side, -along + halves[1 - axis], 2) < 2)
        {
            return 0;
        }
        const float front = normal.dot(r.position) + halves[axis];
        u32 count = 0;
        for (const Corner& corner : twice)
        {
            const float separation = normal.dot(corner.at) - front;
            if (separation > 0)
            {
                continue;
            }
            Contact& c = contacts[count++];
            c = {};
            c.separation = separation;
            c.normal = reference == 0 ? normal : -normal;
            c.position = corner.at - normal * separation;
            c.feature = corner.feature | (reference << 16) | (axis << 17) | (flipped ? 1u << 18 : 0u);
        }
        return count;
    }

    u32 World::touch(const Rigid& a, const Rigid& b, Contact (&contacts)[2])
    {
        if (a.shape == Shape::Circle && b.shape == Shape::Circle)
        {
            const Vec2 d = b.position - a.position;
            const float reach = a.halfWidth + b.halfWidth, square = d.dot(d);
            if (square > reach * reach)
            {
                return 0;
            }
            const float distance = sqrtf(square);
            contacts[0] = {};
            contacts[0].normal = distance > 1e-4f ? d / distance : Vec2(0, 1);
            contacts[0].separation = distance - reach;
            contacts[0].position = a.position + contacts[0].normal * a.halfWidth;
            return 1;
        }
        const bool boxes = a.shape == Shape::Box && b.shape == Shape::Box;
        if (a.shape == Shape::Circle)
        {
            const u32 count = b.shape == Shape::Box ? touchCircleBox(a, b, contacts[0]) : touchCirclePolygon(a, b, contacts[0]);
            contacts[0].normal = -contacts[0].normal;
            return count;
        }
        if (b.shape == Shape::Circle)
        {
            return a.shape == Shape::Box ? touchCircleBox(b, a, contacts[0]) : touchCirclePolygon(b, a, contacts[0]);
        }
        return boxes ? touchBoxes(a, b, contacts) : touchPolygons(a, b, contacts);
    }

    void World::prepareTouch(Touch& t, float dt)
    {
        Rigid& a = m_rigid[t.a];
        Rigid& b = m_rigid[t.b];
        for (u32 i = 0; i < t.count; i++)
        {
            Contact& c = t.contacts[i];
            c.fromA = c.position - a.position;
            c.fromB = c.position - b.position;
            const Vec2 tangent(-c.normal.y, c.normal.x);
            const auto mass = [&](Vec2 direction)
            {
                const float ra = cross(c.fromA, direction), rb = cross(c.fromB, direction);
                const float k = a.inverseMass + b.inverseMass + ra * ra * a.inverseInertia + rb * rb * b.inverseInertia;
                return k > 0 ? 1 / k : 0;
            };
            c.normalMass = mass(c.normal);
            c.tangentMass = mass(tangent);
            c.bias = -kCorrection / dt * fminf(0, c.separation + kSlop);
            const Vec2 speed = b.velocity + cross(b.turning, c.fromB) - a.velocity - cross(a.turning, c.fromA);
            const float closing = -speed.dot(c.normal);
            const float bounce = a.bounce > b.bounce ? a.bounce : b.bounce;
            if (closing > kBounceAbove && bounce * closing > c.bias)
            {
                c.bias = bounce * closing;
            }
            // Start from the answer of the step before.
            const Vec2 impulse = c.normal * c.normalImpulse + tangent * c.tangentImpulse;
            a.velocity = a.velocity - impulse * a.inverseMass;
            a.turning -= a.inverseInertia * cross(c.fromA, impulse);
            b.velocity = b.velocity + impulse * b.inverseMass;
            b.turning += b.inverseInertia * cross(c.fromB, impulse);
        }
    }

    void World::solveTouch(Touch& t)
    {
        Rigid& a = m_rigid[t.a];
        Rigid& b = m_rigid[t.b];
        for (u32 i = 0; i < t.count; i++)
        {
            Contact& c = t.contacts[i];
            const auto speed = [&] { return b.velocity + cross(b.turning, c.fromB) - a.velocity - cross(a.turning, c.fromA); };
            const auto apply = [&](Vec2 impulse)
            {
                a.velocity = a.velocity - impulse * a.inverseMass;
                a.turning -= a.inverseInertia * cross(c.fromA, impulse);
                b.velocity = b.velocity + impulse * b.inverseMass;
                b.turning += b.inverseInertia * cross(c.fromB, impulse);
            };
            // They may only push apart, over the whole step.
            const float before = c.normalImpulse;
            c.normalImpulse = fmaxf(before + c.normalMass * (-speed().dot(c.normal) + c.bias), 0);
            apply(c.normal * (c.normalImpulse - before));
            // Friction holds as hard as they are pressed together.
            const Vec2 tangent(-c.normal.y, c.normal.x);
            const float most = t.friction * c.normalImpulse;
            const float held = c.tangentImpulse;
            c.tangentImpulse = clampFloat(held - c.tangentMass * speed().dot(tangent), -most, most);
            apply(tangent * (c.tangentImpulse - held));
        }
    }

    void World::prepareJoint(Joint& j, float dt)
    {
        j.active = j.rigidA != kNone && (j.b == 0 || j.rigidB != kNone);
        if (!j.active)
        {
            return;
        }
        Rigid& a = m_rigid[j.rigidA];
        j.fromA = { j.localA.x * a.c - j.localA.y * a.s, j.localA.x * a.s + j.localA.y * a.c };
        Vec2 otherEnd = j.localB;
        float inverseMassB = 0, inverseInertiaB = 0;
        j.fromB = {};
        float angleB = 0;
        if (j.b)
        {
            Rigid& b = m_rigid[j.rigidB];
            j.fromB = { j.localB.x * b.c - j.localB.y * b.s, j.localB.x * b.s + j.localB.y * b.c };
            otherEnd = b.position + j.fromB;
            inverseMassB = b.inverseMass;
            inverseInertiaB = b.inverseInertia;
            angleB = b.angle;
        }
        // Turning, about each other.
        const float turnMass = a.inverseInertia + inverseInertiaB;
        j.angularMass = turnMass > 0 ? 1 / turnMass : 0;
        j.angle = angleB - a.angle - j.reference;
        j.angle = atan2f(sinf(j.angle), cosf(j.angle));
        j.angularBias = -kCorrection / dt * j.angle;
        if (!j.limited)
        {
            j.limitImpulse = 0;
        }
        if (!j.motor)
        {
            j.motorImpulse = 0;
        }
        Rigid* bp = j.b ? &m_rigid[j.rigidB] : nullptr;
        const auto turn = [&](float impulse)
        {
            a.turning -= a.inverseInertia * impulse;
            if (bp)
            {
                bp->turning += bp->inverseInertia * impulse;
            }
        };
        const auto push = [&](Vec2 impulse, Vec2 fromA, Vec2 fromB)
        {
            a.velocity = a.velocity - impulse * a.inverseMass;
            a.turning -= a.inverseInertia * cross(fromA, impulse);
            if (bp)
            {
                bp->velocity = bp->velocity + impulse * bp->inverseMass;
                bp->turning += bp->inverseInertia * cross(fromB, impulse);
            }
        };
        if (j.kind == JointKind::Rope || j.kind == JointKind::Spring)
        {
            const Vec2 d = otherEnd - (a.position + j.fromA);
            const float far = sqrtf(d.dot(d));
            j.axis = far > 1e-4f ? d / far : Vec2(0, 1);
            const float ra = cross(j.fromA, j.axis), rb = cross(j.fromB, j.axis);
            float k = a.inverseMass + inverseMassB + a.inverseInertia * ra * ra + inverseInertiaB * rb * rb;
            const float error = far - j.length;
            j.axialSoftness = 0;
            j.axialBias = error > 0 || j.kind == JointKind::Spring ? error * kCorrection / dt : error / dt;
            if (j.kind == JointKind::Spring && j.frequency > 0 && k > 0)
            {
                // A soft constraint: as a spring of that frequency, and a damper, between them.
                const float mass = 1 / k, omega = 6.2831853f * j.frequency;
                const float stiffness = mass * omega * omega, damper = 2 * mass * j.damping * omega;
                const float gamma = dt * (damper + dt * stiffness);
                j.axialSoftness = gamma > 0 ? 1 / gamma : 0;
                j.axialBias = error * dt * stiffness * j.axialSoftness;
                k += j.axialSoftness;
            }
            j.axialMass = k > 0 ? 1 / k : 0;
            if (j.kind == JointKind::Rope && error <= 0)
            {
                j.axialImpulse = 0;
            }
            push(j.axis * j.axialImpulse, j.fromA, j.fromB);
            return;
        }
        if (j.kind == JointKind::Slider)
        {
            // Across the line through b is held; along it, between its limits if it has them; and it doesn't turn.
            j.axis = { j.localAxis.x * (bp ? bp->c : 1) - j.localAxis.y * (bp ? bp->s : 0), j.localAxis.x * (bp ? bp->s : 0) + j.localAxis.y * (bp ? bp->c : 1) };
            const Vec2 across(-j.axis.y, j.axis.x);
            const Vec2 d = (a.position + j.fromA) - otherEnd;
            const float ra = cross(j.fromA, across), rb = cross(j.fromB, across);
            const float k = a.inverseMass + inverseMassB + a.inverseInertia * ra * ra + inverseInertiaB * rb * rb;
            j.axialMass = k > 0 ? 1 / k : 0;
            j.axialBias = -kCorrection / dt * across.dot(d);
            j.along = j.axis.dot(d);
            const float ra2 = cross(j.fromA, j.axis), rb2 = cross(j.fromB, j.axis);
            const float k2 = a.inverseMass + inverseMassB + a.inverseInertia * ra2 * ra2 + inverseInertiaB * rb2 * rb2;
            j.m[0] = k2 > 0 ? 1 / k2 : 0;
            if (!j.limited || (j.along > j.low && j.along < j.high))
            {
                j.limitImpulse = 0;
            }
            push(-(across * j.axialImpulse + j.axis * j.limitImpulse), j.fromA, j.fromB);
            turn(j.angularImpulse);
            return;
        }
        if (j.kind == JointKind::Drag)
        {
            // Softly towards the place, as strong as so many times its weight.
            const float mass = a.inverseMass > 0 ? 1 / a.inverseMass : 1;
            const float omega = 6.2831853f * 4, stiffness = mass * omega * omega, damper = 2 * mass * 0.7f * omega;
            const float gamma = dt * (damper + dt * stiffness);
            j.softness = gamma > 0 ? 1 / gamma : 0;
            const float beta = dt * stiffness * j.softness;
            const Vec2 gap = otherEnd - (a.position + j.fromA);
            j.bias = gap * -beta;
            a.turning *= 0.98f;
            // No harder than so many times its weight.
            j.length = j.strength * mass * fmaxf(sqrtf(m_gravity.dot(m_gravity)), 500.0f) * dt;
        }
        const float m = a.inverseMass + inverseMassB + j.softness;
        const float k11 = m + a.inverseInertia * j.fromA.y * j.fromA.y + inverseInertiaB * j.fromB.y * j.fromB.y;
        const float k12 = -a.inverseInertia * j.fromA.x * j.fromA.y - inverseInertiaB * j.fromB.x * j.fromB.y;
        const float k22 = m + a.inverseInertia * j.fromA.x * j.fromA.x + inverseInertiaB * j.fromB.x * j.fromB.x;
        const float determinant = k11 * k22 - k12 * k12;
        const float inverse = fabsf(determinant) > 1e-12f ? 1 / determinant : 0;
        j.m[0] = k22 * inverse;
        j.m[1] = -k12 * inverse;
        j.m[2] = -k12 * inverse;
        j.m[3] = k11 * inverse;
        if (j.kind != JointKind::Drag)
        {
            const Vec2 gap = otherEnd - (a.position + j.fromA);
            j.bias = gap * (-kCorrection / dt);
        }
        a.velocity = a.velocity - j.impulse * a.inverseMass;
        a.turning -= a.inverseInertia * cross(j.fromA, j.impulse);
        if (j.b)
        {
            Rigid& b = m_rigid[j.rigidB];
            b.velocity = b.velocity + j.impulse * b.inverseMass;
            b.turning += b.inverseInertia * cross(j.fromB, j.impulse);
        }
        turn(j.angularImpulse + j.limitImpulse + j.motorImpulse);
    }

    void World::solveJoint(Joint& j)
    {
        if (!j.active)
        {
            return;
        }
        Rigid& a = m_rigid[j.rigidA];
        Rigid* b = j.b ? &m_rigid[j.rigidB] : nullptr;
        const auto spinApart = [&] { return (b ? b->turning : 0) - a.turning; };
        const auto turn = [&](float impulse)
        {
            a.turning -= a.inverseInertia * impulse;
            if (b)
            {
                b->turning += b->inverseInertia * impulse;
            }
        };
        const auto speedAt = [&] { return (b ? b->velocity + cross(b->turning, j.fromB) : Vec2()) - a.velocity - cross(a.turning, j.fromA); };
        const auto push = [&](Vec2 impulse)
        {
            a.velocity = a.velocity - impulse * a.inverseMass;
            a.turning -= a.inverseInertia * cross(j.fromA, impulse);
            if (b)
            {
                b->velocity = b->velocity + impulse * b->inverseMass;
                b->turning += b->inverseInertia * cross(j.fromB, impulse);
            }
        };
        if (j.kind == JointKind::Rope || j.kind == JointKind::Spring)
        {
            const float closing = speedAt().dot(j.axis);
            const float before = j.axialImpulse;
            float more = -(closing + j.axialBias + j.axialSoftness * before) * j.axialMass;
            j.axialImpulse = j.kind == JointKind::Rope ? fminf(0.0f, before + more) : before + more;
            push(j.axis * (j.axialImpulse - before));
            return;
        }
        if (j.kind == JointKind::Slider)
        {
            const Vec2 across(-j.axis.y, j.axis.x);
            const float a1 = (j.angularBias - spinApart()) * j.angularMass;
            j.angularImpulse += a1;
            turn(a1);
            const float drift = -speedAt().dot(across);
            const float m1 = (j.axialBias - drift) * j.axialMass;
            j.axialImpulse += m1;
            push(-across * m1);
            if (j.limited && (j.along <= j.low || j.along >= j.high))
            {
                // Out along it no further than its ends.
                const float moving = -speedAt().dot(j.axis);
                const bool atLow = j.along <= j.low;
                const float error = atLow ? j.along - j.low : j.along - j.high;
                const float before = j.limitImpulse;
                float want = before + (-(moving) - error * kCorrection * 60) * j.m[0];
                j.limitImpulse = atLow ? fmaxf(0.0f, want) : fminf(0.0f, want);
                push(-j.axis * (j.limitImpulse - before));
            }
            return;
        }
        // Held at the same turn, as a weld is.
        if (j.kind == JointKind::Weld)
        {
            const float more = (j.angularBias - spinApart()) * j.angularMass;
            j.angularImpulse += more;
            turn(more);
        }
        // A hinge's motor, as strong as its torque allows.
        if (j.motor)
        {
            const float most = j.motorTorque / 60.0f;
            const float before = j.motorImpulse;
            j.motorImpulse = clampFloat(before - (spinApart() - j.motorSpeed) * j.angularMass, -most, most);
            turn(j.motorImpulse - before);
        }
        // And its limits.
        if (j.limited && (j.angle <= j.low || j.angle >= j.high))
        {
            const bool atLow = j.angle <= j.low;
            const float error = atLow ? j.angle - j.low : j.angle - j.high;
            const float before = j.limitImpulse;
            const float want = before - (spinApart() + error * kCorrection * 60) * j.angularMass;
            j.limitImpulse = atLow ? fmaxf(0.0f, want) : fminf(0.0f, want);
            turn(j.limitImpulse - before);
        }
        const Vec2 speedB = b ? b->velocity + cross(b->turning, j.fromB) : Vec2();
        const Vec2 speed = speedB - a.velocity - cross(a.turning, j.fromA);
        const Vec2 wanted = j.bias - speed - j.impulse * j.softness;
        const Vec2 impulse(j.m[0] * wanted.x + j.m[2] * wanted.y, j.m[1] * wanted.x + j.m[3] * wanted.y);
        a.velocity = a.velocity - impulse * a.inverseMass;
        a.turning -= a.inverseInertia * cross(j.fromA, impulse);
        if (b)
        {
            b->velocity = b->velocity + impulse * b->inverseMass;
            b->turning += b->inverseInertia * cross(j.fromB, impulse);
        }
        j.impulse = j.impulse + impulse;
        if (j.kind == JointKind::Drag)
        {
            const float most = sqrtf(j.impulse.dot(j.impulse));
            if (most > j.length && most > 0)
            {
                const Vec2 kept = j.impulse * (j.length / most);
                const Vec2 back = kept - j.impulse;
                j.impulse = kept;
                a.velocity = a.velocity - back * a.inverseMass;
                a.turning -= a.inverseInertia * cross(j.fromA, back);
            }
        }
    }

    bool World::join(u32 a, u32 b, Vec2 at, float softness)
    {
        const Entity* ea = find(a);
        const Entity* eb = find(b);
        if (ea == nullptr || (b && eb == nullptr) || a == b)
        {
            return false;
        }
        const auto local = [&](const Entity& e)
        {
            const Vec2 d = at - e.position;
            const float c = cosf(e.rotation * kDegrees), s = sinf(e.rotation * kDegrees);
            return Vec2(d.x * c + d.y * s, -d.x * s + d.y * c);
        };
        Joint j = {};
        j.a = a;
        j.b = b;
        j.localA = local(*ea);
        j.localB = eb ? local(*eb) : at;
        j.softness = softness > 0 ? softness : 0;
        j.reference = ((eb ? eb->rotation : 0) - ea->rotation) * kDegrees;
        return m_joints.push(j);
    }

    namespace
    {
        Vec2 localOf(const World::Entity& e, Vec2 at)
        {
            const Vec2 d = at - e.position;
            const float c = cosf(e.rotation * kDegrees), s = sinf(e.rotation * kDegrees);
            return { d.x * c + d.y * s, -d.x * s + d.y * c };
        }
    }

    bool World::hinge(u32 a, u32 b, Vec2 at, float low, float high, float motorSpeed, float motorTorque)
    {
        if (!join(a, b, at, 0))
        {
            return false;
        }
        Joint& j = m_joints[m_joints.count() - 1];
        // Turning is measured as b's from a's; these are said as a's own, so they are turned round.
        j.limited = low < high;
        j.low = -high * kDegrees;
        j.high = -low * kDegrees;
        j.motor = motorTorque > 0;
        j.motorSpeed = -motorSpeed * kDegrees;
        j.motorTorque = motorTorque;
        return true;
    }

    bool World::weld(u32 a, u32 b, Vec2 at)
    {
        if (!join(a, b, at, 0))
        {
            return false;
        }
        m_joints[m_joints.count() - 1].kind = JointKind::Weld;
        return true;
    }

    bool World::rope(u32 a, u32 b, Vec2 atA, Vec2 atB, float length)
    {
        const Entity* ea = find(a);
        const Entity* eb = b ? find(b) : nullptr;
        if (ea == nullptr || (b && eb == nullptr) || a == b)
        {
            return false;
        }
        Joint j = {};
        j.kind = JointKind::Rope;
        j.a = a;
        j.b = b;
        j.localA = localOf(*ea, atA);
        j.localB = eb ? localOf(*eb, atB) : atB;
        const Vec2 d = atB - atA;
        j.length = length >= 0 ? length : sqrtf(d.dot(d));
        return m_joints.push(j);
    }

    bool World::spring(u32 a, u32 b, Vec2 atA, Vec2 atB, float frequency, float damping, float length)
    {
        if (!rope(a, b, atA, atB, length))
        {
            return false;
        }
        Joint& j = m_joints[m_joints.count() - 1];
        j.kind = JointKind::Spring;
        j.frequency = frequency > 0 ? frequency : 0;
        j.damping = damping > 0 ? damping : 0;
        return true;
    }

    bool World::slider(u32 a, u32 b, Vec2 at, Vec2 axis, float low, float high)
    {
        const Entity* eb = b ? find(b) : nullptr;
        if (axis.dot(axis) < 1e-8f || !join(a, b, at, 0))
        {
            return false;
        }
        Joint& j = m_joints[m_joints.count() - 1];
        j.kind = JointKind::Slider;
        axis = axis / sqrtf(axis.dot(axis));
        // In b's own directions, so that the line turns with it.
        const float c = eb ? cosf(eb->rotation * kDegrees) : 1, s = eb ? sinf(eb->rotation * kDegrees) : 0;
        j.localAxis = { axis.x * c + axis.y * s, -axis.x * s + axis.y * c };
        j.limited = low < high;
        j.low = low;
        j.high = high;
        return true;
    }

    bool World::drag(u32 a, Vec2 at, float strength)
    {
        const Entity* ea = find(a);
        if (ea == nullptr)
        {
            return false;
        }
        Joint j = {};
        j.kind = JointKind::Drag;
        j.a = a;
        j.localA = localOf(*ea, at);
        j.localB = at;
        j.strength = strength > 0 ? strength : 1;
        return m_joints.push(j);
    }

    void World::dragTo(u32 a, Vec2 target)
    {
        for (Joint& j : m_joints)
        {
            if (j.a == a && j.kind == JointKind::Drag)
            {
                j.localB = target;
            }
        }
    }

    // Bodies joined together pass through each other, as a wheel through the car it turns on.
    bool World::joined(u32 a, u32 b) const
    {
        for (const Joint& j : m_joints)
        {
            if ((j.a == a && j.b == b) || (j.a == b && j.b == a))
            {
                return true;
            }
        }
        return false;
    }

    void World::setMotor(u32 a, float speed, float torque)
    {
        for (Joint& j : m_joints)
        {
            if (j.a == a && j.kind == JointKind::Pin)
            {
                j.motor = torque > 0;
                j.motorSpeed = -speed * kDegrees;
                j.motorTorque = torque;
            }
        }
    }

    void World::unjoin(u32 id)
    {
        for (u32 i = 0; i < m_joints.count();)
        {
            if (m_joints[i].a == id || m_joints[i].b == id)
            {
                m_joints.swapRemove(i);
                continue;
            }
            i++;
        }
    }

    bool World::contact(u32 a, u32 b, Vec2& at, Vec2& normal, float& impulse) const
    {
        for (const Touch& t : m_touches)
        {
            const Rigid& x = m_rigid[t.a];
            const Rigid& y = m_rigid[t.b];
            const u32 ix = idOf(x.entity), iy = idOf(y.entity);
            const bool same = x.body != kNone || y.body != kNone;
            if (!same || !((ix == a && iy == b) || (ix == b && iy == a)) || t.count == 0)
            {
                continue;
            }
            at = {};
            impulse = 0;
            for (u32 i = 0; i < t.count; i++)
            {
                at = at + t.contacts[i].position / static_cast<float>(t.count);
                impulse += t.contacts[i].normalImpulse;
            }
            normal = ix == a ? t.contacts[0].normal : -t.contacts[0].normal;
            return true;
        }
        return false;
    }

    void World::push(u32 id, Vec2 impulse, const Vec2* at)
    {
        Body* b = getBody(id);
        const Entity* e = find(id);
        if (b == nullptr)
        {
            return;
        }
        const float mass = b->mass > 0 ? b->mass : 1;
        b->velocity = b->velocity + impulse / mass;
        const Collider* c = getCollider(id);
        if (at && c && b->mass > 0)
        {
            const float square = c->shape == Shape::Circle ? c->halfWidth * c->halfWidth * 0.5f : (c->halfWidth * c->halfWidth + c->halfHeight * c->halfHeight) / 3;
            if (square > 0)
            {
                b->spin += cross(*at - e->position, impulse) / (mass * square) / kDegrees;
            }
        }
    }

    void World::buildWalls(const TileMap& map)
    {
        // Along each row, as one box, so bodies slide across the joins.
        for (u32 y = 0; y < map.height; y++)
        {
            for (u32 x = 0; x < map.width;)
            {
                const auto solid = [&](u32 column)
                {
                    const u16 tile = map.tiles[static_cast<size_t>(y) * map.width + column];
                    return tile && tile < 64 && map.solid[tile];
                };
                if (!solid(x))
                {
                    x++;
                    continue;
                }
                const u32 from = x;
                while (x < map.width && solid(x))
                {
                    x++;
                }
                m_walls.push({ map.origin.x + static_cast<float>(from) * map.size, map.origin.x + static_cast<float>(x) * map.size,
                    map.origin.y + static_cast<float>(y) * map.size, map.origin.y + static_cast<float>(y + 1) * map.size });
            }
        }
    }

    void World::physics(float dt)
    {
        m_rigid.clear();
        bool any = false;
        for (u32 i = 0; i < m_bodies.count(); i++)
        {
            const Body& b = m_bodies[i];
            const Entity& e = m_entities[b.owner];
            const u32 collider = e.parts[static_cast<size_t>(Component::Collider)];
            if (b.mass > 0 && collider != kNone && e.parent == kNone && !e.dying && !m_colliders[collider].wall && m_colliders[collider].shape != Shape::Chain)
            {
                addRigid(m_colliders[collider], i);
                any = true;
            }
        }
        m_stats.contacts = 0;
        if (!any)
        {
            m_touches.clear();
            return;
        }
        for (const Collider& c : m_colliders)
        {
            if (m_entities[c.owner].dying)
            {
                continue;
            }
            if (c.shape == Shape::Chain)
            {
                // Each piece of it a polygon of two corners, facing both ways.
                Vec2 points[256];
                const u32 count = worldPoints(c, points, 256);
                const u32 pieces = count >= 2 ? (c.loop ? count : count - 1) : 0;
                for (u32 k = 0; k < pieces; k++)
                {
                    Rigid r = {};
                    r.id = k == 0 ? idOf(c.owner) : (idOf(c.owner) * 2654435761u) ^ (k << 1) ^ 1u;
                    r.body = kNone;
                    r.entity = c.owner;
                    r.shape = Shape::Polygon;
                    r.oneWay = c.oneWay;
                    r.friction = 0.6f;
                    r.count = 2;
                    r.points[0] = points[k];
                    r.points[1] = points[(k + 1) % count];
                    const Vec2 edge = r.points[1] - r.points[0];
                    const float length = sqrtf(edge.dot(edge));
                    r.normals[0] = length > 0 ? Vec2(edge.y / length, -edge.x / length) : Vec2(0, -1);
                    r.normals[1] = -r.normals[0];
                    r.position = (r.points[0] + r.points[1]) * 0.5f;
                    r.c = 1;
                    m_rigid.push(r);
                }
                continue;
            }
            if (c.wall)
            {
                addRigid(c, kNone);
            }
        }
        for (u32 i = 0; i < m_walls.count(); i++)
        {
            const Box& w = m_walls[i];
            Rigid r = {};
            r.id = i;  // generation 0, so it can't be an entity's
            r.body = kNone;
            r.shape = Shape::Box;
            r.position = { (w.minX + w.maxX) * 0.5f, (w.minY + w.maxY) * 0.5f };
            r.c = 1;
            r.friction = 0.6f;
            r.halfWidth = (w.maxX - w.minX) * 0.5f;
            r.halfHeight = (w.maxY - w.minY) * 0.5f;
            m_rigid.push(r);
        }

        m_rigidOrder.clear();
        for (u32 i = 0; i < m_rigid.count(); i++)
        {
            Rigid& r = m_rigid[i];
            if (r.body != kNone)
            {
                const Body& b = m_bodies[r.body];
                r.velocity = r.velocity + m_gravity * (b.gravity * dt);
                const float kept = clampFloat(1 - b.drag * dt, 0, 1);
                r.velocity = r.velocity * kept;
                r.turning *= kept;
            }
            const float x = r.shape == Shape::Circle ? r.halfWidth : fabsf(r.c) * r.halfWidth + fabsf(r.s) * r.halfHeight;
            const float y = r.shape == Shape::Circle ? r.halfWidth : fabsf(r.s) * r.halfWidth + fabsf(r.c) * r.halfHeight;
            r.minX = r.position.x - x;
            r.maxX = r.position.x + x;
            r.minY = r.position.y - y;
            r.maxY = r.position.y + y;
            if (r.shape == Shape::Polygon && r.count)
            {
                r.minX = r.maxX = r.points[0].x;
                r.minY = r.maxY = r.points[0].y;
                for (u32 p = 1; p < r.count; p++)
                {
                    r.minX = fminf(r.minX, r.points[p].x);
                    r.maxX = fmaxf(r.maxX, r.points[p].x);
                    r.minY = fminf(r.minY, r.points[p].y);
                    r.maxY = fmaxf(r.maxY, r.points[p].y);
                }
            }
            m_rigidOrder.push(i);
        }
        std::sort(m_rigidOrder.begin(), m_rigidOrder.end(), [this](u32 a, u32 b) { return m_rigid[a].minX < m_rigid[b].minX; });

        // Touches, with what is known of them from the step before.
        m_touchesBefore.swap(m_touches);
        m_touches.clear();
        for (u32 i = 0; i < m_rigidOrder.count(); i++)
        {
            const u32 first = m_rigidOrder[i];
            for (u32 k = i + 1; k < m_rigidOrder.count() && m_rigid[m_rigidOrder[k]].minX <= m_rigid[first].maxX; k++)
            {
                const u32 second = m_rigidOrder[k];
                const bool swap = m_rigid[first].id > m_rigid[second].id;
                Touch t = {};
                t.a = swap ? second : first;
                t.b = swap ? first : second;
                const Rigid& a = m_rigid[t.a];
                const Rigid& b = m_rigid[t.b];
                if ((a.body == kNone && b.body == kNone) || a.minY > b.maxY || b.minY > a.maxY || joined(a.id, b.id))
                {
                    continue;
                }
                t.count = touch(a, b, t.contacts);
                if (t.count == 0)
                {
                    continue;
                }
                if (a.oneWay != b.oneWay)
                {
                    // Held up by it only when coming down onto its top, not when coming up through it or from the side.
                    const bool aIs = a.oneWay;
                    const Rigid& mover = aIs ? b : a;
                    const Vec2 up = m_gravity.dot(m_gravity) > 0 ? -m_gravity / sqrtf(m_gravity.dot(m_gravity)) : Vec2(0, -1);
                    const Vec2 out = aIs ? t.contacts[0].normal : -t.contacts[0].normal;   // from the platform to the mover
                    const float reach = mover.shape == Shape::Circle ? mover.halfWidth : fmaxf(mover.halfWidth, mover.halfHeight);
                    // As deep as it could have come down this step, and a little more: any deeper, it came from below.
                    const float deepest = fmaxf(4.0f, reach * 0.5f) + fmaxf(0.0f, -mover.velocity.dot(up)) * dt * 1.5f;
                    if (out.dot(up) < 0.7f || mover.velocity.dot(up) > 1 || t.contacts[0].separation < -deepest)
                    {
                        continue;
                    }
                }
                t.key = (static_cast<u64>(a.id) << 32) | b.id;
                t.friction = sqrtf(a.friction * b.friction);
                const Touch* end = m_touchesBefore.begin() + m_touchesBefore.count();
                const Touch* old = std::lower_bound(m_touchesBefore.begin(), m_touchesBefore.begin() + m_touchesBefore.count(), t.key, [](const Touch& x, u64 key) { return x.key < key; });
                if (old != end && old->key == t.key)
                {
                    for (u32 n = 0; n < t.count; n++)
                    {
                        for (u32 o = 0; o < old->count; o++)
                        {
                            if (old->contacts[o].feature == t.contacts[n].feature)
                            {
                                t.contacts[n].normalImpulse = old->contacts[o].normalImpulse;
                                t.contacts[n].tangentImpulse = old->contacts[o].tangentImpulse;
                            }
                        }
                    }
                }
                m_stats.contacts += t.count;
                m_touches.push(t);
            }
        }
        std::sort(m_touches.begin(), m_touches.begin() + m_touches.count(), [](const Touch& a, const Touch& b) { return a.key < b.key; });

        for (u32 i = 0; i < m_joints.count();)
        {
            Joint& j = m_joints[i];
            if (!alive(j.a) || (j.b && !alive(j.b)))
            {
                m_joints.swapRemove(i);
                continue;
            }
            j.rigidA = j.rigidB = kNone;
            for (u32 r = 0; r < m_rigid.count(); r++)
            {
                j.rigidA = m_rigid[r].body != kNone && m_rigid[r].id == j.a ? r : j.rigidA;
                j.rigidB = j.b && m_rigid[r].id == j.b ? r : j.rigidB;
            }
            i++;
        }

        for (Touch& t : m_touches)
        {
            prepareTouch(t, dt);
        }
        for (Joint& j : m_joints)
        {
            prepareJoint(j, dt);
        }
        for (u32 i = 0; i < kIterations; i++)
        {
            for (Joint& j : m_joints)
            {
                solveJoint(j);
            }
            for (Touch& t : m_touches)
            {
                solveTouch(t);
            }
        }

        for (const Rigid& r : m_rigid)
        {
            if (r.body == kNone)
            {
                continue;
            }
            Body& b = m_bodies[r.body];
            Entity& e = m_entities[r.entity];
            b.velocity = r.velocity;
            b.spin = r.turning / kDegrees;
            e.position = e.position + r.velocity * dt;
            e.rotation += b.spin * dt;
            // Nothing that can't be recovered from, whatever it was given.
            if (!(fabsf(e.position.x) < 1e7f && fabsf(e.position.y) < 1e7f && fabsf(e.rotation) < 1e7f))
            {
                markDying(r.entity);
                e.position = {};
                e.rotation = 0;
                b.velocity = {};
                b.spin = 0;
            }
            b.onGround = b.onCeiling = false;
        }
        for (const Touch& t : m_touches)
        {
            for (u32 side = 0; side < 2; side++)
            {
                const Rigid& r = m_rigid[side ? t.b : t.a];
                if (r.body != kNone && t.contacts[0].normalImpulse > 0)
                {
                    const float down = side ? -t.contacts[0].normal.y : t.contacts[0].normal.y;
                    m_bodies[r.body].onGround = m_bodies[r.body].onGround || down > 0.6f;
                }
            }
        }
    }
}
