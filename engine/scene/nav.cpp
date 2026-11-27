#include "scene/space.h"
#include "scene/world.h"

#include <math.h>
#include <string.h>

// Where things can walk, as squares over a part of the world, and the shortest way between two places: for those
// that the game moves about by itself. Each square has the height of what is under it; a step between two squares
// is allowed if it is no higher than a climb.
namespace nx
{
    namespace
    {
        constexpr u8 kWalk = 1, kEdge = 2;
        constexpr u32 kMostSide = 256;

        struct Open
        {
            float f;
            u32 cell;
        };
    }

    void Space::drawOverlays()
    {
        gpu::Device& device = *m_solids->device();
        gpu::MeshDraw d;
        d.lines = true;
        d.lit = d.fogged = d.depthWrite = false;
        d.cull = gpu::Cull::None;
        if (m_showNavigation && m_navSide)
        {
            if (m_navMeshVersion != m_navVersion || !m_navMesh)
            {
                // A square for each place that can be walked on; every second or more of them, if there are very many.
                device.destroyMesh(m_navMesh);
                m_navMesh = {};
                m_navMeshVersion = m_navVersion;
                u32 walkable = 0;
                for (u32 i = 0; i < m_navSide * m_navSide; i++)
                {
                    walkable += (m_navFlags[i] & kWalk) ? 1 : 0;
                }
                u32 stride = 1;
                while (walkable / (stride * stride) > 16000)
                {
                    stride++;
                }
                MeshData mesh;
                const gpu::Color green(0.3f, 1, 0.5f, 1);
                const float c = m_navCell * 0.45f;
                for (u32 z = 0; z < m_navSide; z += stride)
                {
                    for (u32 x = 0; x < m_navSide; x += stride)
                    {
                        const u32 i = z * m_navSide + x;
                        if (!(m_navFlags[i] & kWalk))
                        {
                            continue;
                        }
                        const Vec3 m(m_navLow.x + (static_cast<float>(x) + 0.5f) * m_navCell, m_navHeight[i] + 0.04f, m_navLow.z + (static_cast<float>(z) + 0.5f) * m_navCell);
                        mesh.quad(m + Vec3(-c, 0, c), m + Vec3(c, 0, c), m + Vec3(c, 0, -c), m + Vec3(-c, 0, -c), green);
                    }
                }
                if (mesh.indices.count())
                {
                    m_navMesh = device.createMesh(mesh.vertices.begin(), mesh.vertices.count(), mesh.indices.begin(), mesh.indices.count(), false);
                }
            }
            if (m_navMesh)
            {
                d.mesh = m_navMesh;
                d.world = Mat4();
                d.color = { 1, 1, 1, 0.8f };
                d.blend = gpu::Blend::Alpha;
                device.drawMesh(d);
            }
        }
        if (!m_showShapes)
        {
            return;
        }
        d.blend = gpu::Blend::None;
        for (const Collider3& c : m_colliders)
        {
            if (!visibleCollider(c))
            {
                continue;
            }
            const World::Entity& e = m_world.m_entities[c.owner];
            const Body3* body = e.parts3[4] != kNone ? &m_bodies[e.parts3[4]] : nullptr;
            d.color = c.together ? gpu::Color(1, 0.6f, 0.2f, 1) : c.wall ? gpu::Color(0.75f, 0.75f, 0.75f, 1)
                    : body && body->walker ? gpu::Color(0.35f, 0.7f, 1, 1) : body && body->mass > 0 ? (body->still > 0.5f ? gpu::Color(0.2f, 0.55f, 0.3f, 1) : gpu::Color(0.35f, 1, 0.45f, 1))
                    : gpu::Color(1, 0.85f, 0.3f, 1);
            Rigid r;
            makeRigid(c, r);
            const auto shape = [&](Solid solid, const Vec3& place, const Vec3& size)
            {
                d.mesh = m_solids->get(solid, 1);
                d.first = d.count = 0;
                d.world = compose(place, r.turn, size);
                device.drawMesh(d);
            };
            if (r.shape == Shape3::Sphere)
            {
                shape(Solid::Sphere, r.place, Vec3(1, 1, 1) * (r.half.x * 2));
            }
            else if (r.shape == Shape3::Capsule)
            {
                // Its round ends and its straight middle.
                const float straight = fmaxf(0, r.half.y - r.half.x);
                const Vec3 reach = r.axes[1] * straight;
                shape(Solid::Sphere, r.place + reach, Vec3(1, 1, 1) * (r.half.x * 2));
                shape(Solid::Sphere, r.place - reach, Vec3(1, 1, 1) * (r.half.x * 2));
                if (straight > 0)
                {
                    shape(Solid::Cylinder, r.place, Vec3(r.half.x * 2, straight * 2, r.half.x * 2));
                }
            }
            else if (r.shape == Shape3::Shell && r.model && r.model->gpuMesh)
            {
                // Its own triangles, of the first level.
                u32 first = 0xFFFFFFFFu, end = 0;
                for (const ModelPart& part : r.model->parts)
                {
                    first = part.first[0] < first ? part.first[0] : first;
                    end = part.first[0] + part.count[0] > end ? part.first[0] + part.count[0] : end;
                }
                d.mesh = r.model->gpuMesh;
                d.first = first;
                d.count = end > first ? end - first : 0;
                d.world = m_spatials[e.parts3[0]].world;
                device.drawMesh(d);
                d.first = d.count = 0;
            }
            else
            {
                shape(Solid::Box, r.place, r.half * 2);
            }
        }
    }

    void Space::clearNav()
    {
        m_navVersion++;
        m_navSide = 0;
        m_navHeight.clear();
        m_navFlags.clear();
        m_navCost.clear();
        m_navFrom.clear();
        m_navSeen.clear();
    }

    bool Space::navigate(const Vec3& middle, float size, float cell, float climb, u32 groups)
    {
        clearNav();
        cell = cell < 0.25f ? 0.25f : cell;
        u32 side = static_cast<u32>(ceilf(size / cell));
        side = side < 2 ? 2 : side > kMostSide ? kMostSide : side;
        const u32 count = side * side;
        if (!m_navHeight.reserve(count) || !m_navFlags.reserve(count) || !m_navCost.reserve(count) || !m_navFrom.reserve(count) || !m_navSeen.reserve(count))
        {
            clearNav();
            return false;
        }
        m_navHeight.resize(count);
        m_navFlags.resize(count);
        m_navCost.resize(count);
        m_navFrom.resize(count);
        m_navSeen.resize(count);
        memset(m_navSeen.begin(), 0, count * sizeof(u32));
        m_navVisit = 0;
        m_navVersion++;
        m_navSide = side;
        m_navCell = cell;
        m_navClimb = climb;
        m_navLow = { middle.x - static_cast<float>(side) * cell * 0.5f, middle.y, middle.z - static_cast<float>(side) * cell * 0.5f };
        const float steep = 0.64f;   // about 50 degrees
        for (u32 z = 0; z < side; z++)
        {
            for (u32 x = 0; x < side; x++)
            {
                const u32 i = z * side + x;
                const float wx = m_navLow.x + (static_cast<float>(x) + 0.5f) * cell, wz = m_navLow.z + (static_cast<float>(z) + 0.5f) * cell;
                const float land = heightAt(wx, wz);
                const bool any = land > -9e5f;
                const float top = (any ? fmaxf(land, middle.y) : middle.y) + 30, bottom = any ? land - 2 : middle.y - 30;
                u32 id = 0;
                Vec3 hit, normal;
                m_navFlags[i] = 0;
                m_navHeight[i] = -1e9f;
                if (raycast({ wx, top, wz }, { wx, bottom, wz }, groups | 1, 0, id, hit, normal))
                {
                    m_navHeight[i] = hit.y;
                    const bool wet = m_hasWater && hit.y < m_water - 0.5f;
                    if (normal.y >= steep && !wet)
                    {
                        m_navFlags[i] = kWalk;
                    }
                }
            }
        }
        // Squares beside where it can't go cost more, so that ways keep off walls.
        for (u32 z = 0; z < side; z++)
        {
            for (u32 x = 0; x < side; x++)
            {
                const u32 i = z * side + x;
                if (!(m_navFlags[i] & kWalk))
                {
                    continue;
                }
                static const int around[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                for (const auto& o : around)
                {
                    const int nx = static_cast<int>(x) + o[0], nz = static_cast<int>(z) + o[1];
                    if (nx < 0 || nz < 0 || nx >= static_cast<int>(side) || nz >= static_cast<int>(side) || !navStep(i, static_cast<u32>(nz) * side + static_cast<u32>(nx)))
                    {
                        m_navFlags[i] |= kEdge;
                    }
                }
            }
        }
        return true;
    }

    bool Space::navStep(u32 a, u32 b) const
    {
        return (m_navFlags[a] & kWalk) && (m_navFlags[b] & kWalk) && fabsf(m_navHeight[a] - m_navHeight[b]) <= m_navClimb;
    }

    bool Space::navCellOf(const Vec3& at, u32& cell) const
    {
        if (m_navSide == 0)
        {
            return false;
        }
        const int x = static_cast<int>(floorf((at.x - m_navLow.x) / m_navCell)), z = static_cast<int>(floorf((at.z - m_navLow.z) / m_navCell));
        if (x < 0 || z < 0 || x >= static_cast<int>(m_navSide) || z >= static_cast<int>(m_navSide))
        {
            return false;
        }
        cell = static_cast<u32>(z) * m_navSide + static_cast<u32>(x);
        return true;
    }

    Vec3 Space::navPlace(u32 cell) const
    {
        const u32 x = cell % m_navSide, z = cell / m_navSide;
        return { m_navLow.x + (static_cast<float>(x) + 0.5f) * m_navCell, m_navHeight[cell], m_navLow.z + (static_cast<float>(z) + 0.5f) * m_navCell };
    }

    bool Space::walkable(const Vec3& at) const
    {
        u32 cell;
        return navCellOf(at, cell) && (m_navFlags[cell] & kWalk) && fabsf(m_navHeight[cell] - at.y) < 2.5f;
    }

    // The walkable square nearest a place, looking in rings round it; the one at its height if there are several.
    bool Space::navNearest(const Vec3& at, u32& cell) const
    {
        if (m_navSide == 0)
        {
            return false;
        }
        const int cx = static_cast<int>(floorf((at.x - m_navLow.x) / m_navCell)), cz = static_cast<int>(floorf((at.z - m_navLow.z) / m_navCell));
        float best = 1e30f;
        bool found = false;
        for (int ring = 0; ring <= 8 && !found; ring++)
        {
            for (int z = cz - ring; z <= cz + ring; z++)
            {
                for (int x = cx - ring; x <= cx + ring; x++)
                {
                    if ((abs(x - cx) != ring && abs(z - cz) != ring) || x < 0 || z < 0 || x >= static_cast<int>(m_navSide) || z >= static_cast<int>(m_navSide))
                    {
                        continue;
                    }
                    const u32 i = static_cast<u32>(z) * m_navSide + static_cast<u32>(x);
                    if (!(m_navFlags[i] & kWalk))
                    {
                        continue;
                    }
                    const Vec3 d = navPlace(i) - at;
                    const float far = d.x * d.x + d.z * d.z + d.y * d.y * 4;
                    if (far < best)
                    {
                        best = far;
                        cell = i;
                        found = true;
                    }
                }
            }
        }
        return found;
    }

    bool Space::nearestWalkable(const Vec3& at, Vec3& out) const
    {
        u32 cell;
        if (!navNearest(at, cell))
        {
            return false;
        }
        out = navPlace(cell);
        return true;
    }

    // Whether one can go straight from one square to another, square by square along the line.
    bool Space::navStraight(u32 from, u32 to) const
    {
        int x = static_cast<int>(from % m_navSide), z = static_cast<int>(from / m_navSide);
        const int x1 = static_cast<int>(to % m_navSide), z1 = static_cast<int>(to / m_navSide);
        const int sx = x1 > x ? 1 : -1, sz = z1 > z ? 1 : -1;
        const float dx = fabsf(static_cast<float>(x1 - x)), dz = fabsf(static_cast<float>(z1 - z));
        // From the middle of one square to the middle of the other, through each square the line crosses.
        float nextX = dx > 0 ? 0.5f / dx : 1e30f, nextZ = dz > 0 ? 0.5f / dz : 1e30f;
        const float stepX = dx > 0 ? 1 / dx : 1e30f, stepZ = dz > 0 ? 1 / dz : 1e30f;
        u32 at = from;
        const auto go = [&](int nx, int nz) {
            const u32 next = static_cast<u32>(nz) * m_navSide + static_cast<u32>(nx);
            if (!navStep(at, next))
            {
                return false;
            }
            at = next;
            return true;
        };
        while (x != x1 || z != z1)
        {
            if (fabsf(nextX - nextZ) < 1e-5f)
            {
                // Through a corner: both squares beside it must be open.
                const u32 here = at;
                if (!go(x + sx, z))
                {
                    return false;
                }
                at = here;
                if (!go(x, z + sz))
                {
                    return false;
                }
                at = here;
                x += sx;
                z += sz;
                nextX += stepX;
                nextZ += stepZ;
                if (!go(x, z))
                {
                    return false;
                }
            }
            else if (nextX < nextZ)
            {
                x += sx;
                nextX += stepX;
                if (!go(x, z))
                {
                    return false;
                }
            }
            else
            {
                z += sz;
                nextZ += stepZ;
                if (!go(x, z))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool Space::findPath(const Vec3& from, const Vec3& to, GrowArray<Vec3>& out)
    {
        out.clear();
        u32 start, goal;
        if (!navNearest(from, start) || !navNearest(to, goal))
        {
            return false;
        }
        const u32 side = m_navSide;
        // A* over the squares, eight ways round, with no cutting of corners. A number for this search marks the
        // squares it has seen, so nothing needs clearing.
        if (++m_navVisit == 0)
        {
            memset(m_navSeen.begin(), 0, m_navSeen.count() * sizeof(u32));
            m_navVisit = 1;
        }
        const u32 visit = m_navVisit;
        const auto guess = [&](u32 c) {
            const float dx = fabsf(static_cast<float>(c % side) - static_cast<float>(goal % side)), dz = fabsf(static_cast<float>(c / side) - static_cast<float>(goal / side));
            return (dx + dz) + (1.4142f - 2) * fminf(dx, dz);
        };
        GrowArray<Open> heap;
        const auto push = [&](float f, u32 c) {
            if (!heap.push({ f, c }))
            {
                return;
            }
            u32 i = heap.count() - 1;
            while (i > 0 && heap[(i - 1) / 2].f > heap[i].f)
            {
                const Open t = heap[i];
                heap[i] = heap[(i - 1) / 2];
                heap[(i - 1) / 2] = t;
                i = (i - 1) / 2;
            }
        };
        const auto pop = [&]() {
            const Open top = heap[0];
            heap[0] = heap[heap.count() - 1];
            heap.resize(heap.count() - 1);
            u32 i = 0;
            for (;;)
            {
                const u32 l = i * 2 + 1, r = l + 1;
                u32 least = i;
                least = l < heap.count() && heap[l].f < heap[least].f ? l : least;
                least = r < heap.count() && heap[r].f < heap[least].f ? r : least;
                if (least == i)
                {
                    break;
                }
                const Open t = heap[i];
                heap[i] = heap[least];
                heap[least] = t;
                i = least;
            }
            return top;
        };
        m_navSeen[start] = visit;
        m_navCost[start] = 0;
        m_navFrom[start] = start;
        push(guess(start), start);
        u32 closest = start;
        float closestGuess = guess(start);
        bool reached = false;
        u32 steps = 0;
        while (heap.count() && steps++ < side * side * 2)
        {
            const Open o = pop();
            if (o.cell == goal)
            {
                reached = true;
                break;
            }
            const float g = m_navCost[o.cell];
            if (o.f - guess(o.cell) > g + 1e-3f)
            {
                continue;   // a worse way to it, seen before a better one
            }
            const int cx = static_cast<int>(o.cell % side), cz = static_cast<int>(o.cell / side);
            for (int dz = -1; dz <= 1; dz++)
            {
                for (int dx = -1; dx <= 1; dx++)
                {
                    const int nx = cx + dx, nz = cz + dz;
                    if ((dx == 0 && dz == 0) || nx < 0 || nz < 0 || nx >= static_cast<int>(side) || nz >= static_cast<int>(side))
                    {
                        continue;
                    }
                    const u32 next = static_cast<u32>(nz) * side + static_cast<u32>(nx);
                    if (!navStep(o.cell, next))
                    {
                        continue;
                    }
                    if (dx && dz && (!navStep(o.cell, static_cast<u32>(cz) * side + static_cast<u32>(nx)) || !navStep(o.cell, static_cast<u32>(nz) * side + static_cast<u32>(cx))))
                    {
                        continue;
                    }
                    const float cost = g + (dx && dz ? 1.4142f : 1.0f) * ((m_navFlags[next] & kEdge) ? 3.0f : 1.0f) + fabsf(m_navHeight[next] - m_navHeight[o.cell]) * 0.5f;
                    if (m_navSeen[next] != visit || cost < m_navCost[next])
                    {
                        m_navSeen[next] = visit;
                        m_navCost[next] = cost;
                        m_navFrom[next] = o.cell;
                        const float h = guess(next);
                        push(cost + h, next);
                        if (h < closestGuess)
                        {
                            closestGuess = h;
                            closest = next;
                        }
                    }
                }
            }
        }
        // Back from the end, then straightened: each point the furthest that can be gone to straight from the last.
        GrowArray<u32> cells;
        for (u32 c = reached ? goal : closest;; c = m_navFrom[c])
        {
            cells.push(c);
            if (c == start)
            {
                break;
            }
        }
        u32 at = cells.count() - 1;
        out.push(from);
        while (at > 0)
        {
            u32 far = at - 1;
            for (u32 k = 0; k < at; k++)
            {
                if (navStraight(cells[at], cells[k]))
                {
                    far = k;
                    break;
                }
            }
            out.push(far == 0 && reached ? Vec3(to.x, m_navHeight[goal], to.z) : navPlace(cells[far]));
            at = far;
        }
        if (out.count() == 1)
        {
            out.push(reached ? Vec3(to.x, m_navHeight[goal], to.z) : navPlace(start));
        }
        return reached;
    }
}
