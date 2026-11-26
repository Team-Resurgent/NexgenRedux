#include "scene/space.h"

#include <math.h>
#include <string.h>

// A world of blocks, in chunks of 16 on each side that are only kept where there is something. Each chunk is made
// into triangles of only the faces that can be seen, side by side ones of the same kind joined into one, and the
// corners darker where blocks crowd them. Things stand on them, bump into them and see them along a line, as they
// do the land.
namespace nx
{
    namespace
    {
        constexpr int kSide = 16;
        constexpr int kPad = kSide + 2;
        constexpr u32 kMostChunks = 64 * 16 * 64;
        constexpr float kDarker[4] = { 0.52f, 0.7f, 0.85f, 1.0f };

        float clampFloat(float v, float low, float high) { return v < low ? low : v > high ? high : v; }

        float hashed(int x, int z, u32 seed)
        {
            u32 h = static_cast<u32>(x) * 73856093u ^ static_cast<u32>(z) * 19349663u ^ seed * 2654435761u;
            h ^= h >> 13;
            h *= 0x5bd1e995u;
            h ^= h >> 15;
            return static_cast<float>(h & 0xFFFF) / 65535.0f;
        }

        float hashed3(int x, int y, int z, u32 seed) { return hashed(x * 31 + y * 7919, z ^ (y * 104729), seed); }

        float smooth2(float x, float z, u32 seed)
        {
            const int ix = static_cast<int>(floorf(x)), iz = static_cast<int>(floorf(z));
            float fx = x - static_cast<float>(ix), fz = z - static_cast<float>(iz);
            fx = fx * fx * (3 - 2 * fx);
            fz = fz * fz * (3 - 2 * fz);
            const float a = hashed(ix, iz, seed), b = hashed(ix + 1, iz, seed), c = hashed(ix, iz + 1, seed), d = hashed(ix + 1, iz + 1, seed);
            return a + (b - a) * fx + (c - a) * fz + (a - b - c + d) * fx * fz;
        }

        float smooth3(float x, float y, float z, u32 seed)
        {
            const int ix = static_cast<int>(floorf(x)), iy = static_cast<int>(floorf(y)), iz = static_cast<int>(floorf(z));
            float fx = x - static_cast<float>(ix), fy = y - static_cast<float>(iy), fz = z - static_cast<float>(iz);
            fx = fx * fx * (3 - 2 * fx);
            fy = fy * fy * (3 - 2 * fy);
            fz = fz * fz * (3 - 2 * fz);
            float layer[2];
            for (int k = 0; k < 2; k++)
            {
                const float a = hashed3(ix, iy + k, iz, seed), b = hashed3(ix + 1, iy + k, iz, seed);
                const float c = hashed3(ix, iy + k, iz + 1, seed), d = hashed3(ix + 1, iy + k, iz + 1, seed);
                layer[k] = a + (b - a) * fx + (c - a) * fz + (a - b - c + d) * fx * fz;
            }
            return layer[0] + (layer[1] - layer[0]) * fy;
        }
    }

    struct Space::VoxelChunk
    {
        u8 cells[kSide * kSide * kSide];
        gpu::Mesh mesh;
        u32 indices;
        u32 solids;
        bool changed;
    };

    void Space::defaultVoxelColors()
    {
        const gpu::Color kinds[][2] = {
            { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } },
            { { 0.36f, 0.62f, 0.26f, 1 }, { 0.47f, 0.36f, 0.24f, 1 } },   // 1 grass
            { { 0.5f, 0.38f, 0.26f, 1 }, { 0.47f, 0.36f, 0.24f, 1 } },    // 2 earth
            { { 0.55f, 0.55f, 0.56f, 1 }, { 0.5f, 0.5f, 0.52f, 1 } },     // 3 stone
            { { 0.86f, 0.8f, 0.58f, 1 }, { 0.82f, 0.75f, 0.54f, 1 } },    // 4 sand
            { { 0.62f, 0.5f, 0.33f, 1 }, { 0.4f, 0.29f, 0.18f, 1 } },     // 5 wood
            { { 0.24f, 0.47f, 0.2f, 1 }, { 0.22f, 0.43f, 0.18f, 1 } },    // 6 leaves
            { { 0.66f, 0.33f, 0.26f, 1 }, { 0.62f, 0.3f, 0.24f, 1 } },    // 7 brick
            { { 0.94f, 0.95f, 0.97f, 1 }, { 0.86f, 0.88f, 0.92f, 1 } },   // 8 snow
            { { 0.76f, 0.62f, 0.42f, 1 }, { 0.72f, 0.58f, 0.39f, 1 } },   // 9 planks
            { { 0.25f, 0.25f, 0.28f, 1 }, { 0.22f, 0.22f, 0.25f, 1 } },   // 10 dark stone
            { { 0.95f, 0.78f, 0.3f, 1 }, { 0.92f, 0.72f, 0.26f, 1 } },    // 11 gold
            { { 0.62f, 0.82f, 0.9f, 1 }, { 0.56f, 0.76f, 0.86f, 1 } },    // 12 ice
        };
        for (u32 i = 0; i < 256; i++)
        {
            const u32 k = i < sizeof(kinds) / sizeof(kinds[0]) ? i : 3;
            m_voxelTop[i] = kinds[k][0];
            m_voxelSide[i] = kinds[k][1];
        }
    }

    bool Space::makeVoxels(const Vec3& origin, u32 sizeX, u32 sizeY, u32 sizeZ, float cell)
    {
        clearVoxels();
        const u32 cx = (sizeX + kSide - 1) / kSide, cy = (sizeY + kSide - 1) / kSide, cz = (sizeZ + kSide - 1) / kSide;
        if (sizeX == 0 || sizeY == 0 || sizeZ == 0 || cx * cy * cz > kMostChunks || cx > 64 || cz > 64 || cy > 16 || !m_voxelChunks.reserve(cx * cy * cz))
        {
            return false;
        }
        m_voxelChunks.resize(cx * cy * cz);
        for (VoxelChunk*& c : m_voxelChunks)
        {
            c = nullptr;
        }
        m_voxelOrigin = origin;
        m_voxelSize[0] = static_cast<int>(sizeX);
        m_voxelSize[1] = static_cast<int>(sizeY);
        m_voxelSize[2] = static_cast<int>(sizeZ);
        m_voxelChunkSide[0] = cx;
        m_voxelChunkSide[1] = cy;
        m_voxelChunkSide[2] = cz;
        m_voxelCell = cell < 0.05f ? 0.05f : cell;
        if (!m_voxelColorsSet)
        {
            defaultVoxelColors();
            m_voxelColorsSet = true;
        }
        m_landChanges++;
        return true;
    }

    void Space::clearVoxels()
    {
        for (VoxelChunk*& c : m_voxelChunks)
        {
            if (c)
            {
                if (m_solids)
                {
                    m_solids->device()->destroyMesh(c->mesh);
                }
                destroy(c);
                c = nullptr;
            }
        }
        m_voxelChunks.clear();
        m_voxelSize[0] = m_voxelSize[1] = m_voxelSize[2] = 0;
        m_landChanges++;
    }

    void Space::setVoxelColor(u32 type, const gpu::Color& top, const gpu::Color& side)
    {
        if (!m_voxelColorsSet)
        {
            defaultVoxelColors();
            m_voxelColorsSet = true;
        }
        if (type > 0 && type < 256)
        {
            m_voxelTop[type] = top;
            m_voxelSide[type] = side;
            for (VoxelChunk* c : m_voxelChunks)
            {
                if (c)
                {
                    c->changed = true;
                }
            }
        }
    }

    u8 Space::voxel(int x, int y, int z) const
    {
        if (x < 0 || y < 0 || z < 0 || x >= m_voxelSize[0] || y >= m_voxelSize[1] || z >= m_voxelSize[2])
        {
            return 0;
        }
        const VoxelChunk* c = m_voxelChunks[(static_cast<u32>(z / kSide) * m_voxelChunkSide[1] + static_cast<u32>(y / kSide)) * m_voxelChunkSide[0] + static_cast<u32>(x / kSide)];
        return c ? c->cells[((z % kSide) * kSide + (y % kSide)) * kSide + (x % kSide)] : 0;
    }

    bool Space::putVoxel(int x, int y, int z, u8 type)
    {
        if (x < 0 || y < 0 || z < 0 || x >= m_voxelSize[0] || y >= m_voxelSize[1] || z >= m_voxelSize[2])
        {
            return false;
        }
        VoxelChunk*& c = m_voxelChunks[(static_cast<u32>(z / kSide) * m_voxelChunkSide[1] + static_cast<u32>(y / kSide)) * m_voxelChunkSide[0] + static_cast<u32>(x / kSide)];
        if (c == nullptr)
        {
            if (type == 0)
            {
                return false;
            }
            c = create<VoxelChunk>(MemTag::Scene);
            if (c == nullptr)
            {
                return false;
            }
            memset(c->cells, 0, sizeof(c->cells));
            c->mesh = {};
            c->indices = c->solids = 0;
            c->changed = true;
        }
        u8& cell = c->cells[((z % kSide) * kSide + (y % kSide)) * kSide + (x % kSide)];
        if (cell == type)
        {
            return false;
        }
        c->solids += (type != 0) - (cell != 0);
        cell = type;
        c->changed = true;
        return true;
    }

    // A change at the side of a chunk changes what can be seen of the one beside it too.
    void Space::voxelsChanged(int fromX, int fromY, int fromZ, int toX, int toY, int toZ)
    {
        const int low[3] = { fromX - 1, fromY - 1, fromZ - 1 }, high[3] = { toX + 1, toY + 1, toZ + 1 };
        int lowChunk[3], highChunk[3];
        for (u32 a = 0; a < 3; a++)
        {
            lowChunk[a] = low[a] < 0 ? 0 : low[a] / kSide;
            highChunk[a] = high[a] / kSide;
            const int most = static_cast<int>(m_voxelChunkSide[a]) - 1;
            highChunk[a] = highChunk[a] > most ? most : highChunk[a];
        }
        for (int z = lowChunk[2]; z <= highChunk[2]; z++)
        {
            for (int y = lowChunk[1]; y <= highChunk[1]; y++)
            {
                for (int x = lowChunk[0]; x <= highChunk[0]; x++)
                {
                    if (VoxelChunk* c = m_voxelChunks[(static_cast<u32>(z) * m_voxelChunkSide[1] + static_cast<u32>(y)) * m_voxelChunkSide[0] + static_cast<u32>(x)])
                    {
                        c->changed = true;
                    }
                }
            }
        }
        m_landChanges++;
    }

    bool Space::setVoxel(int x, int y, int z, u32 type)
    {
        if (!putVoxel(x, y, z, static_cast<u8>(type > 255 ? 255 : type)))
        {
            return false;
        }
        voxelsChanged(x, y, z, x, y, z);
        return true;
    }

    u32 Space::fillVoxels(int x0, int y0, int z0, int x1, int y1, int z1, u32 type)
    {
        const auto order = [](int& a, int& b, int most)
        {
            if (a > b)
            {
                const int t = a;
                a = b;
                b = t;
            }
            a = a < 0 ? 0 : a;
            b = b >= most ? most - 1 : b;
        };
        order(x0, x1, m_voxelSize[0]);
        order(y0, y1, m_voxelSize[1]);
        order(z0, z1, m_voxelSize[2]);
        u32 changed = 0;
        for (int z = z0; z <= z1; z++)
        {
            for (int y = y0; y <= y1; y++)
            {
                for (int x = x0; x <= x1; x++)
                {
                    changed += putVoxel(x, y, z, static_cast<u8>(type > 255 ? 255 : type));
                }
            }
        }
        if (changed)
        {
            voxelsChanged(x0, y0, z0, x1, y1, z1);
        }
        return changed;
    }

    u32 Space::voxelBall(const Vec3& middle, float radius, u32 type)
    {
        if (!hasVoxels())
        {
            return 0;
        }
        const Vec3 at = (middle - m_voxelOrigin) * (1 / m_voxelCell);
        const float r = radius / m_voxelCell;
        const int x0 = static_cast<int>(floorf(at.x - r)), y0 = static_cast<int>(floorf(at.y - r)), z0 = static_cast<int>(floorf(at.z - r));
        const int x1 = static_cast<int>(floorf(at.x + r)), y1 = static_cast<int>(floorf(at.y + r)), z1 = static_cast<int>(floorf(at.z + r));
        u32 changed = 0;
        for (int z = z0; z <= z1; z++)
        {
            for (int y = y0; y <= y1; y++)
            {
                for (int x = x0; x <= x1; x++)
                {
                    const Vec3 d = Vec3(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f) - at;
                    if (d.dot(d) <= r * r)
                    {
                        changed += putVoxel(x, y, z, static_cast<u8>(type > 255 ? 255 : type));
                    }
                }
            }
        }
        if (changed)
        {
            voxelsChanged(x0, y0, z0, x1, y1, z1);
        }
        return changed;
    }

    void Space::voxelHills(u32 seed, float height, float roughness, int sea, float caves)
    {
        if (!hasVoxels())
        {
            return;
        }
        const int sizeY = m_voxelSize[1];
        const float base = static_cast<float>(sizeY) * 0.3f;
        const float rough = clampFloat(roughness, 0, 1);
        for (int z = 0; z < m_voxelSize[2]; z++)
        {
            for (int x = 0; x < m_voxelSize[0]; x++)
            {
                const float fx = static_cast<float>(x), fz = static_cast<float>(z);
                float n = smooth2(fx / 48, fz / 48, seed) * 0.6f + smooth2(fx / 20, fz / 20, seed + 1) * 0.3f * (0.5f + rough) + smooth2(fx / 7, fz / 7, seed + 2) * 0.12f * rough * 2;
                n = n * n * 1.4f;
                const int top = static_cast<int>(clampFloat(base + n * height, 1, static_cast<float>(sizeY - 1)));
                const bool beach = top <= sea + 1;
                const bool snowy = static_cast<float>(top) > base + height * 0.85f;
                for (int y = 0; y <= top; y++)
                {
                    u8 kind = 3;
                    if (y == top)
                    {
                        kind = beach ? 4 : snowy ? 8 : 1;
                    }
                    else if (y > top - 4)
                    {
                        kind = beach ? 4 : 2;
                    }
                    else if (y < 2)
                    {
                        kind = 10;
                    }
                    // Hollows under the hills, never breaking out of the top or the bottom.
                    if (caves > 0 && y > 2 && y < top - 3)
                    {
                        const float c = smooth3(fx / 9, static_cast<float>(y) / 6, fz / 9, seed + 7) * 0.65f + smooth3(fx / 4, static_cast<float>(y) / 3, fz / 4, seed + 8) * 0.35f;
                        if (c > 1 - caves * 0.45f)
                        {
                            continue;
                        }
                    }
                    putVoxel(x, y, z, kind);
                }
            }
        }
        voxelsChanged(0, 0, 0, m_voxelSize[0] - 1, m_voxelSize[1] - 1, m_voxelSize[2] - 1);
    }

    int Space::voxelTop(int x, int z) const
    {
        for (int y = m_voxelSize[1] - 1; y >= 0; y--)
        {
            if (voxel(x, y, z))
            {
                return y;
            }
        }
        return -1;
    }

    Vec3 Space::voxelCell(const Vec3& at) const
    {
        const Vec3 g = (at - m_voxelOrigin) * (1 / m_voxelCell);
        return { floorf(g.x), floorf(g.y), floorf(g.z) };
    }

    Vec3 Space::voxelPlace(int x, int y, int z) const
    {
        return m_voxelOrigin + Vec3(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, static_cast<float>(z) + 0.5f) * m_voxelCell;
    }

    u32 Space::voxelChunks() const
    {
        u32 n = 0;
        for (const VoxelChunk* c : m_voxelChunks)
        {
            n += c && c->solids;
        }
        return n;
    }

    u32 Space::voxelsWaiting() const
    {
        u32 n = 0;
        for (const VoxelChunk* c : m_voxelChunks)
        {
            n += c && c->changed && c->solids;
        }
        return n;
    }

    // --- Into triangles ---

    void Space::buildVoxelChunk(u32 index)
    {
        VoxelChunk& chunk = *m_voxelChunks[index];
        chunk.changed = false;
        gpu::Device& device = *m_solids->device();
        device.destroyMesh(chunk.mesh);
        chunk.mesh = {};
        chunk.indices = 0;
        if (chunk.solids == 0)
        {
            return;
        }
        const u32 sx = m_voxelChunkSide[0], sy = m_voxelChunkSide[1];
        const int bx = static_cast<int>(index % sx) * kSide, by = static_cast<int>(index / sx % sy) * kSide, bz = static_cast<int>(index / sx / sy) * kSide;
        // This chunk and one more all round, so that neighbours are quick to ask about.
        static u8 pad[kPad * kPad * kPad];
        for (int z = 0; z < kPad; z++)
        {
            for (int y = 0; y < kPad; y++)
            {
                for (int x = 0; x < kPad; x++)
                {
                    const bool inside = x > 0 && y > 0 && z > 0 && x <= kSide && y <= kSide && z <= kSide;
                    pad[(z * kPad + y) * kPad + x] = inside ? chunk.cells[((z - 1) * kSide + (y - 1)) * kSide + (x - 1)] : voxel(bx + x - 1, by + y - 1, bz + z - 1);
                }
            }
        }
        const auto at = [&](int x, int y, int z) { return pad[((z + 1) * kPad + (y + 1)) * kPad + (x + 1)]; };
        MeshData d;
        static u32 mask[kSide * kSide];
        const float cell = m_voxelCell;
        const Vec3 corner = m_voxelOrigin + Vec3(static_cast<float>(bx), static_cast<float>(by), static_cast<float>(bz)) * cell;
        for (int face = 0; face < 6; face++)
        {
            const int axis = face / 2, ua = (axis + 1) % 3, va = (axis + 2) % 3;
            const int sign = face & 1 ? -1 : 1;
            int n[3] = { 0, 0, 0 }, U[3] = { 0, 0, 0 }, V[3] = { 0, 0, 0 };
            n[axis] = sign;
            U[ua] = 1;
            V[va] = 1;
            const Vec3 normal(static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2]));
            for (int s = 0; s < kSide; s++)
            {
                // Which faces of this layer can be seen, by kind and how dark their corners are.
                for (int v = 0; v < kSide; v++)
                {
                    for (int u = 0; u < kSide; u++)
                    {
                        int p[3];
                        p[axis] = s;
                        p[ua] = u;
                        p[va] = v;
                        const u8 kind = at(p[0], p[1], p[2]);
                        u32 key = 0;
                        if (kind && !at(p[0] + n[0], p[1] + n[1], p[2] + n[2]))
                        {
                            const int q[3] = { p[0] + n[0], p[1] + n[1], p[2] + n[2] };
                            static const int du[4] = { -1, 1, 1, -1 }, dv[4] = { -1, -1, 1, 1 };
                            u32 ao[4];
                            for (int k = 0; k < 4; k++)
                            {
                                const bool a = at(q[0] + U[0] * du[k], q[1] + U[1] * du[k], q[2] + U[2] * du[k]) != 0;
                                const bool b = at(q[0] + V[0] * dv[k], q[1] + V[1] * dv[k], q[2] + V[2] * dv[k]) != 0;
                                const bool c = at(q[0] + U[0] * du[k] + V[0] * dv[k], q[1] + U[1] * du[k] + V[1] * dv[k], q[2] + U[2] * du[k] + V[2] * dv[k]) != 0;
                                ao[k] = a && b ? 0 : 3 - (a + b + c);
                            }
                            const bool even = ao[0] == ao[1] && ao[1] == ao[2] && ao[2] == ao[3];
                            key = kind | ao[0] << 8 | ao[1] << 10 | ao[2] << 12 | ao[3] << 14 | 1u << 16 | (even ? 0 : 1u << 17);
                        }
                        mask[v * kSide + u] = key;
                    }
                }
                // As few faces as will cover them: as wide as the same goes, then as many rows as are all the same.
                for (int v = 0; v < kSide; v++)
                {
                    for (int u = 0; u < kSide;)
                    {
                        const u32 key = mask[v * kSide + u];
                        if (key == 0)
                        {
                            u++;
                            continue;
                        }
                        int w = 1, h = 1;
                        if (!(key & 1u << 17))
                        {
                            while (u + w < kSide && mask[v * kSide + u + w] == key)
                            {
                                w++;
                            }
                            for (bool same = true; same && v + h < kSide; h += same)
                            {
                                for (int k = 0; k < w; k++)
                                {
                                    if (mask[(v + h) * kSide + u + k] != key)
                                    {
                                        same = false;
                                        break;
                                    }
                                }
                            }
                        }
                        for (int j = 0; j < h; j++)
                        {
                            for (int k = 0; k < w; k++)
                            {
                                mask[(v + j) * kSide + u + k] = 0;
                            }
                        }
                        const u8 kind = static_cast<u8>(key & 0xFF);
                        const gpu::Color& color = axis == 1 && sign > 0 ? m_voxelTop[kind] : m_voxelSide[kind];
                        float p0[3];
                        p0[axis] = static_cast<float>(s + (sign > 0 ? 1 : 0));
                        p0[ua] = static_cast<float>(u);
                        p0[va] = static_cast<float>(v);
                        const u32 first = d.vertices.count();
                        static const int cu[4] = { 0, 1, 1, 0 }, cv[4] = { 0, 0, 1, 1 };
                        bool room = true;
                        for (int k = 0; k < 4; k++)
                        {
                            float p[3] = { p0[0], p0[1], p0[2] };
                            p[ua] += static_cast<float>(cu[k] * w);
                            p[va] += static_cast<float>(cv[k] * h);
                            // Every other layer a shade lighter on top, so that steps of one block can be told apart.
                            const float dark = kDarker[(key >> (8 + k * 2)) & 3] * (axis == 1 && ((by + s) & 1) ? 0.88f : 1.0f);
                            const gpu::Color c = { color.r * dark, color.g * dark, color.b * dark, 1 };
                            // The grain of the ground, a quarter of it to a block.
                            room = room && d.vertex(corner + Vec3(p[0], p[1], p[2]) * cell, normal, c, p[ua] * 0.25f, p[va] * 0.25f);
                        }
                        if (!room)
                        {
                            break;
                        }
                        // Cut across the corners that are alike, so that the darkening runs the right way.
                        const u32 a0 = (key >> 8) & 3, a1 = (key >> 10) & 3, a2 = (key >> 12) & 3, a3 = (key >> 14) & 3;
                        const u32 o = a0 + a2 >= a1 + a3 ? 0 : 1;
                        const u32 i0 = first + o, i1 = first + (1 + o) % 4, i2 = first + (2 + o) % 4, i3 = first + (3 + o) % 4;
                        if (sign > 0)
                        {
                            d.triangle(i0, i1, i2);
                            d.triangle(i0, i2, i3);
                        }
                        else
                        {
                            d.triangle(i0, i2, i1);
                            d.triangle(i0, i3, i2);
                        }
                        u += w;
                    }
                }
            }
        }
        if (d.indices.count())
        {
            chunk.mesh = device.createMesh(d.vertices.begin(), d.vertices.count(), d.indices.begin(), d.indices.count(), false);
            chunk.indices = chunk.mesh ? d.indices.count() : 0;
        }
    }

    void Space::drawVoxels(const gpu::View& view, const Frustum& frustum, bool shadow)
    {
        if (!hasVoxels() || !m_solids)
        {
            return;
        }
        const float side = static_cast<float>(kSide) * m_voxelCell, radius = side * 0.8661f;
        const u32 sx = m_voxelChunkSide[0], sy = m_voxelChunkSide[1];
        // What has changed is made again: those that had been made already at once, as they were dug or built;
        // new ones a few a frame, nearest first.
        if (!shadow)
        {
            const u32 most = m_solids->device()->shadowSize() < gpu::kShadowSize ? 3 : 10;
            for (u32 made = 0; made < most;)
            {
                u32 pick = kNone;
                float nearest = 1e30f;
                for (u32 i = 0; i < m_voxelChunks.count(); i++)
                {
                    const VoxelChunk* c = m_voxelChunks[i];
                    if (c == nullptr || !c->changed)
                    {
                        continue;
                    }
                    if (c->mesh || c->solids == 0)
                    {
                        buildVoxelChunk(i);
                        continue;
                    }
                    const Vec3 middle = m_voxelOrigin + Vec3(static_cast<float>(i % sx) + 0.5f, static_cast<float>(i / sx % sy) + 0.5f, static_cast<float>(i / sx / sy) + 0.5f) * side;
                    const Vec3 d = middle - view.eye;
                    const float far = d.dot(d) * (frustum.sees(middle, radius) ? 1 : 4);
                    if (far < nearest)
                    {
                        nearest = far;
                        pick = i;
                    }
                }
                if (pick == kNone)
                {
                    break;
                }
                buildVoxelChunk(pick);
                made++;
            }
        }
        gpu::Device& device = *m_solids->device();
        for (u32 i = 0; i < m_voxelChunks.count(); i++)
        {
            const VoxelChunk* c = m_voxelChunks[i];
            if (c == nullptr || !c->mesh)
            {
                continue;
            }
            const Vec3 middle = m_voxelOrigin + Vec3(static_cast<float>(i % sx) + 0.5f, static_cast<float>(i / sx % sy) + 0.5f, static_cast<float>(i / sx / sy) + 0.5f) * side;
            const Vec3 d = middle - view.eye;
            const float far = fmaxf(0, sqrtf(d.dot(d)) - radius);
            if (shadow ? far > m_shadowDistance * 2 : (far > m_far || !frustum.sees(middle, radius)))
            {
                continue;
            }
            gpu::MeshDraw draw;
            draw.mesh = c->mesh;
            draw.count = c->indices;
            draw.texture = m_solids->grain();
            draw.repeat = true;
            draw.lines = m_lines && !shadow;
            pickLights(view, middle, radius, draw);
            device.drawMesh(draw);
            m_stats.drawn++;
        }
    }

    // --- Touching, and along a line ---

    u32 Space::touchVoxels(const Rigid& a, Contact (&contacts)[4]) const
    {
        if (!hasVoxels())
        {
            return 0;
        }
        const float inverse = 1 / m_voxelCell;
        const Vec3 low = (a.low - m_voxelOrigin) * inverse, high = (a.high - m_voxelOrigin) * inverse;
        int from[3] = { static_cast<int>(floorf(low.x)), static_cast<int>(floorf(low.y)), static_cast<int>(floorf(low.z)) };
        int to[3] = { static_cast<int>(floorf(high.x)), static_cast<int>(floorf(high.y)), static_cast<int>(floorf(high.z)) };
        for (u32 k = 0; k < 3; k++)
        {
            from[k] = from[k] < 0 ? 0 : from[k];
            to[k] = to[k] >= m_voxelSize[k] ? m_voxelSize[k] - 1 : to[k];
            if (from[k] > to[k] || (to[k] - from[k]) > 24)
            {
                return 0;
            }
        }
        u32 count = 0;
        Rigid box = {};
        box.entity = kNone;
        box.body = kNone;
        box.shape = Shape3::Box;
        box.half = Vec3(1, 1, 1) * (m_voxelCell * 0.5f);
        box.axes[0] = { 1, 0, 0 };
        box.axes[1] = { 0, 1, 0 };
        box.axes[2] = { 0, 0, 1 };
        Contact found[4];
        for (int z = from[2]; z <= to[2]; z++)
        {
            for (int y = from[1]; y <= to[1]; y++)
            {
                for (int x = from[0]; x <= to[0]; x++)
                {
                    // Only blocks with a side open: those inside can't be touched first.
                    if (!voxel(x, y, z) || (voxel(x - 1, y, z) && voxel(x + 1, y, z) && voxel(x, y - 1, z) && voxel(x, y + 1, z) && voxel(x, y, z - 1) && voxel(x, y, z + 1)))
                    {
                        continue;
                    }
                    box.place = voxelPlace(x, y, z);
                    box.low = box.place - box.half;
                    box.high = box.place + box.half;
                    const u32 n = touchPair(a, box, found);
                    for (u32 i = 0; i < n; i++)
                    {
                        // Against the face between two blocks there is nothing: it is a seam, not a side.
                        const Vec3 out = -found[i].normal;
                        const int ox = fabsf(out.x) > 0.9f ? (out.x > 0 ? 1 : -1) : 0, oy = fabsf(out.y) > 0.9f ? (out.y > 0 ? 1 : -1) : 0, oz = fabsf(out.z) > 0.9f ? (out.z > 0 ? 1 : -1) : 0;
                        if ((ox || oy || oz) && voxel(x + ox, y + oy, z + oz))
                        {
                            continue;
                        }
                        if (count < 4)
                        {
                            contacts[count++] = found[i];
                            continue;
                        }
                        u32 least = 0;
                        for (u32 k = 1; k < 4; k++)
                        {
                            least = contacts[k].depth < contacts[least].depth ? k : least;
                        }
                        if (contacts[least].depth < found[i].depth)
                        {
                            contacts[least] = found[i];
                        }
                    }
                }
            }
        }
        return count;
    }

    u32 Space::touchLand(const Rigid& a, Contact (&contacts)[4]) const
    {
        u32 count = touchHeights(a, contacts);
        if (!hasVoxels())
        {
            return count;
        }
        Contact more[4];
        const u32 extra = touchVoxels(a, more);
        for (u32 i = 0; i < extra; i++)
        {
            if (count < 4)
            {
                contacts[count++] = more[i];
                continue;
            }
            u32 least = 0;
            for (u32 k = 1; k < 4; k++)
            {
                least = contacts[k].depth < contacts[least].depth ? k : least;
            }
            if (contacts[least].depth < more[i].depth)
            {
                contacts[least] = more[i];
            }
        }
        return count;
    }

    // From cell to cell along the line, to the first face of a block it goes into. 2 if it meets none.
    float Space::rayVoxels(const Vec3& from, const Vec3& way, int (&cell)[3], Vec3& normal) const
    {
        if (!hasVoxels())
        {
            return 2;
        }
        const float inverse = 1 / m_voxelCell;
        const Vec3 start = (from - m_voxelOrigin) * inverse, along = way * inverse;
        const float s[3] = { start.x, start.y, start.z }, w[3] = { along.x, along.y, along.z };
        // Only within the blocks' box.
        float enter = 0, leave = 1;
        int enterAxis = -1;
        for (u32 k = 0; k < 3; k++)
        {
            const float size = static_cast<float>(m_voxelSize[k]);
            if (fabsf(w[k]) < 1e-9f)
            {
                if (s[k] < 0 || s[k] >= size)
                {
                    return 2;
                }
                continue;
            }
            float t0 = (0 - s[k]) / w[k], t1 = (size - s[k]) / w[k];
            if (t0 > t1)
            {
                const float t = t0;
                t0 = t1;
                t1 = t;
            }
            if (t0 > enter)
            {
                enter = t0;
                enterAxis = static_cast<int>(k);
            }
            leave = fminf(leave, t1);
        }
        if (enter > leave)
        {
            return 2;
        }
        int c[3], step[3];
        float next[3], delta[3];
        for (u32 k = 0; k < 3; k++)
        {
            const float p = s[k] + w[k] * (enter + 1e-5f);
            c[k] = static_cast<int>(floorf(p));
            c[k] = c[k] < 0 ? 0 : c[k] >= m_voxelSize[k] ? m_voxelSize[k] - 1 : c[k];
            step[k] = w[k] > 0 ? 1 : -1;
            delta[k] = fabsf(w[k]) > 1e-9f ? fabsf(1 / w[k]) : 1e30f;
            next[k] = fabsf(w[k]) > 1e-9f ? ((static_cast<float>(c[k]) + (w[k] > 0 ? 1 : 0)) - s[k]) / w[k] : 1e30f;
        }
        // From inside a block, nothing is met until the line has come out of it.
        bool out = !voxel(c[0], c[1], c[2]);
        int came = enterAxis;
        float t = enter;
        for (u32 i = 0; i < 4096; i++)
        {
            if (out && voxel(c[0], c[1], c[2]))
            {
                cell[0] = c[0];
                cell[1] = c[1];
                cell[2] = c[2];
                normal = Vec3(0, 0, 0);
                if (came >= 0)
                {
                    float n[3] = { 0, 0, 0 };
                    n[came] = static_cast<float>(-step[came]);
                    normal = Vec3(n[0], n[1], n[2]);
                }
                else
                {
                    normal = -way.normalized();
                }
                return t;
            }
            out = out || !voxel(c[0], c[1], c[2]);
            const int k = next[0] < next[1] ? (next[0] < next[2] ? 0 : 2) : (next[1] < next[2] ? 1 : 2);
            t = next[k];
            if (t > leave)
            {
                return 2;
            }
            c[k] += step[k];
            next[k] += delta[k];
            came = k;
            if (c[k] < 0 || c[k] >= m_voxelSize[k])
            {
                return 2;
            }
        }
        return 2;
    }

    bool Space::voxelRay(const Vec3& from, const Vec3& to, Vec3& cell, Vec3& face, Vec3& hit) const
    {
        int c[3];
        Vec3 normal;
        const float t = rayVoxels(from, to - from, c, normal);
        if (t > 1)
        {
            return false;
        }
        cell = Vec3(static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]));
        face = normal;
        hit = from + (to - from) * t;
        return true;
    }
}
