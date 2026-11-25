#include "scene/space.h"
#include "scene/world.h"

#include <math.h>
#include <string.h>

// Hills, in regions that come and go; water; and shapes by their distance.
namespace nx
{
    namespace
    {
        float clampFloat(float v, float low, float high)
        {
            return v < low ? low : v > high ? high : v;
        }

        gpu::Color mix(const gpu::Color& a, const gpu::Color& b, float t)
        {
            return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
        }

        float hash(int x, int z, u32 seed)
        {
            u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(z) * 668265263u + seed * 2246822519u;
            h = (h ^ (h >> 13)) * 1274126177u;
            return static_cast<float>((h ^ (h >> 16)) >> 8) / 16777216.0f;
        }

        // Smooth between random values at whole numbers.
        float noise(float x, float z, u32 seed)
        {
            const float fx = floorf(x), fz = floorf(z);
            const int ix = static_cast<int>(fx), iz = static_cast<int>(fz);
            float tx = x - fx, tz = z - fz;
            tx = tx * tx * tx * (tx * (tx * 6 - 15) + 10);
            tz = tz * tz * tz * (tz * (tz * 6 - 15) + 10);
            const float a = hash(ix, iz, seed), b = hash(ix + 1, iz, seed), c = hash(ix, iz + 1, seed), d = hash(ix + 1, iz + 1, seed);
            return a + (b - a) * tx + (c - a) * tz + (a - b - c + d) * tx * tz;
        }
    }

    // --- Regions ---

    void Space::startLand(u32 seed, float spacing, float height, float roughness)
    {
        while (m_chunks.count())
        {
            removeChunk(m_chunks.count() - 1);
        }
        m_flats.clear();
        m_sculpt.clear();
        m_paint.clear();
        m_regionEvents.clear();
        m_regionNext = 0;
        m_landSeed = seed;
        m_landSpacing = spacing > 0.1f ? spacing : 0.1f;
        m_landHeight = height;
        m_landRough = clampFloat(roughness, 0, 1);
        m_landSquares = 0;
        m_island = m_started = false;
        m_landOn = true;
    }

    bool Space::makeLand(u32 seed, u32 squares, float spacing, float height, float roughness, bool island)
    {
        if (m_solids == nullptr)
        {
            return false;
        }
        startLand(seed, spacing, height, roughness);
        squares = squares / kChunk * kChunk;
        m_landSquares = squares < kChunk ? kChunk : squares > 256 ? 256 : squares;
        m_island = island;
        const int half = static_cast<int>(m_landSquares / kChunk) / 2, more = static_cast<int>(m_landSquares / kChunk) - half;
        for (int z = -half; z < more; z++)
        {
            for (int x = -half; x < more; x++)
            {
                if (!addChunk(x, z))
                {
                    m_landOn = false;
                    return false;
                }
            }
        }
        return true;
    }

    bool Space::makeEndlessLand(u32 seed, float spacing, float height, float roughness, u32 reach)
    {
        if (m_solids == nullptr)
        {
            return false;
        }
        startLand(seed, spacing, height, roughness);
        m_reach = reach < 1 ? 1 : reach > 12 ? 12 : reach;
        return true;
    }

    // How high a corner of a square is, before anything is levelled. Squares are numbered from the middle.
    float Space::generate(int x, int z) const
    {
        const float px = static_cast<float>(x) * m_landSpacing * 0.011f, pz = static_cast<float>(z) * m_landSpacing * 0.011f;
        float sum = 0, size = 1, total = 0, across = 1;
        for (u32 octave = 0; octave < 6; octave++)
        {
            float n = noise(px * across + 173.3f, pz * across + 91.7f, m_landSeed + octave);
            // Ridges among the larger shapes, softness among the smaller.
            n = octave < 3 ? 1 - fabsf(n * 2 - 1) * (0.35f + m_landRough * 0.65f) : n;
            sum += n * size;
            total += size;
            size *= 0.42f + m_landRough * 0.12f;
            across *= 2.07f;
        }
        const float h = sum / total;
        float level = (h * h * 1.4f - 0.25f) * m_landHeight;
        if (m_island)
        {
            const float half = static_cast<float>(m_landSquares) * 0.5f;
            const float dx = static_cast<float>(x) / half, dz = static_cast<float>(z) / half;
            const float t = clampFloat((sqrtf(dx * dx + dz * dz) + (noise(px * 2 + 50, pz * 2 + 50, m_landSeed + 50) - 0.5f) * 0.2f - 0.55f) / 0.4f, 0, 1);
            level += (-m_landHeight * 0.3f - level) * t * t * (3 - 2 * t);
        }
        const float wx = static_cast<float>(x) * m_landSpacing, wz = static_cast<float>(z) * m_landSpacing;
        for (const Flat& f : m_flats)
        {
            const float ox = wx - f.x, oz = wz - f.z;
            const float far = sqrtf(ox * ox + oz * oz) / f.radius;
            if (far < 1)
            {
                // Flat in the middle, and gently back to what it was.
                const float t = clampFloat((1 - far) * 2, 0, 1);
                level += (f.level - level) * t * t * (3 - 2 * t);
            }
        }
        return level + sculpted(x, z);
    }

    const Space::Chunk* Space::chunkAt(int x, int z) const
    {
        const u32 slot = m_table[static_cast<u32>(z & 31) * 32 + static_cast<u32>(x & 31)];
        const Chunk* c = slot ? &m_chunks[slot - 1] : nullptr;
        return c && c->x == x && c->z == z ? c : nullptr;
    }

    float Space::corner(int x, int z) const
    {
        const int cx = x >> 5, cz = z >> 5;
        const Chunk* c = chunkAt(cx, cz);
        return c ? c->heights[static_cast<u32>(z - cz * 32 + 1) * kRow + static_cast<u32>(x - cx * 32 + 1)] : generate(x, z);
    }

    void Space::fillChunk(Chunk& c)
    {
        c.changed = true;
        c.low = 1e9f;
        c.high = -1e9f;
        for (u32 z = 0; z < kRow; z++)
        {
            for (u32 x = 0; x < kRow; x++)
            {
                const float h = generate(c.x * 32 + static_cast<int>(x) - 1, c.z * 32 + static_cast<int>(z) - 1);
                c.heights[z * kRow + x] = h;
                c.low = fminf(c.low, h);
                c.high = fmaxf(c.high, h);
            }
        }
    }

    bool Space::addChunk(int x, int z)
    {
        u16& slot = m_table[static_cast<u32>(z & 31) * 32 + static_cast<u32>(x & 31)];
        if (slot)
        {
            return false;  // another, from very far away, that hasn't gone yet
        }
        Chunk c = {};
        c.x = x;
        c.z = z;
        c.heights = static_cast<float*>(memAlloc(kRow * kRow * sizeof(float), MemTag::Scene));
        if (c.heights == nullptr || !m_chunks.push(c))
        {
            memFree(c.heights);
            return false;
        }
        slot = static_cast<u16>(m_chunks.count());
        fillChunk(m_chunks[m_chunks.count() - 1]);
        return true;
    }

    void Space::removeChunk(u32 index)
    {
        Chunk& c = m_chunks[index];
        m_table[static_cast<u32>(c.z & 31) * 32 + static_cast<u32>(c.x & 31)] = 0;
        if (m_solids && m_solids->device())
        {
            m_solids->device()->destroyMesh(c.mesh);
        }
        memFree(c.heights);
        m_chunks.swapRemove(index);
        if (index < m_chunks.count())
        {
            const Chunk& moved = m_chunks[index];
            m_table[static_cast<u32>(moved.z & 31) * 32 + static_cast<u32>(moved.x & 31)] = static_cast<u16>(index + 1);
        }
    }

    // What has come near, a little each step, and what has been left behind.
    void Space::stream()
    {
        m_regionsWaiting = 0;
        if (!m_landOn || m_landSquares || m_focusCount == 0)
        {
            m_focusCount = 0;
            return;
        }
        const float size = regionSize();
        const u32 allowed = m_started ? 1 : 400;
        const float reach = m_started ? static_cast<float>(m_reach) + 0.5f : fminf(static_cast<float>(m_reach), 2) + 0.5f;
        for (u32 added = 0; added < allowed; added++)
        {
            float nearest = reach;
            int bx = 0, bz = 0;
            bool found = false;
            m_regionsWaiting = 0;
            for (u32 f = 0; f < m_focusCount; f++)
            {
                const float fx = m_focus[f].x / size - 0.5f, fz = m_focus[f].z / size - 0.5f;
                const int mx = static_cast<int>(floorf(fx + 0.5f)), mz = static_cast<int>(floorf(fz + 0.5f));
                const int r = static_cast<int>(reach) + 1;
                for (int z = mz - r; z <= mz + r; z++)
                {
                    for (int x = mx - r; x <= mx + r; x++)
                    {
                        const float dx = static_cast<float>(x) - fx, dz = static_cast<float>(z) - fz;
                        const float far = sqrtf(dx * dx + dz * dz);
                        if (far < reach && !chunkAt(x, z))
                        {
                            m_regionsWaiting++;
                            if (far < nearest)
                            {
                                nearest = far;
                                bx = x;
                                bz = z;
                                found = true;
                            }
                        }
                    }
                }
            }
            if (!found || !addChunk(bx, bz))
            {
                break;
            }
            if (m_regionEvents.count() < 2048)
            {
                m_regionEvents.push({ bx, bz, true });
            }
        }
        m_started = true;
        // A little farther than they come, so that they don't come and go at the edge.
        for (u32 i = 0; i < m_chunks.count();)
        {
            const Chunk& c = m_chunks[i];
            bool near = false;
            for (u32 f = 0; f < m_focusCount; f++)
            {
                const float dx = static_cast<float>(c.x) + 0.5f - m_focus[f].x / size, dz = static_cast<float>(c.z) + 0.5f - m_focus[f].z / size;
                near = near || sqrtf(dx * dx + dz * dz) < static_cast<float>(m_reach) + 1.7f;
            }
            if (near)
            {
                i++;
                continue;
            }
            if (m_regionEvents.count() < 2048)
            {
                m_regionEvents.push({ c.x, c.z, false });
            }
            for (const Spatial& s : m_spatials)
            {
                if (s.streamed && m_world.m_entities[s.owner].parent == kNone && static_cast<int>(floorf(s.place.x / size)) == c.x && static_cast<int>(floorf(s.place.z / size)) == c.z)
                {
                    m_world.destroyEntity(idOf(s.owner));
                }
            }
            removeChunk(i);
        }
        m_focusCount = 0;
    }

    bool Space::nextRegion(int& x, int& z, bool& arrived)
    {
        if (m_regionNext >= m_regionEvents.count())
        {
            m_regionEvents.clear();
            m_regionNext = 0;
            return false;
        }
        const RegionEvent& e = m_regionEvents[m_regionNext++];
        x = e.x;
        z = e.z;
        arrived = e.arrived;
        return true;
    }

    void Space::setStreamed(u32 id, bool streamed)
    {
        if (Spatial* s = spatial(id))
        {
            s->streamed = streamed;
        }
    }

    void Space::flatten(float x, float z, float radius, float level)
    {
        if (!m_landOn || !(radius > 0) || m_flats.count() >= 256)
        {
            return;
        }
        m_flats.push({ x, z, radius, level });
        const float size = regionSize(), reach = radius + m_landSpacing * 2;
        for (Chunk& c : m_chunks)
        {
            const float lx = static_cast<float>(c.x) * size, lz = static_cast<float>(c.z) * size;
            if (x + reach > lx && x - reach < lx + size && z + reach > lz && z - reach < lz + size)
            {
                fillChunk(c);
            }
        }
    }

    void Space::setLandColors(const gpu::Color& low, const gpu::Color& middle, const gpu::Color& high, const gpu::Color& steep, float lowHeight, float highHeight)
    {
        m_landColors[0] = low;
        m_landColors[1] = middle;
        m_landColors[2] = high;
        m_landColors[3] = steep;
        m_landLow = lowHeight;
        m_landHigh = highHeight;
        for (Chunk& c : m_chunks)
        {
            c.changed = true;
        }
    }

    float Space::heightAt(float x, float z) const
    {
        const float flat = m_hasGround ? m_ground : -1e6f;
        if (!m_landOn)
        {
            return flat;
        }
        const float gx = x / m_landSpacing, gz = z / m_landSpacing;
        const float half = m_landSquares ? static_cast<float>(m_landSquares) * 0.5f : 1e6f;
        if (!(gx >= -half && gz >= -half && gx <= half && gz <= half))
        {
            return flat;
        }
        int ix = static_cast<int>(floorf(gx)), iz = static_cast<int>(floorf(gz));
        if (m_landSquares)
        {
            // The far edge belongs to the last square.
            ix = ix >= static_cast<int>(half) ? ix - 1 : ix;
            iz = iz >= static_cast<int>(half) ? iz - 1 : iz;
        }
        const float tx = gx - static_cast<float>(ix), tz = gz - static_cast<float>(iz);
        float a, b, c, d;
        const int cx = ix >> 5, cz = iz >> 5;
        if (const Chunk* chunk = chunkAt(cx, cz))
        {
            const float* row = chunk->heights + static_cast<u32>(iz - cz * 32 + 1) * kRow + static_cast<u32>(ix - cx * 32 + 1);
            a = row[0];
            b = row[1];
            c = row[kRow];
            d = row[kRow + 1];
        }
        else
        {
            a = corner(ix, iz);
            b = corner(ix + 1, iz);
            c = corner(ix, iz + 1);
            d = corner(ix + 1, iz + 1);
        }
        // On the same two triangles that are drawn.
        const float h = tx > tz ? a + (b - a) * tx + (d - b) * tz : a + (c - a) * tz + (d - c) * tx;
        return fmaxf(h, flat);
    }

    Vec3 Space::normalAt(float x, float z) const
    {
        if (!m_landOn)
        {
            return { 0, 1, 0 };
        }
        const float e = m_landSpacing * 0.5f;
        const float l = heightAt(x - e, z), r = heightAt(x + e, z), n = heightAt(x, z - e), f = heightAt(x, z + e);
        if (l < -9e5f || r < -9e5f || n < -9e5f || f < -9e5f)
        {
            return { 0, 1, 0 };
        }
        return Vec3((l - r) / (2 * e), 1, (n - f) / (2 * e)).normalized();
    }

    gpu::Color Space::landColor(float height, const Vec3& normal, float x, float z) const
    {
        const float wander = (noise(x * 0.13f, z * 0.13f, 99) - 0.5f) * (m_landHigh - m_landLow) * 0.16f;
        const float span = fmaxf(0.01f, (m_landHigh - m_landLow) * 0.14f);
        gpu::Color c = mix(m_landColors[0], m_landColors[1], clampFloat((height + wander - m_landLow) / span, 0, 1));
        c = mix(c, m_landColors[2], clampFloat((height + wander - m_landHigh) / span, 0, 1));
        c = mix(c, m_landColors[3], clampFloat((0.86f - normal.y) * 6, 0, 1));
        const float shade = 0.9f + noise(x * 1.7f, z * 1.7f, 7) * 0.2f;
        return { clampFloat(c.r * shade, 0, 1), clampFloat(c.g * shade, 0, 1), clampFloat(c.b * shade, 0, 1), 1 };
    }

    void Space::buildChunk(Chunk& chunk)
    {
        chunk.changed = false;
        MeshData d;
        const u32 row = kChunk + 1;
        for (u32 z = 0; z <= kChunk; z++)
        {
            for (u32 x = 0; x <= kChunk; x++)
            {
                const float* at = chunk.heights + (z + 1) * kRow + x + 1;
                const Vec3 normal = Vec3((at[-1] - at[1]) / (2 * m_landSpacing), 1, (at[-static_cast<int>(kRow)] - at[kRow]) / (2 * m_landSpacing)).normalized();
                const float px = static_cast<float>(chunk.x * 32 + static_cast<int>(x)) * m_landSpacing, pz = static_cast<float>(chunk.z * 32 + static_cast<int>(z)) * m_landSpacing;
                gpu::Color color = landColor(*at, normal, px, pz);
                if (const u8* paint = painted(chunk.x * 32 + static_cast<int>(x), chunk.z * 32 + static_cast<int>(z)))
                {
                    color = mix(color, { static_cast<float>(paint[0]) / 255, static_cast<float>(paint[1]) / 255, static_cast<float>(paint[2]) / 255, 1 }, static_cast<float>(paint[3]) / 255);
                }
                // Within the region, so that they stay exact far from the middle; the grain repeats anyway.
                d.vertex({ px, *at, pz }, normal, color, static_cast<float>(x) * m_landSpacing * 0.25f, static_cast<float>(z) * m_landSpacing * 0.25f);
            }
        }
        // A skirt all round, hanging down, hides the gaps between chunks of different detail.
        u32 rim[4 * kChunk];
        for (u32 i = 0; i < kChunk; i++)
        {
            rim[i] = i;
            rim[kChunk + i] = i * row + kChunk;
            rim[kChunk * 2 + i] = kChunk * row + kChunk - i;
            rim[kChunk * 3 + i] = (kChunk - i) * row;
        }
        const u32 skirt = d.vertices.count();
        for (const u32 i : rim)
        {
            gpu::MeshVertex v = d.vertices[i];
            v.y -= m_landSpacing * 3;
            d.vertices.push(v);
        }
        // Every level from the same vertices: the triangles of all three, one after another.
        for (u32 detail = 0; detail < kChunkDetails; detail++)
        {
            const u32 step = 1u << (detail * 2 > 3 ? 3 : detail * 2);
            for (u32 z = 0; z < kChunk; z += step)
            {
                for (u32 x = 0; x < kChunk; x += step)
                {
                    const u32 a = z * row + x, b = a + step, c = a + step * row, e = c + step;
                    d.triangle(a, e, b);
                    d.triangle(a, c, e);
                }
            }
            for (u32 i = 0; i < 4 * kChunk; i += step)
            {
                const u32 next = (i + step) % (4 * kChunk);
                d.triangle(rim[i], rim[next], skirt + i);
                d.triangle(rim[next], skirt + next, skirt + i);
            }
        }
        gpu::Device& device = *m_solids->device();
        device.destroyMesh(chunk.mesh);
        chunk.mesh = device.createMesh(d.vertices.begin(), d.vertices.count(), d.indices.begin(), d.indices.count(), false);
    }

    void Space::drawLand(const gpu::View& view, const Frustum& frustum, bool shadow)
    {
        if (!m_landOn)
        {
            return;
        }
        const float size = regionSize();
        u32 first[kChunkDetails + 1] = {};
        for (u32 detail = 0; detail < kChunkDetails; detail++)
        {
            const u32 step = 1u << (detail * 2 > 3 ? 3 : detail * 2);
            const u32 squares = kChunk / step;
            first[detail + 1] = first[detail] + squares * squares * 6 + 4 * squares * 6;
        }
        // Meshes are made a few at a time, nearest first, while the land is streaming.
        u32 made = 0;
        for (Chunk& c : m_chunks)
        {
            const Vec3 middle((static_cast<float>(c.x) + 0.5f) * size, (c.low + c.high) * 0.5f, (static_cast<float>(c.z) + 0.5f) * size);
            const float tall = (c.high - c.low) * 0.5f;
            const float radius = sqrtf(size * size * 0.5f + tall * tall);
            const Vec3 d = middle - view.eye;
            const float far = fmaxf(0, sqrtf(d.dot(d)) - radius);
            if (shadow ? far > m_shadowDistance * 2 : !frustum.sees(middle, radius))
            {
                continue;
            }
            if (c.changed)
            {
                if (m_landSquares == 0 && !c.mesh && made >= 2 && far > size)
                {
                    continue;
                }
                buildChunk(c);
                made++;
            }
            const u32 detail = far < size * 1.2f ? 0 : far < size * 3 ? 1 : 2;
            gpu::MeshDraw draw;
            draw.mesh = c.mesh;
            draw.texture = m_solids->grain();
            draw.repeat = true;
            draw.lines = m_lines && !shadow;
            draw.first = first[detail];
            draw.count = first[detail + 1] - first[detail];
            if (draw.lines)
            {
                // Without the skirt, which comes last.
                const u32 step = 1u << (detail * 2 > 3 ? 3 : detail * 2);
                draw.count -= 4 * kChunk / step * 6;
            }
            pickLights(view, middle, radius, draw);
            m_solids->device()->drawMesh(draw);
        }
    }

    void Space::drawWater(const gpu::View& view)
    {
        if (!m_hasWater || m_lines)
        {
            return;
        }
        constexpr u32 squares = 40;
        MeshData& d = m_waterData;
        if (d.vertices.count() == 0)
        {
            d.plane(1, 1, squares, { 1, 1, 1, 1 });
        }
        // Closer together near the camera, and reaching as far as can be seen.
        const float reach = m_far * 0.9f;
        struct Wave
        {
            float at, far, sin, cos, slowSin, slowCos;
        };
        Wave across[squares + 1], along[squares + 1];
        for (u32 i = 0; i <= squares; i++)
        {
            const float t = static_cast<float>(i) / static_cast<float>(squares) * 2 - 1;
            Wave &a = across[i], &b = along[i];
            a.far = b.far = t * fabsf(t) * reach;
            a.at = floorf(view.eye.x) + a.far;
            b.at = floorf(view.eye.z) + b.far;
            a.sin = sinf(a.at * 0.7f + m_time * 1.3f);
            a.cos = cosf(a.at * 0.7f + m_time * 1.3f);
            b.sin = sinf(b.at * 0.9f - m_time * 1.1f);
            b.cos = cosf(b.at * 0.9f - m_time * 1.1f);
            a.slowSin = sinf(a.at * 0.35f + m_time * 0.8f);
            a.slowCos = cosf(a.at * 0.35f + m_time * 0.8f);
            b.slowSin = sinf(b.at * 0.35f);
            b.slowCos = cosf(b.at * 0.35f);
        }
        for (u32 i = 0; i < d.vertices.count(); i++)
        {
            gpu::MeshVertex& v = d.vertices[i];
            const Wave &p = across[i % (squares + 1)], &q = along[i / (squares + 1)];
            const float wx = p.at, wz = q.at;
            const float calm = clampFloat(1 - (fabsf(p.far) + fabsf(q.far)) / (reach * 0.35f), 0, 1);
            // The sine and cosine of the sum of two angles.
            const float sinC = p.slowSin * q.slowCos + p.slowCos * q.slowSin, cosC = p.slowCos * q.slowCos - p.slowSin * q.slowSin;
            v.x = wx;
            v.z = wz;
            v.y = m_water + (p.sin + q.sin + sinC * 1.5f) * 0.05f * calm;
            const Vec3 n = Vec3(-(p.cos * 0.7f + cosC * 0.52f) * 0.09f * calm, 1, -(q.cos * 0.9f + cosC * 0.52f) * 0.09f * calm).normalized();
            v.nx = n.x;
            v.ny = n.y;
            v.nz = n.z;
        }
        gpu::Device& device = *m_solids->device();
        if (m_waterMesh)
        {
            device.updateMesh(m_waterMesh, d.vertices.begin(), d.vertices.count());
        }
        else
        {
            m_waterMesh = device.createMesh(d.vertices.begin(), d.vertices.count(), d.indices.begin(), d.indices.count(), true);
        }
        gpu::MeshDraw draw;
        draw.mesh = m_waterMesh;
        draw.color = m_waterColor;
        draw.blend = m_waterColor.a < 1 ? gpu::Blend::Alpha : gpu::Blend::None;
        draw.shine = 0.8f;
        draw.cull = gpu::Cull::None;
        pickLights(view, { view.eye.x, m_water, view.eye.z }, 30, draw);
        device.drawMesh(draw);
    }

    // --- Shapes by their distance ---

    float Space::fieldDistance(const gpu::Field& field, const Vec3& at, gpu::Color* color)
    {
        FieldTurns turns;
        turnField(field, turns);
        return fieldDistance(field, turns, at, color);
    }

    void Space::turnField(const gpu::Field& field, FieldTurns& turns)
    {
        for (u32 i = 0; i < field.count && i < gpu::kFieldShapes; i++)
        {
            turns.cosine[i] = cosf(field.shapes[i].turn);
            turns.sine[i] = sinf(field.shapes[i].turn);
        }
    }

    float Space::fieldDistance(const gpu::Field& field, const FieldTurns& turns, const Vec3& at, gpu::Color* color)
    {
        Vec3 p = at;
        if (field.repeat > 0)
        {
            p.x -= field.repeat * floorf(p.x / field.repeat + 0.5f);
            p.z -= field.repeat * floorf(p.z / field.repeat + 0.5f);
        }
        float found = 500;
        gpu::Color mixed = { 0, 0, 0, 1 };
        for (u32 i = 0; i < field.count && i < gpu::kFieldShapes; i++)
        {
            const gpu::FieldShape& s = field.shapes[i];
            Vec3 d = p - s.place;
            float far = 0;
            if (s.kind == gpu::FieldKind::Sphere)
            {
                // Too far away to make any difference: the usual answer, and found without a square root.
                const float reach = found + s.size.x + s.soft;
                const float square = d.dot(d);
                if (s.operation == gpu::FieldOperation::Join && reach > 0 && square > reach * reach)
                {
                    continue;
                }
                far = sqrtf(square) - s.size.x;
            }
            else
            {
                const float c = turns.cosine[i], n = turns.sine[i];
                d = { d.x * c - d.z * n, d.y, d.x * n + d.z * c };
                switch (s.kind)
                {
                case gpu::FieldKind::Box:
                {
                    const Vec3 q(fabsf(d.x) - s.size.x, fabsf(d.y) - s.size.y, fabsf(d.z) - s.size.z);
                    far = Vec3(fmaxf(q.x, 0), fmaxf(q.y, 0), fmaxf(q.z, 0)).length() + fminf(fmaxf(q.x, fmaxf(q.y, q.z)), 0) - 0.03f;
                    break;
                }
                case gpu::FieldKind::Torus:
                {
                    const float ring = sqrtf(d.x * d.x + d.z * d.z) - s.size.x;
                    far = sqrtf(ring * ring + d.y * d.y) - s.size.y;
                    break;
                }
                case gpu::FieldKind::Capsule:
                    d.y -= clampFloat(d.y, -s.size.y, s.size.y);
                    far = d.length() - s.size.x;
                    break;
                case gpu::FieldKind::Cylinder:
                {
                    const float wx = sqrtf(d.x * d.x + d.z * d.z) - s.size.x, wy = fabsf(d.y) - s.size.y;
                    far = fminf(fmaxf(wx, wy), 0) + sqrtf(fmaxf(wx, 0) * fmaxf(wx, 0) + fmaxf(wy, 0) * fmaxf(wy, 0));
                    break;
                }
                default: far = d.y; break;
                }
            }
            const float soft = s.soft > 0.0001f ? s.soft : 0.0001f;
            // Joined is the nearest of the two; cut and kept are the farthest.
            const float sign = s.operation == gpu::FieldOperation::Join ? 1.0f : -1.0f;
            far = s.operation == gpu::FieldOperation::Cut ? -far : far;
            const float h = clampFloat(0.5f + 0.5f * sign * (found - far) / soft, 0, 1);
            found = found + (far - found) * h - sign * soft * h * (1 - h);
            if (color && s.operation != gpu::FieldOperation::Keep)
            {
                mixed = mix(mixed, s.color, h);
            }
        }
        if (color)
        {
            *color = mixed;
        }
        return found;
    }

    // Surface nets: a vertex in each cell that the surface passes through, joined to its neighbours'.
    void Space::fieldMesh(const gpu::Field& field, const Vec3& middle, float half, u32 cells, MeshData& out)
    {
        out.clear();
        if (cells < 2 || cells > 60 || !(half > 0))
        {
            return;
        }
        const u32 points = cells + 1;
        GrowArray<float, MemTag::Frame> values;
        GrowArray<int, MemTag::Frame> made;
        if (!values.reserve(points * points * points) || !made.reserve(cells * cells * cells))
        {
            return;
        }
        const float step = 2 * half / static_cast<float>(cells);
        const Vec3 corner = middle - Vec3(half, half, half);
        const auto placeOf = [&](u32 x, u32 y, u32 z) { return corner + Vec3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)) * step; };
        // In blocks: where the middle of one is farther from everything than its corners are from its
        // middle, there is no surface in it, and only the sign matters.
        constexpr u32 kBlock = 4;
        FieldTurns turns;
        turnField(field, turns);
        values.resize(points * points * points);
        const float reach = step * static_cast<float>(kBlock) * 0.8661f;
        for (u32 bz = 0; bz < points; bz += kBlock)
        {
            for (u32 by = 0; by < points; by += kBlock)
            {
                for (u32 bx = 0; bx < points; bx += kBlock)
                {
                    const float inside = static_cast<float>(kBlock - 1) * 0.5f;
                    const float here = fieldDistance(field, turns, placeOf(bx, by, bz) + Vec3(inside, inside, inside) * step, nullptr);
                    const bool empty = fabsf(here) > reach;
                    for (u32 z = bz; z < bz + kBlock && z < points; z++)
                    {
                        for (u32 y = by; y < by + kBlock && y < points; y++)
                        {
                            for (u32 x = bx; x < bx + kBlock && x < points; x++)
                            {
                                values[(z * points + y) * points + x] = empty ? here : fieldDistance(field, turns, placeOf(x, y, z), nullptr);
                            }
                        }
                    }
                }
            }
        }
        const auto valueAt = [&](u32 x, u32 y, u32 z) { return values[(z * points + y) * points + x]; };
        for (u32 z = 0; z < cells; z++)
        {
            for (u32 y = 0; y < cells; y++)
            {
                for (u32 x = 0; x < cells; x++)
                {
                    u32 inside = 0;
                    float v[8];
                    for (u32 k = 0; k < 8; k++)
                    {
                        v[k] = valueAt(x + (k & 1), y + ((k >> 1) & 1), z + (k >> 2));
                        inside += v[k] < 0;
                    }
                    if (inside == 0 || inside == 8)
                    {
                        made.push(-1);
                        continue;
                    }
                    // The middle of where the edges are crossed.
                    Vec3 sum;
                    float count = 0;
                    static const u8 edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
                    for (const auto& e : edges)
                    {
                        const float a = v[e[0]], b = v[e[1]];
                        if ((a < 0) != (b < 0))
                        {
                            const float t = a / (a - b);
                            const Vec3 pa(static_cast<float>(e[0] & 1), static_cast<float>((e[0] >> 1) & 1), static_cast<float>(e[0] >> 2));
                            const Vec3 pb(static_cast<float>(e[1] & 1), static_cast<float>((e[1] >> 1) & 1), static_cast<float>(e[1] >> 2));
                            sum = sum + pa + (pb - pa) * t;
                            count += 1;
                        }
                    }
                    const Vec3 at = placeOf(x, y, z) + sum / count * step;
                    gpu::Color color;
                    fieldDistance(field, turns, at, &color);
                    // Which way it goes up, from the corners.
                    const Vec3 normal = Vec3(v[1] + v[3] + v[5] + v[7] - v[0] - v[2] - v[4] - v[6], v[2] + v[3] + v[6] + v[7] - v[0] - v[1] - v[4] - v[5], v[4] + v[5] + v[6] + v[7] - v[0] - v[1] - v[2] - v[3]).normalized();
                    made.push(out.vertex(at, normal, color, 0, 0) ? static_cast<int>(out.vertices.count() - 1) : -1);
                }
            }
        }
        const auto madeAt = [&](u32 x, u32 y, u32 z) { return made[(z * cells + y) * cells + x]; };
        // Across each edge of the grid that the surface crosses go four vertices.
        for (u32 z = 1; z < cells; z++)
        {
            for (u32 y = 1; y < cells; y++)
            {
                for (u32 x = 1; x < cells; x++)
                {
                    const float here = valueAt(x, y, z);
                    const auto join = [&](float there, int a, int b, int c, int d)
                    {
                        if ((here < 0) != (there < 0) && a >= 0 && b >= 0 && c >= 0 && d >= 0)
                        {
                            const u32 q[4] = { static_cast<u32>(a), static_cast<u32>(here < 0 ? b : d), static_cast<u32>(c), static_cast<u32>(here < 0 ? d : b) };
                            out.triangle(q[0], q[1], q[2]);
                            out.triangle(q[0], q[2], q[3]);
                        }
                    };
                    join(valueAt(x + 1, y, z), madeAt(x, y - 1, z - 1), madeAt(x, y, z - 1), madeAt(x, y, z), madeAt(x, y - 1, z));
                    join(valueAt(x, y + 1, z), madeAt(x - 1, y, z - 1), madeAt(x - 1, y, z), madeAt(x, y, z), madeAt(x, y, z - 1));
                    join(valueAt(x, y, z + 1), madeAt(x - 1, y - 1, z), madeAt(x, y - 1, z), madeAt(x, y, z), madeAt(x - 1, y, z));
                }
            }
        }
    }
}
