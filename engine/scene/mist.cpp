#include "scene/space.h"

#include <math.h>
#include <string.h>

// Mist that can be walked into: a box of it, drawn as slices that face the camera, from far to near, each a little
// see-through and coloured by noise in three dimensions that drifts with the wind. Soft at the edges of the box, and
// thinning just in front of the camera, so that it has no edges and nothing pops as one goes into it.
namespace nx
{
    namespace
    {
        constexpr u32 kNoise = 32;      // on each side, repeating
        constexpr u32 kSlices = 20;
        constexpr u32 kGrid = 8;        // squares across each slice
        constexpr u32 kSliceVertices = (kGrid + 1) * (kGrid + 1);

        float clampFloat(float v, float low, float high) { return v < low ? low : v > high ? high : v; }

        float lattice(int x, int y, int z, int period, u32 seed)
        {
            x = ((x % period) + period) % period;
            y = ((y % period) + period) % period;
            z = ((z % period) + period) % period;
            u32 h = static_cast<u32>(x) * 73856093u ^ static_cast<u32>(y) * 19349663u ^ static_cast<u32>(z) * 83492791u ^ seed * 2654435761u;
            h ^= h >> 13;
            h *= 0x5bd1e995u;
            h ^= h >> 15;
            return static_cast<float>(h & 0xFFFF) / 65535.0f;
        }

        // Smooth between random values at whole numbers, repeating every period.
        float noise(float x, float y, float z, int period, u32 seed)
        {
            const int ix = static_cast<int>(floorf(x)), iy = static_cast<int>(floorf(y)), iz = static_cast<int>(floorf(z));
            float fx = x - static_cast<float>(ix), fy = y - static_cast<float>(iy), fz = z - static_cast<float>(iz);
            fx = fx * fx * (3 - 2 * fx);
            fy = fy * fy * (3 - 2 * fy);
            fz = fz * fz * (3 - 2 * fz);
            float c[2][2][2];
            for (int k = 0; k < 2; k++)
            {
                for (int j = 0; j < 2; j++)
                {
                    for (int i = 0; i < 2; i++)
                    {
                        c[k][j][i] = lattice(ix + i, iy + j, iz + k, period, seed);
                    }
                }
            }
            const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
            const float a = lerp(lerp(c[0][0][0], c[0][0][1], fx), lerp(c[0][1][0], c[0][1][1], fx), fy);
            const float b = lerp(lerp(c[1][0][0], c[1][0][1], fx), lerp(c[1][1][0], c[1][1][1], fx), fy);
            return lerp(a, b, fz);
        }
    }

    // White, with how thick the mist is there as its alpha: billows, and finer wisps on them.
    gpu::Texture Space::noiseVolume()
    {
        if (m_noiseVolume || !m_solids)
        {
            return m_noiseVolume;
        }
        u8* pixels = static_cast<u8*>(memAlloc(kNoise * kNoise * kNoise * 4, MemTag::Frame));
        if (pixels == nullptr)
        {
            return {};
        }
        for (u32 z = 0; z < kNoise; z++)
        {
            for (u32 y = 0; y < kNoise; y++)
            {
                for (u32 x = 0; x < kNoise; x++)
                {
                    const float fx = static_cast<float>(x), fy = static_cast<float>(y), fz = static_cast<float>(z);
                    float v = noise(fx / 8, fy / 8, fz / 8, 4, 1) * 0.6f + noise(fx / 4, fy / 4, fz / 4, 8, 2) * 0.28f + noise(fx / 2, fy / 2, fz / 2, 16, 3) * 0.12f;
                    v = clampFloat((v - 0.28f) / 0.55f, 0, 1);
                    u8* p = pixels + ((z * kNoise + y) * kNoise + x) * 4;
                    p[0] = p[1] = p[2] = 255;
                    p[3] = static_cast<u8>(v * v * (3 - 2 * v) * 255);
                }
            }
        }
        m_noiseVolume = m_solids->device()->createVolume(pixels, kNoise);
        memFree(pixels);
        return m_noiseVolume;
    }

    u32 Space::addMist(const Vec3& middle, const Vec3& size, const gpu::Color& color, float thickness, float swirl, float grain)
    {
        Mist m = {};
        m.id = ++m_nextMist;
        m.middle = middle;
        m.size = size;
        m.color = color;
        m.thickness = thickness;
        m.swirl = swirl;
        m.grain = grain > 0.5f ? grain : 0.5f;
        return m_mists.push(m) ? m.id : 0;
    }

    bool Space::moveMist(u32 id, const Vec3& middle)
    {
        for (Mist& m : m_mists)
        {
            if (m.id == id)
            {
                m.middle = middle;
                return true;
            }
        }
        return false;
    }

    void Space::removeMist(u32 id)
    {
        for (u32 i = 0; i < m_mists.count(); i++)
        {
            if (m_mists[i].id == id)
            {
                if (m_solids)
                {
                    m_solids->device()->destroyMesh(m_mists[i].mesh);
                }
                m_mists.swapRemove(i);
                return;
            }
        }
    }

    void Space::clearMists()
    {
        while (m_mists.count())
        {
            removeMist(m_mists[0].id);
        }
        if (m_noiseVolume && m_solids)
        {
            m_solids->device()->destroyTexture(m_noiseVolume);
        }
        m_noiseVolume = {};
    }

    void Space::drawMists(const gpu::View& view, const Frustum& frustum)
    {
        if (m_mists.count() == 0 || m_lines || !m_solids)
        {
            return;
        }
        gpu::Device& device = *m_solids->device();
        const gpu::Texture volume = noiseVolume();
        static gpu::MeshVertex vertices[kSlices * kSliceVertices];
        const Vec3 forward = view.forward.normalized(), right = view.right.normalized(), up = view.up.normalized();
        for (Mist& m : m_mists)
        {
            const Vec3 half = m.size * 0.5f;
            const float radius = sqrtf(half.dot(half));
            if (!frustum.sees(m.middle, radius))
            {
                continue;
            }
            if (!m.mesh)
            {
                // The same squares for every frame; only where their corners are changes.
                static u16 indices[kSlices * kGrid * kGrid * 6];
                u32 n = 0;
                for (u32 s = 0; s < kSlices; s++)
                {
                    for (u32 y = 0; y < kGrid; y++)
                    {
                        for (u32 x = 0; x < kGrid; x++)
                        {
                            const u16 a = static_cast<u16>(s * kSliceVertices + y * (kGrid + 1) + x);
                            const u16 b = static_cast<u16>(a + 1), c = static_cast<u16>(a + kGrid + 1), d = static_cast<u16>(c + 1);
                            const u16 six[6] = { a, c, b, b, c, d };
                            for (u16 i : six)
                            {
                                indices[n++] = i;
                            }
                        }
                    }
                }
                memset(vertices, 0, sizeof(vertices));
                m.mesh = device.createMesh(vertices, kSlices * kSliceVertices, indices, n, true);
                if (!m.mesh)
                {
                    continue;
                }
            }
            // As much of each slice as the box covers, seen from the camera: a wide, low bank is thin bands, not
            // whole screens.
            float left = 1e9f, rightMost = -1e9f, low = 1e9f, high = -1e9f;
            for (u32 c = 0; c < 8; c++)
            {
                const Vec3 corner = Vec3(c & 1 ? half.x : -half.x, c & 2 ? half.y : -half.y, c & 4 ? half.z : -half.z);
                const float a = corner.dot(right), b = corner.dot(up);
                left = fminf(left, a);
                rightMost = fmaxf(rightMost, a);
                low = fminf(low, b);
                high = fmaxf(high, b);
            }
            // Slices from far to near, across the ball round the box. Each lets through as much as its share of
            // the whole thickness, so the number of them doesn't matter.
            const u32 slices = device.shadowSize() < gpu::kShadowSize ? kSlices / 2 : kSlices;
            const float gap = 2 * radius / static_cast<float>(slices);
            const float share = 1 - powf(clampFloat(1 - m.color.a * 0.9f, 0.01f, 1), m.thickness * gap / fmaxf(0.5f, fminf(half.x, fminf(half.y, half.z)) * 2) * 3);
            const float soft = 0.3f;    // of each half, where it thins to nothing at the box's sides
            const Vec3 toMiddle = m.middle - view.eye;
            u32 v = 0;
            for (u32 s = 0; s < slices; s++)
            {
                const float along = radius - (static_cast<float>(s) + 0.5f) * gap;
                const Vec3 centre = m.middle + forward * along;
                const float ahead = toMiddle.dot(forward) + along;   // how far in front of the camera it is
                const float nearFade = clampFloat((ahead - 0.3f) / 2.5f, 0, 1);
                for (u32 y = 0; y <= kGrid; y++)
                {
                    for (u32 x = 0; x <= kGrid; x++, v++)
                    {
                        const float u = static_cast<float>(x) / kGrid * 2 - 1, w = static_cast<float>(y) / kGrid * 2 - 1;
                        const Vec3 p = centre + right * (left + (rightMost - left) * (u * 0.5f + 0.5f)) + up * (low + (high - low) * (w * 0.5f + 0.5f));
                        const Vec3 local = p - m.middle;
                        float inside = 1;
                        const float axes[3] = { local.x / fmaxf(half.x, 0.01f), local.y / fmaxf(half.y, 0.01f), local.z / fmaxf(half.z, 0.01f) };
                        for (float a : axes)
                        {
                            const float t = clampFloat((1 - fabsf(a)) / soft, 0, 1);
                            inside *= t * t * (3 - 2 * t);
                        }
                        const float alpha = clampFloat(share * inside * nearFade, 0, 1);
                        gpu::MeshVertex& out = vertices[v];
                        out.x = p.x;
                        out.y = p.y;
                        out.z = p.z;
                        out.nx = 0;
                        out.ny = 1;
                        out.nz = 0;
                        out.r = static_cast<u8>(clampFloat(m.color.r, 0, 1) * 255);
                        out.g = static_cast<u8>(clampFloat(m.color.g, 0, 1) * 255);
                        out.b = static_cast<u8>(clampFloat(m.color.b, 0, 1) * 255);
                        out.a = static_cast<u8>(alpha * 255);
                        out.u = out.v = 0;
                    }
                }
            }
            device.updateMesh(m.mesh, vertices, slices * kSliceVertices);
            gpu::MeshDraw d;
            d.mesh = m.mesh;
            d.count = slices * kGrid * kGrid * 6;
            d.blend = gpu::Blend::Alpha;
            d.depthWrite = false;
            d.cull = gpu::Cull::None;
            d.lit = false;
            d.volume = volume;
            d.volumeScale = 1.0f / m.grain;
            // Drifting with the wind, and rising: a bank of it hardly at all, smoke steadily, fire in leaps. In metres a
            // second, and the noise is moved the other way to move what is seen this way.
            const float rise = 0.04f * m.swirl * m.swirl;
            const Vec3 drift = Vec3(m_wind.x * 0.35f, rise, m_wind.z * 0.35f);
            d.volumeOffset = drift * (-m_time * d.volumeScale);
            if (!volume)
            {
                // Without volumes: plain, which is still soft at its edges.
                d.color = { 1, 1, 1, 0.55f };
            }
            device.drawMesh(d);
        }
    }
}
