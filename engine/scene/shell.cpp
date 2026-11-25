#include "scene/space.h"
#include "scene/world.h"

#include <math.h>

// Colliders of the exact shape of a model: its triangles, from either side. They don't move by themselves.
namespace nx
{
    namespace
    {
        constexpr float kNear = 0.01f;

        Vec3 times(const Vec3& a, const Vec3& b)
        {
            return { a.x * b.x, a.y * b.y, a.z * b.z };
        }

        int clampInt(int v, int low, int high)
        {
            return v < low ? low : v > high ? high : v;
        }

        Vec3 alongSegment(const Vec3& a, const Vec3& b, const Vec3& p)
        {
            const Vec3 d = b - a;
            const float length = d.dot(d);
            const float t = length > 1e-9f ? (p - a).dot(d) / length : 0;
            return a + d * (t < 0 ? 0 : t > 1 ? 1 : t);
        }

        // The place on a triangle that is nearest.
        Vec3 onTriangle(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c)
        {
            const Vec3 ab = b - a, ac = c - a, ap = p - a;
            const float d1 = ab.dot(ap), d2 = ac.dot(ap);
            if (d1 <= 0 && d2 <= 0)
            {
                return a;
            }
            const Vec3 bp = p - b;
            const float d3 = ab.dot(bp), d4 = ac.dot(bp);
            if (d3 >= 0 && d4 <= d3)
            {
                return b;
            }
            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0 && d1 >= 0 && d3 <= 0)
            {
                return a + ab * (d1 / (d1 - d3));
            }
            const Vec3 cp = p - c;
            const float d5 = ab.dot(cp), d6 = ac.dot(cp);
            if (d6 >= 0 && d5 <= d6)
            {
                return c;
            }
            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0 && d2 >= 0 && d6 <= 0)
            {
                return a + ac * (d2 / (d2 - d6));
            }
            const float va = d3 * d6 - d5 * d4;
            if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0)
            {
                return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
            }
            const float sum = va + vb + vc;
            return sum > 1e-20f ? a + ab * (vb / sum) + ac * (vc / sum) : a;
        }
    }

    // --- Which triangles are where ---

    bool ModelData::makeCells() const
    {
        if (cellStarts.count())
        {
            return true;
        }
        u32 triangles = 0;
        for (const ModelPart& p : parts)
        {
            triangles += p.count[0] / 3;
        }
        const Vec3 span = high - low;
        const float longest = fmaxf(fmaxf(span.x, span.y), fmaxf(span.z, 0.001f));
        // About two triangles to a cell, if they were spread evenly.
        const float across = fminf(24, fmaxf(1, cbrtf(static_cast<float>(triangles)) * 1.3f));
        u32 cells = 1;
        const float spans[3] = { span.x, span.y, span.z };
        for (u32 i = 0; i < 3; i++)
        {
            cellCount[i] = static_cast<u32>(clampInt(static_cast<int>(spans[i] / longest * across + 0.5f), 1, 24));
            cellSize[i] = fmaxf(spans[i], 0.001f) / static_cast<float>(cellCount[i]);
            cells *= cellCount[i];
        }
        if (triangles > 60000 || !cellStarts.reserve(cells + 1))
        {
            return false;
        }
        cellStarts.resize(cells + 1);
        for (u32& s : cellStarts)
        {
            s = 0;
        }
        const auto each = [&](const auto& visit)
        {
            for (u32 t = 0; t < triangles; t++)
            {
                Vec3 least(1e9f, 1e9f, 1e9f), most(-1e9f, -1e9f, -1e9f);
                for (u32 k = 0; k < 3; k++)
                {
                    const gpu::MeshVertex& v = mesh.vertices[mesh.indices[t * 3 + k]];
                    least = { fminf(least.x, v.x), fminf(least.y, v.y), fminf(least.z, v.z) };
                    most = { fmaxf(most.x, v.x), fmaxf(most.y, v.y), fmaxf(most.z, v.z) };
                }
                int from[3], to[3];
                cellsOf(least, most, from, to);
                for (int z = from[2]; z <= to[2]; z++)
                {
                    for (int y = from[1]; y <= to[1]; y++)
                    {
                        for (int x = from[0]; x <= to[0]; x++)
                        {
                            visit((static_cast<u32>(z) * cellCount[1] + static_cast<u32>(y)) * cellCount[0] + static_cast<u32>(x), t);
                        }
                    }
                }
            }
        };
        each([&](u32 cell, u32) { cellStarts[cell + 1]++; });
        for (u32 i = 0; i < cells; i++)
        {
            cellStarts[i + 1] += cellStarts[i];
        }
        const u32 total = cellStarts[cells];
        if (!cellTriangles.reserve(total ? total : 1))
        {
            cellStarts.clear();
            return false;
        }
        cellTriangles.resize(total);
        // Each is filled from its start, which then is the next one's: put right by moving them along.
        each([&](u32 cell, u32 t) { cellTriangles[cellStarts[cell]++] = static_cast<u16>(t); });
        for (u32 i = cells; i > 0; i--)
        {
            cellStarts[i] = cellStarts[i - 1];
        }
        cellStarts[0] = 0;
        return true;
    }

    void ModelData::cellsOf(const Vec3& least, const Vec3& most, int (&from)[3], int (&to)[3]) const
    {
        const float a[3] = { least.x - low.x, least.y - low.y, least.z - low.z }, b[3] = { most.x - low.x, most.y - low.y, most.z - low.z };
        for (u32 i = 0; i < 3; i++)
        {
            const float limit = static_cast<float>(cellCount[i]) + 1;
            from[i] = clampInt(static_cast<int>(floorf(fmaxf(-1, fminf(limit, a[i] / cellSize[i] - 0.001f)))), 0, static_cast<int>(cellCount[i]) - 1);
            to[i] = clampInt(static_cast<int>(floorf(fmaxf(-1, fminf(limit, b[i] / cellSize[i] + 0.001f)))), 0, static_cast<int>(cellCount[i]) - 1);
        }
    }

    // The triangles that may be within a box, in space.
    template <typename F>
    void Space::eachTriangle(const Rigid& shell, const Vec3& low, const Vec3& high, const F& visit)
    {
        const ModelData& m = *shell.model;
        if (!m.makeCells())
        {
            return;
        }
        // The box as the model would have it.
        const Vec3 middle = (low + high) * 0.5f - shell.place, half = (high - low) * 0.5f;
        float at[3], reach[3];
        const float sizes[3] = { shell.size.x, shell.size.y, shell.size.z };
        for (u32 i = 0; i < 3; i++)
        {
            const Vec3& x = shell.axes[i];
            at[i] = middle.dot(x) / sizes[i];
            reach[i] = (fabsf(x.x) * half.x + fabsf(x.y) * half.y + fabsf(x.z) * half.z) / sizes[i];
        }
        const Vec3 centre = (m.low + m.high) * 0.5f;
        const Vec3 least(centre.x + at[0] - reach[0], centre.y + at[1] - reach[1], centre.z + at[2] - reach[2]);
        const Vec3 most(centre.x + at[0] + reach[0], centre.y + at[1] + reach[1], centre.z + at[2] + reach[2]);
        if (least.x > m.high.x || least.y > m.high.y || least.z > m.high.z || most.x < m.low.x || most.y < m.low.y || most.z < m.low.z)
        {
            return;
        }
        int from[3], to[3];
        m.cellsOf(least, most, from, to);
        for (int z = from[2]; z <= to[2]; z++)
        {
            for (int y = from[1]; y <= to[1]; y++)
            {
                for (int x = from[0]; x <= to[0]; x++)
                {
                    const u32 cell = (static_cast<u32>(z) * m.cellCount[1] + static_cast<u32>(y)) * m.cellCount[0] + static_cast<u32>(x);
                    for (u32 i = m.cellStarts[cell]; i < m.cellStarts[cell + 1]; i++)
                    {
                        const u32 t = m.cellTriangles[i];
                        Vec3 p[3];
                        Vec3 tl(1e9f, 1e9f, 1e9f), th(-1e9f, -1e9f, -1e9f);
                        for (u32 k = 0; k < 3; k++)
                        {
                            const gpu::MeshVertex& v = m.mesh.vertices[m.mesh.indices[t * 3 + k]];
                            const Vec3 l = times(Vec3(v.x, v.y, v.z) - centre, shell.size);
                            p[k] = shell.place + shell.axes[0] * l.x + shell.axes[1] * l.y + shell.axes[2] * l.z;
                            tl = { fminf(tl.x, p[k].x), fminf(tl.y, p[k].y), fminf(tl.z, p[k].z) };
                            th = { fmaxf(th.x, p[k].x), fmaxf(th.y, p[k].y), fmaxf(th.z, p[k].z) };
                        }
                        if (tl.x <= high.x && tl.y <= high.y && tl.z <= high.z && th.x >= low.x && th.y >= low.y && th.z >= low.z)
                        {
                            visit(p);
                        }
                    }
                }
            }
        }
    }

    u32 Space::touchShell(const Rigid& a, const Rigid& shell, Contact (&contacts)[4])
    {
        if (shell.model == nullptr)
        {
            return 0;
        }
        struct Found
        {
            Contact contact;
            Vec3 at, normal;   // where on the triangle, and the triangle's own
            bool face;
        };
        constexpr u32 kMost = 24;
        Found found[kMost];
        u32 count = 0;
        const auto add = [&](const Found& f)
        {
            if (count < kMost)
            {
                found[count++] = f;
                return;
            }
            u32 least = 0;
            for (u32 i = 1; i < kMost; i++)
            {
                least = found[i].contact.depth < found[least].contact.depth ? i : least;
            }
            if (found[least].contact.depth < f.contact.depth)
            {
                found[least] = f;
            }
        };
        const Vec3 margin(kNear, kNear, kNear);
        eachTriangle(shell, a.low - margin, a.high + margin, [&](const Vec3 (&p)[3])
        {
            Vec3 n = (p[1] - p[0]).cross(p[2] - p[0]);
            const float area = n.length();
            if (area < 1e-10f)
            {
                return;
            }
            n = n / area;
            if (a.shape == Shape3::Box)
            {
                // Its corners through the triangle, from the side that its middle is on; and the triangle's corners in it.
                const float side = (a.place - p[0]).dot(n) < 0 ? -1.0f : 1.0f;
                const Vec3 out = n * side;
                const float far = fabsf((a.place - p[0]).dot(n));
                for (u32 i = 0; i < 8; i++)
                {
                    const Vec3 corner = a.place + a.axes[0] * (i & 1 ? a.half.x : -a.half.x) + a.axes[1] * (i & 2 ? a.half.y : -a.half.y) + a.axes[2] * (i & 4 ? a.half.z : -a.half.z);
                    const float above = (corner - p[0]).dot(out);
                    if (above > kNear || above < -fmaxf(0.3f, far))
                    {
                        continue;
                    }
                    const Vec3 flat = corner - out * above;
                    const Vec3 off = onTriangle(flat, p[0], p[1], p[2]) - flat;
                    if (off.dot(off) > 1e-6f)
                    {
                        continue;
                    }
                    Found f = {};
                    f.contact.place = flat;
                    f.contact.normal = -out;
                    f.contact.depth = -above;
                    f.at = flat;
                    f.normal = out;
                    f.face = true;
                    add(f);
                }
                for (const Vec3& v : p)
                {
                    const Vec3 d = v - a.place;
                    Contact c;
                    if (fabsf(d.dot(a.axes[0])) < a.half.x && fabsf(d.dot(a.axes[1])) < a.half.y && fabsf(d.dot(a.axes[2])) < a.half.z && touchBall(v, 0, a, c))
                    {
                        Found f = {};
                        f.contact = c;
                        // Out of the box is into the shell: from a to it.
                        f.contact.normal = -c.normal;
                        f.at = v;
                        f.normal = out;
                        f.face = false;
                        add(f);
                    }
                }
                return;
            }
            // A ball, or the place along a capsule that comes nearest.
            const float radius = a.half.x;
            const Vec3 reach = a.shape == Shape3::Capsule ? a.axes[1] * (a.half.y - a.half.x) : Vec3();
            Vec3 at = a.place, near = onTriangle(at, p[0], p[1], p[2]);
            if (a.shape == Shape3::Capsule)
            {
                for (u32 i = 0; i < 5; i++)
                {
                    at = alongSegment(a.place - reach, a.place + reach, near);
                    near = onTriangle(at, p[0], p[1], p[2]);
                }
            }
            const Vec3 d = at - near;
            const float far = d.length();
            if (far > radius + kNear)
            {
                return;
            }
            const Vec3 side = (a.place - p[0]).dot(n) < 0 ? -n : n;
            const Vec3 out = far > 1e-4f ? d / far : side;
            Found f = {};
            f.contact.depth = radius - far;
            f.contact.place = near;
            f.contact.normal = -out;
            f.at = near;
            f.normal = side;
            f.face = out.dot(side) > 0.999f;
            f.contact.edge = !f.face && side.y > 0.65f;
            add(f);
        });

        // Edges between two triangles that are level with each other aren't there.
        for (u32 i = 0; i < count; i++)
        {
            for (u32 k = 0; k < count && !found[i].face; k++)
            {
                if (found[k].face && fabsf((found[i].at - found[k].at).dot(found[k].normal)) < 0.012f && found[k].contact.depth >= found[i].contact.depth - 0.001f)
                {
                    found[i].contact.depth = -1;
                }
                // Of two that share an edge, one may be stood on.
                const Vec3 gap = found[i].at - found[k].at;
                if (!found[k].face && gap.dot(gap) < 1e-4f)
                {
                    found[i].contact.edge = found[i].contact.edge || found[k].contact.edge;
                }
            }
        }
        u32 taken = 0;
        while (taken < 4)
        {
            u32 pick = kMost;
            float score = -0.5f;
            for (u32 i = 0; i < count; i++)
            {
                const Contact& c = found[i].contact;
                if (c.depth < -kNear)
                {
                    continue;
                }
                // The deepest, and then those far from the rest.
                float s = c.depth + 1;
                for (u32 k = 0; k < taken; k++)
                {
                    const Vec3 gap = c.place - contacts[k].place;
                    const float apart = gap.dot(gap) + (1 - c.normal.dot(contacts[k].normal)) * 0.5f;
                    s = apart < 1e-4f ? -1 : fminf(s, apart);
                }
                if (s > score)
                {
                    score = s;
                    pick = i;
                }
            }
            if (pick == kMost || (taken && score < 1e-4f))
            {
                break;
            }
            contacts[taken++] = found[pick].contact;
            found[pick].contact.depth = -1;
        }
        return taken;
    }

    // How far along a line, or 2.
    float Space::rayShell(const Rigid& shell, const Vec3& from, const Vec3& way, float most, Vec3& normal)
    {
        float best = 2;
        if (shell.model == nullptr)
        {
            return best;
        }
        // In pieces, so that only what is near the line is looked at.
        const float length = way.length();
        const float piece = fmaxf(2, fmaxf(shell.half.x, fmaxf(shell.half.y, shell.half.z)) * 0.25f);
        const u32 pieces = length / piece < 200 ? static_cast<u32>(length / piece) + 1 : 200;
        for (u32 i = 0; i < pieces && best > 1; i++)
        {
            const float t0 = static_cast<float>(i) / static_cast<float>(pieces), t1 = static_cast<float>(i + 1) / static_cast<float>(pieces);
            if (t0 > most)
            {
                break;
            }
            const Vec3 p = from + way * t0, q = from + way * t1;
            const Vec3 low(fminf(p.x, q.x), fminf(p.y, q.y), fminf(p.z, q.z)), high(fmaxf(p.x, q.x), fmaxf(p.y, q.y), fmaxf(p.z, q.z));
            if (low.x > shell.high.x || low.y > shell.high.y || low.z > shell.high.z || high.x < shell.low.x || high.y < shell.low.y || high.z < shell.low.z)
            {
                continue;
            }
            eachTriangle(shell, low, high, [&](const Vec3 (&v)[3])
            {
                const Vec3 ab = v[1] - v[0], ac = v[2] - v[0];
                const Vec3 n = ab.cross(ac);
                const float into = n.dot(way);
                if (fabsf(into) < 1e-12f)
                {
                    return;
                }
                const float t = n.dot(v[0] - from) / into;
                if (t < 0 || t > 1 || t >= best)
                {
                    return;
                }
                const Vec3 at = from + way * t - v[0];
                const float d00 = ab.dot(ab), d01 = ab.dot(ac), d11 = ac.dot(ac), d20 = at.dot(ab), d21 = at.dot(ac), whole = d00 * d11 - d01 * d01;
                if (whole < 1e-20f)
                {
                    return;
                }
                const float y = (d11 * d20 - d01 * d21) / whole, z = (d00 * d21 - d01 * d20) / whole;
                if (y >= -1e-4f && z >= -1e-4f && y + z <= 1.0002f)
                {
                    best = t;
                    normal = (into > 0 ? -n : n).normalized();
                }
            });
        }
        return best;
    }
}
