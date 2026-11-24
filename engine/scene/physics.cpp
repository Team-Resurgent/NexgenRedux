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
        if (body != kNone)
        {
            const Body& b = m_bodies[body];
            r.position = e.position;
            r.velocity = b.velocity;
            r.turning = b.spin * kDegrees;
            r.friction = b.friction;
            r.bounce = b.bounce;
            r.inverseMass = 1 / b.mass;
            const float inertia = c.shape == Shape::Circle ? b.mass * r.halfWidth * r.halfWidth * 0.5f : b.mass * (r.halfWidth * r.halfWidth + r.halfHeight * r.halfHeight) / 3;
            r.inverseInertia = inertia > 0 ? 1 / inertia : 0;
        }
        r.c = cosf(r.angle);
        r.s = sinf(r.angle);
        m_rigid.push(r);
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
        if (a.shape == Shape::Circle)
        {
            const u32 count = touchCircleBox(a, b, contacts[0]);
            contacts[0].normal = -contacts[0].normal;
            return count;
        }
        return b.shape == Shape::Circle ? touchCircleBox(b, a, contacts[0]) : touchBoxes(a, b, contacts);
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
        if (j.b)
        {
            Rigid& b = m_rigid[j.rigidB];
            j.fromB = { j.localB.x * b.c - j.localB.y * b.s, j.localB.x * b.s + j.localB.y * b.c };
            otherEnd = b.position + j.fromB;
            inverseMassB = b.inverseMass;
            inverseInertiaB = b.inverseInertia;
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
        const Vec2 gap = otherEnd - (a.position + j.fromA);
        j.bias = gap * (-kCorrection / dt);
        a.velocity = a.velocity - j.impulse * a.inverseMass;
        a.turning -= a.inverseInertia * cross(j.fromA, j.impulse);
        if (j.b)
        {
            Rigid& b = m_rigid[j.rigidB];
            b.velocity = b.velocity + j.impulse * b.inverseMass;
            b.turning += b.inverseInertia * cross(j.fromB, j.impulse);
        }
    }

    void World::solveJoint(Joint& j)
    {
        if (!j.active)
        {
            return;
        }
        Rigid& a = m_rigid[j.rigidA];
        Rigid* b = j.b ? &m_rigid[j.rigidB] : nullptr;
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
        return m_joints.push(j);
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
            if (b.mass > 0 && collider != kNone && e.parent == kNone && !e.dying && !m_colliders[collider].wall)
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
            if (c.wall && !m_entities[c.owner].dying)
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
                if ((a.body == kNone && b.body == kNone) || a.minY > b.maxY || b.minY > a.maxY)
                {
                    continue;
                }
                t.count = touch(a, b, t.contacts);
                if (t.count == 0)
                {
                    continue;
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
