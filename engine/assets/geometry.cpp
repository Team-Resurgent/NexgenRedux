#include "assets/geometry.h"

#include <math.h>

namespace nx
{
    namespace
    {
        constexpr float kTurn = 6.28318531f;

        u8 toByte(float v)
        {
            return v <= 0 ? 0 : v >= 1 ? 255 : static_cast<u8>(v * 255 + 0.5f);
        }
    }

    gpu::MeshVertex meshVertex(const Vec3& p, const Vec3& n, const gpu::Color& c, float u, float v)
    {
        return { p.x, p.y, p.z, n.x, n.y, n.z, toByte(c.r), toByte(c.g), toByte(c.b), toByte(c.a), u, v };
    }

    bool MeshData::vertex(const Vec3& place, const Vec3& normal, const gpu::Color& color, float u, float v)
    {
        return vertices.count() < 65536 && vertices.push(meshVertex(place, normal, color, u, v));
    }

    bool MeshData::triangle(u32 a, u32 b, u32 c)
    {
        const u32 count = vertices.count();
        return a < count && b < count && c < count && indices.push(static_cast<u16>(a)) && indices.push(static_cast<u16>(b)) && indices.push(static_cast<u16>(c));
    }

    bool MeshData::quad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const gpu::Color& color)
    {
        const u32 first = vertices.count();
        if (first + 4 > 65536)
        {
            return false;
        }
        const Vec3 normal = (b - a).cross(c - a).normalized();
        return vertex(a, normal, color, 0, 1) && vertex(b, normal, color, 1, 1) && vertex(c, normal, color, 1, 0) && vertex(d, normal, color, 0, 0) &&
               triangle(first, first + 1, first + 2) && triangle(first, first + 2, first + 3);
    }

    void MeshData::box(const Vec3& m, const Vec3& h, const gpu::Color& color)
    {
        const auto at = [&](float x, float y, float z) { return Vec3(m.x + x * h.x, m.y + y * h.y, m.z + z * h.z); };
        quad(at(-1, -1, 1), at(1, -1, 1), at(1, 1, 1), at(-1, 1, 1), color);
        quad(at(1, -1, -1), at(-1, -1, -1), at(-1, 1, -1), at(1, 1, -1), color);
        quad(at(1, -1, 1), at(1, -1, -1), at(1, 1, -1), at(1, 1, 1), color);
        quad(at(-1, -1, -1), at(-1, -1, 1), at(-1, 1, 1), at(-1, 1, -1), color);
        quad(at(-1, 1, 1), at(1, 1, 1), at(1, 1, -1), at(-1, 1, -1), color);
        quad(at(-1, -1, -1), at(1, -1, -1), at(1, -1, 1), at(-1, -1, 1), color);
    }

    void MeshData::sphere(const Vec3& m, float radius, u32 around, const gpu::Color& color)
    {
        const u32 rows = around / 2 < 2 ? 2 : around / 2;
        const u32 first = vertices.count();
        for (u32 y = 0; y <= rows; y++)
        {
            const float down = 3.14159265f * static_cast<float>(y) / static_cast<float>(rows);
            for (u32 x = 0; x <= around; x++)
            {
                const float angle = kTurn * static_cast<float>(x) / static_cast<float>(around);
                const Vec3 n(sinf(down) * cosf(angle), cosf(down), -sinf(down) * sinf(angle));
                vertex(m + n * radius, n, color, static_cast<float>(x) / static_cast<float>(around), static_cast<float>(y) / static_cast<float>(rows));
            }
        }
        for (u32 y = 0; y < rows; y++)
        {
            for (u32 x = 0; x < around; x++)
            {
                const u32 a = first + y * (around + 1) + x, b = a + around + 1;
                if (y)
                {
                    triangle(a, b, a + 1);
                }
                if (y + 1 < rows)
                {
                    triangle(a + 1, b, b + 1);
                }
            }
        }
    }

    void MeshData::cylinder(const Vec3& bottom, float height, float r0, float r1, u32 around, const gpu::Color& color)
    {
        const u32 first = vertices.count();
        const float slope = height != 0 ? (r0 - r1) / height : 0;
        for (u32 x = 0; x <= around; x++)
        {
            const float angle = kTurn * static_cast<float>(x) / static_cast<float>(around);
            const Vec3 out(cosf(angle), 0, -sinf(angle));
            const Vec3 normal = Vec3(out.x, slope, out.z).normalized();
            const float u = static_cast<float>(x) / static_cast<float>(around);
            vertex(bottom + out * r0, normal, color, u, 1);
            vertex(bottom + out * r1 + Vec3(0, height, 0), normal, color, u, 0);
        }
        for (u32 x = 0; x < around; x++)
        {
            const u32 a = first + x * 2;
            triangle(a, a + 2, a + 3);
            triangle(a, a + 3, a + 1);
        }
        for (u32 end = 0; end < 2; end++)
        {
            const float radius = end ? r1 : r0;
            if (radius <= 0)
            {
                continue;
            }
            const Vec3 normal(0, end ? 1.0f : -1.0f, 0);
            const Vec3 middle = end ? bottom + Vec3(0, height, 0) : bottom;
            const u32 hub = vertices.count();
            vertex(middle, normal, color, 0.5f, 0.5f);
            for (u32 x = 0; x <= around; x++)
            {
                const float angle = kTurn * static_cast<float>(x) / static_cast<float>(around);
                vertex(middle + Vec3(cosf(angle), 0, -sinf(angle)) * radius, normal, color, 0.5f + cosf(angle) * 0.5f, 0.5f + sinf(angle) * 0.5f);
            }
            for (u32 x = 0; x < around; x++)
            {
                end ? triangle(hub, hub + 1 + x, hub + 2 + x) : triangle(hub, hub + 2 + x, hub + 1 + x);
            }
        }
    }

    void MeshData::capsule(float radius, float halfHeight, u32 around, const gpu::Color& color)
    {
        const u32 first = vertices.count();
        sphere({}, radius, around, color);
        // Pulled apart at the middle.
        for (u32 i = first; i < vertices.count(); i++)
        {
            vertices[i].y += vertices[i].ny > 0.001f ? halfHeight : vertices[i].ny < -0.001f ? -halfHeight : 0;
        }
        // The row in the middle becomes the top of the sides, with another below it.
        const u32 rows = around / 2 < 2 ? 2 : around / 2;
        if (rows % 2 == 0)
        {
            const u32 middle = first + rows / 2 * (around + 1);
            const u32 below = vertices.count();
            for (u32 x = 0; x <= around; x++)
            {
                gpu::MeshVertex v = vertices[middle + x];
                vertices[middle + x].y = halfHeight;
                v.y = -halfHeight;
                vertices.push(v);
            }
            for (u32 i = 0; i < indices.count(); i++)
            {
                // Triangles of the lower half use the lower row.
                const u32 t = i / 3 * 3;
                const u32 lowest = first + (rows / 2 + 1) * (around + 1);
                const bool lower = indices[t] >= lowest || indices[t + 1] >= lowest || indices[t + 2] >= lowest;
                if (lower && indices[i] >= middle && indices[i] <= middle + around && indices[t] >= first)
                {
                    indices[i] = static_cast<u16>(below + indices[i] - middle);
                }
            }
            for (u32 x = 0; x < around; x++)
            {
                triangle(middle + x, below + x, middle + x + 1);
                triangle(middle + x + 1, below + x, below + x + 1);
            }
        }
    }

    void MeshData::torus(float ring, float tube, u32 around, const gpu::Color& color)
    {
        const u32 across = around / 2 < 3 ? 3 : around / 2;
        const u32 first = vertices.count();
        for (u32 i = 0; i <= around; i++)
        {
            const float a = kTurn * static_cast<float>(i) / static_cast<float>(around);
            for (u32 k = 0; k <= across; k++)
            {
                const float b = kTurn * static_cast<float>(k) / static_cast<float>(across);
                const Vec3 n(cosf(b) * cosf(a), sinf(b), -cosf(b) * sinf(a));
                vertex({ cosf(a) * (ring + tube * cosf(b)), tube * sinf(b), -sinf(a) * (ring + tube * cosf(b)) }, n, color,
                    static_cast<float>(i) / static_cast<float>(around), static_cast<float>(k) / static_cast<float>(across));
            }
        }
        for (u32 i = 0; i < around; i++)
        {
            for (u32 k = 0; k < across; k++)
            {
                const u32 a = first + i * (across + 1) + k, b = a + across + 1;
                triangle(a, b, b + 1);
                triangle(a, b + 1, a + 1);
            }
        }
    }

    void MeshData::plane(float w, float d, u32 squares, const gpu::Color& color)
    {
        const u32 first = vertices.count();
        squares = squares < 1 ? 1 : squares > 200 ? 200 : squares;
        for (u32 z = 0; z <= squares; z++)
        {
            for (u32 x = 0; x <= squares; x++)
            {
                const float u = static_cast<float>(x) / static_cast<float>(squares), v = static_cast<float>(z) / static_cast<float>(squares);
                vertex({ (u * 2 - 1) * w, 0, (v * 2 - 1) * d }, { 0, 1, 0 }, color, u, v);
            }
        }
        for (u32 z = 0; z < squares; z++)
        {
            for (u32 x = 0; x < squares; x++)
            {
                const u32 a = first + z * (squares + 1) + x, b = a + squares + 1;
                triangle(a, b, b + 1);
                triangle(a, b + 1, a + 1);
            }
        }
    }

    void MeshData::transform(u32 first, const Mat4& by)
    {
        for (u32 i = first; i < vertices.count(); i++)
        {
            gpu::MeshVertex& v = vertices[i];
            const Vec3 p = by.transformPoint({ v.x, v.y, v.z });
            const Vec3 n = by.transformDirection({ v.nx, v.ny, v.nz }).normalized();
            v.x = p.x;
            v.y = p.y;
            v.z = p.z;
            v.nx = n.x;
            v.ny = n.y;
            v.nz = n.z;
        }
    }

    void MeshData::computeNormals()
    {
        for (gpu::MeshVertex& v : vertices)
        {
            v.nx = v.ny = v.nz = 0;
        }
        for (u32 i = 0; i + 2 < indices.count(); i += 3)
        {
            gpu::MeshVertex* v[3] = { &vertices[indices[i]], &vertices[indices[i + 1]], &vertices[indices[i + 2]] };
            const Vec3 a(v[0]->x, v[0]->y, v[0]->z), b(v[1]->x, v[1]->y, v[1]->z), c(v[2]->x, v[2]->y, v[2]->z);
            const Vec3 n = (b - a).cross(c - a);  // larger triangles count for more
            for (gpu::MeshVertex* p : v)
            {
                p->nx += n.x;
                p->ny += n.y;
                p->nz += n.z;
            }
        }
        for (gpu::MeshVertex& v : vertices)
        {
            const Vec3 n = Vec3(v.nx, v.ny, v.nz).normalized();
            v.nx = n.x;
            v.ny = n.y;
            v.nz = n.z;
        }
    }

    void MeshData::bounds(Vec3& middle, float& radius) const
    {
        Vec3 low(1e30f, 1e30f, 1e30f), high(-1e30f, -1e30f, -1e30f);
        for (const gpu::MeshVertex& v : vertices)
        {
            low = { fminf(low.x, v.x), fminf(low.y, v.y), fminf(low.z, v.z) };
            high = { fmaxf(high.x, v.x), fmaxf(high.y, v.y), fmaxf(high.z, v.z) };
        }
        middle = vertices.count() ? (low + high) * 0.5f : Vec3();
        float most = 0;
        for (const gpu::MeshVertex& v : vertices)
        {
            const Vec3 d = Vec3(v.x, v.y, v.z) - middle;
            most = fmaxf(most, d.dot(d));
        }
        radius = sqrtf(most);
    }

    void simplify(const gpu::MeshVertex* in, u32 vertexCount, const u16* indices, u32 indexCount, u32 cells, MeshData& out)
    {
        out.clear();
        Vec3 low(1e30f, 1e30f, 1e30f), high(-1e30f, -1e30f, -1e30f);
        for (u32 i = 0; i < vertexCount; i++)
        {
            low = { fminf(low.x, in[i].x), fminf(low.y, in[i].y), fminf(low.z, in[i].z) };
            high = { fmaxf(high.x, in[i].x), fmaxf(high.y, in[i].y), fmaxf(high.z, in[i].z) };
        }
        const Vec3 size = high - low;
        const float longest = fmaxf(size.x, fmaxf(size.y, size.z));
        if (longest <= 0 || cells == 0 || vertexCount == 0)
        {
            return;
        }
        const float across = static_cast<float>(cells) / longest;

        // Sorted by cell in a table with room to spare; the first vertex found stands for its cell. Facing
        // counts too, so that a thin wall keeps both its sides.
        u32 slots = 64;
        while (slots < vertexCount * 2)
        {
            slots *= 2;
        }
        u64* keys = static_cast<u64*>(memAlloc(slots * sizeof(u64), MemTag::General));
        u16* place = static_cast<u16*>(memAlloc(slots * sizeof(u16), MemTag::General));
        u16* becomes = static_cast<u16*>(memAlloc(vertexCount * sizeof(u16), MemTag::General));
        struct Sum
        {
            Vec3 place, normal;
            float count;
        };
        Sum* sums = static_cast<Sum*>(memAlloc(vertexCount * sizeof(Sum), MemTag::General));
        if (keys && place && becomes && sums)
        {
            for (u32 i = 0; i < slots; i++)
            {
                keys[i] = ~0ull;
            }
            for (u32 i = 0; i < vertexCount; i++)
            {
                const gpu::MeshVertex& v = in[i];
                const u64 x = static_cast<u64>((v.x - low.x) * across), y = static_cast<u64>((v.y - low.y) * across), z = static_cast<u64>((v.z - low.z) * across);
                const float ax = fabsf(v.nx), ay = fabsf(v.ny), az = fabsf(v.nz);
                const u64 facing = ax > ay && ax > az ? (v.nx > 0 ? 0u : 1u) : ay > az ? (v.ny > 0 ? 2u : 3u) : (v.nz > 0 ? 4u : 5u);
                const u64 color = (static_cast<u64>(v.r >> 5) << 6) | (static_cast<u64>(v.g >> 5) << 3) | (v.b >> 5);
                const u64 key = x | y << 10 | z << 20 | facing << 30 | color << 33;
                u32 at = static_cast<u32>((key * 11400714819323198485ull) >> 40) & (slots - 1);
                while (keys[at] != ~0ull && keys[at] != key)
                {
                    at = (at + 1) & (slots - 1);
                }
                if (keys[at] == ~0ull)
                {
                    keys[at] = key;
                    place[at] = static_cast<u16>(out.vertices.count());
                    sums[out.vertices.count()] = {};
                    out.vertices.push(v);
                }
                becomes[i] = place[at];
                Sum& s = sums[place[at]];
                s.place = s.place + Vec3(v.x, v.y, v.z);
                s.normal = s.normal + Vec3(v.nx, v.ny, v.nz);
                s.count += 1;
            }
            for (u32 i = 0; i < out.vertices.count(); i++)
            {
                gpu::MeshVertex& v = out.vertices[i];
                const Vec3 p = sums[i].place / sums[i].count, n = sums[i].normal.normalized();
                v.x = p.x;
                v.y = p.y;
                v.z = p.z;
                v.nx = n.x;
                v.ny = n.y;
                v.nz = n.z;
            }
            for (u32 i = 0; i + 2 < indexCount; i += 3)
            {
                const u16 a = becomes[indices[i]], b = becomes[indices[i + 1]], c = becomes[indices[i + 2]];
                if (a != b && b != c && a != c)
                {
                    out.triangle(a, b, c);
                }
            }
        }
        memFree(keys);
        memFree(place);
        memFree(becomes);
        memFree(sums);
    }
}
