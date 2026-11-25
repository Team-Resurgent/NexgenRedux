#include "scene/space.h"

#include <math.h>
#include <string.h>

// Changes to the land by hand: how much each corner was raised, and what it was painted.
namespace nx
{
    namespace
    {
        constexpr int kWidestBrush = 48;   // squares, from the middle
        constexpr float kFarthest = 300;   // metres, either way

        float clamped(float v, float low, float high)
        {
            return v < low ? low : v > high ? high : v;
        }

        template <typename T, MemTag Tag>
        bool zeros(GrowArray<T, Tag>& a, u32 count)
        {
            if (!a.reserve(count))
            {
                return false;
            }
            a.resize(count);
            memset(a.begin(), 0, count * sizeof(T));
            return true;
        }

        // Full in the middle and nothing at the edge.
        float falloff(float far, float radius)
        {
            const float t = 1 - far * far / (radius * radius);
            return t > 0 ? t * t : 0;
        }

        u8 byteOf(float v)
        {
            return static_cast<u8>(clamped(v, 0, 1) * 255 + 0.5f);
        }

        const char kLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string toText(const std::string& bytes)
        {
            std::string out;
            out.reserve(bytes.size() * 4 / 3 + 4);
            u32 held = 0;
            int bits = 0;
            for (const char c : bytes)
            {
                held = (held << 8) | static_cast<u8>(c);
                for (bits += 8; bits >= 6; bits -= 6)
                {
                    out += kLetters[(held >> (bits - 6)) & 63];
                }
            }
            if (bits)
            {
                out += kLetters[(held << (6 - bits)) & 63];
            }
            return out;
        }

        bool fromText(const std::string& text, std::string& out)
        {
            u32 held = 0;
            int bits = 0;
            for (const char c : text)
            {
                const char* at = c ? strchr(kLetters, c) : nullptr;
                if (at == nullptr)
                {
                    return false;
                }
                held = (held << 6) | static_cast<u32>(at - kLetters);
                bits += 6;
                if (bits >= 8)
                {
                    bits -= 8;
                    out += static_cast<char>((held >> bits) & 255);
                }
            }
            return true;
        }

        void putNumber(std::string& out, u32 v)
        {
            for (; v >= 128; v >>= 7)
            {
                out += static_cast<char>(v | 128);
            }
            out += static_cast<char>(v);
        }

        struct Bytes
        {
            const std::string& from;
            size_t at;
            bool ok;

            u8 byte()
            {
                ok = ok && at < from.size();
                return ok ? static_cast<u8>(from[at++]) : 0;
            }

            u32 number()
            {
                u32 v = 0;
                for (u32 shift = 0; shift < 35; shift += 7)
                {
                    const u8 b = byte();
                    v |= static_cast<u32>(b & 127) << shift;
                    if (!(b & 128))
                    {
                        return v;
                    }
                }
                ok = false;
                return 0;
            }
        };
    }

    // How much a corner was raised by hand. Corners are numbered from the middle.
    float Space::sculpted(int x, int z) const
    {
        const int half = static_cast<int>(m_landSquares) / 2;
        if (m_sculpt.count() == 0 || x < -half || z < -half || x > half || z > half)
        {
            return 0;
        }
        return m_sculpt[static_cast<u32>(z + half) * (m_landSquares + 1) + static_cast<u32>(x + half)];
    }

    const u8* Space::painted(int x, int z) const
    {
        const int half = static_cast<int>(m_landSquares) / 2;
        if (m_paint.count() == 0 || x < -half || z < -half || x > half || z > half)
        {
            return nullptr;
        }
        return &m_paint[(static_cast<u32>(z + half) * (m_landSquares + 1) + static_cast<u32>(x + half)) * 4];
    }

    // Squares, by the corner that is least.
    bool Space::isHole(int x, int z) const
    {
        const int half = static_cast<int>(m_landSquares) / 2;
        return m_holes.count() && x >= -half && z >= -half && x < half && z < half && m_holes[static_cast<u32>(z + half) * m_landSquares + static_cast<u32>(x + half)];
    }

    void Space::changeHoles(int fromX, int fromZ, int toX, int toZ)
    {
        for (Chunk& c : m_chunks)
        {
            c.changed = c.changed || (c.x * 32 <= toX && c.x * 32 + 32 > fromX && c.z * 32 <= toZ && c.z * 32 + 32 > fromZ);
        }
        for (Body3& b : m_bodies)
        {
            b.still = 0;
        }
    }

    bool Space::hole(float x, float z, float radius, bool open)
    {
        Brush b;
        if (!brush(x, z, radius, b))
        {
            return false;
        }
        if (m_holes.count() == 0 && (!open || !zeros(m_holes, m_landSquares * m_landSquares)))
        {
            return !open;
        }
        const int half = static_cast<int>(m_landSquares) / 2;
        for (int sz = b.fromZ - 1; sz <= b.toZ; sz++)
        {
            for (int sx = b.fromX - 1; sx <= b.toX; sx++)
            {
                const float dx = static_cast<float>(sx) + 0.5f - b.x, dz = static_cast<float>(sz) + 0.5f - b.z;
                if (sx >= -half && sz >= -half && sx < half && sz < half && dx * dx + dz * dz <= b.radius * b.radius)
                {
                    m_holes[static_cast<u32>(sz + half) * m_landSquares + static_cast<u32>(sx + half)] = open ? 1 : 0;
                }
            }
        }
        changeHoles(b.fromX - 1, b.fromZ - 1, b.toX, b.toZ);
        return true;
    }

    // Walls at the sides of the land.
    void Space::keepIn(Vec3& place, Vec3& velocity, const Vec3& reach, float bounce) const
    {
        if (!m_edges || !m_landOn || m_landSquares == 0)
        {
            return;
        }
        const float half = landSize() * 0.5f;
        const auto keep = [&](float& at, float& speed, float out)
        {
            const float limit = half > out ? half - out : 0;
            if (at > limit || at < -limit)
            {
                speed = (at > 0) == (speed > 0) ? -speed * bounce : speed;
                at = at > 0 ? limit : -limit;
            }
        };
        keep(place.x, velocity.x, reach.x);
        keep(place.z, velocity.z, reach.z);
    }

    bool Space::brush(float x, float z, float radius, Brush& b)
    {
        if (!m_landOn || m_landSquares == 0 || !(radius > 0) || !(x > -1e5f && x < 1e5f && z > -1e5f && z < 1e5f))
        {
            return false;
        }
        const int half = static_cast<int>(m_landSquares) / 2;
        const float gx = x / m_landSpacing, gz = z / m_landSpacing;
        b.radius = clamped(radius / m_landSpacing, 0.75f, static_cast<float>(kWidestBrush));
        b.x = gx;
        b.z = gz;
        const auto within = [&](float v) { return static_cast<int>(clamped(v, static_cast<float>(-half), static_cast<float>(half))); };
        b.fromX = within(ceilf(gx - b.radius));
        b.toX = within(floorf(gx + b.radius));
        b.fromZ = within(ceilf(gz - b.radius));
        b.toZ = within(floorf(gz + b.radius));
        const float end = static_cast<float>(half) + b.radius;
        return fabsf(gx) < end && fabsf(gz) < end && b.fromX <= b.toX && b.fromZ <= b.toZ;
    }

    bool Space::makeSculpt()
    {
        const u32 corners = (m_landSquares + 1) * (m_landSquares + 1);
        return m_sculpt.count() == corners || zeros(m_sculpt, corners);
    }

    // How it moves each corner, from what is there now; and then all that have those corners.
    template <typename F>
    bool Space::change(float x, float z, float radius, const F& by)
    {
        Brush b;
        if (!brush(x, z, radius, b) || !makeSculpt())
        {
            return false;
        }
        const u32 wide = static_cast<u32>(b.toX - b.fromX + 1), deep = static_cast<u32>(b.toZ - b.fromZ + 1);
        GrowArray<float> moved;
        if (!zeros(moved, wide * deep))
        {
            return false;
        }
        for (int cz = b.fromZ; cz <= b.toZ; cz++)
        {
            for (int cx = b.fromX; cx <= b.toX; cx++)
            {
                const float dx = static_cast<float>(cx) - b.x, dz = static_cast<float>(cz) - b.z;
                const float much = falloff(sqrtf(dx * dx + dz * dz), b.radius);
                const float was = sculpted(cx, cz);
                const float want = much > 0 ? was + by(cx, cz, corner(cx, cz), much) : was;
                moved[static_cast<u32>(cz - b.fromZ) * wide + static_cast<u32>(cx - b.fromX)] = clamped(want, -kFarthest, kFarthest) - was;
            }
        }
        moveCorners(b.fromX, b.fromZ, b.toX, b.toZ, moved.begin());
        return true;
    }

    void Space::moveCorners(int fromX, int fromZ, int toX, int toZ, const float* moved)
    {
        const int half = static_cast<int>(m_landSquares) / 2;
        const u32 wide = static_cast<u32>(toX - fromX + 1);
        for (int z = fromZ; z <= toZ; z++)
        {
            for (int x = fromX; x <= toX; x++)
            {
                m_sculpt[static_cast<u32>(z + half) * (m_landSquares + 1) + static_cast<u32>(x + half)] += moved[static_cast<u32>(z - fromZ) * wide + static_cast<u32>(x - fromX)];
            }
        }
        for (Chunk& c : m_chunks)
        {
            // Those one beyond its edges too, which its slopes come from.
            const int left = c.x * 32 - 1, near = c.z * 32 - 1;
            const int last = static_cast<int>(kRow) - 1;
            const int x0 = fromX > left ? fromX : left, x1 = toX < left + last ? toX : left + last;
            const int z0 = fromZ > near ? fromZ : near, z1 = toZ < near + last ? toZ : near + last;
            if (x0 > x1 || z0 > z1)
            {
                continue;
            }
            for (int z = z0; z <= z1; z++)
            {
                for (int x = x0; x <= x1; x++)
                {
                    c.heights[static_cast<u32>(z - near) * kRow + static_cast<u32>(x - left)] += moved[static_cast<u32>(z - fromZ) * wide + static_cast<u32>(x - fromX)];
                }
            }
            c.changed = true;
            c.low = 1e9f;
            c.high = -1e9f;
            for (u32 i = 0; i < kRow * kRow; i++)
            {
                c.low = fminf(c.low, c.heights[i]);
                c.high = fmaxf(c.high, c.heights[i]);
            }
        }
        // What rested there may not any more.
        for (Body3& b : m_bodies)
        {
            b.still = 0;
        }
    }

    bool Space::raise(float x, float z, float radius, float by)
    {
        return change(x, z, radius, [&](int, int, float, float much) { return by * much; });
    }

    bool Space::smooth(float x, float z, float radius, float amount)
    {
        amount = clamped(amount, 0, 1);
        return change(x, z, radius, [&](int cx, int cz, float height, float much)
        {
            const float round = (corner(cx - 1, cz) + corner(cx + 1, cz) + corner(cx, cz - 1) + corner(cx, cz + 1)) * 0.25f;
            return (round - height) * amount * much;
        });
    }

    bool Space::level(float x, float z, float radius, float height, float amount)
    {
        amount = clamped(amount, 0, 1);
        // Its middle is flat, unlike the others'.
        return change(x, z, radius, [&](int, int, float now, float much) { return (height - now) * amount * clamped(much * 2, 0, 1); });
    }

    bool Space::paint(float x, float z, float radius, const gpu::Color& color, float amount)
    {
        Brush b;
        if (!brush(x, z, radius, b))
        {
            return false;
        }
        const u32 bytes = (m_landSquares + 1) * (m_landSquares + 1) * 4;
        if (m_paint.count() != bytes && !zeros(m_paint, bytes))
        {
            return false;
        }
        const int half = static_cast<int>(m_landSquares) / 2;
        amount = clamped(amount, 0, 1);
        for (int cz = b.fromZ; cz <= b.toZ; cz++)
        {
            for (int cx = b.fromX; cx <= b.toX; cx++)
            {
                const float dx = static_cast<float>(cx) - b.x, dz = static_cast<float>(cz) - b.z;
                const float much = amount * clamped(falloff(sqrtf(dx * dx + dz * dz), b.radius) * 2, 0, 1);
                if (much < 0.002f)
                {
                    continue;
                }
                u8* p = &m_paint[(static_cast<u32>(cz + half) * (m_landSquares + 1) + static_cast<u32>(cx + half)) * 4];
                const float was = static_cast<float>(p[3]) / 255;
                if (color.a <= 0)
                {
                    // Wiped away, and always by something.
                    const float now = clamped(was - much, 0, 1);
                    p[3] = byteOf(now) == p[3] && p[3] ? static_cast<u8>(p[3] - 1) : byteOf(now);
                    if (p[3] == 0)
                    {
                        p[0] = p[1] = p[2] = 0;
                    }
                    continue;
                }
                const float now = clamped(was + (1 - was) * much * color.a, 0, 1);
                const float share = now > 0 ? clamped(much * color.a / now, 0, 1) : 1;
                const float to[3] = { color.r, color.g, color.b };
                for (u32 i = 0; i < 3; i++)
                {
                    const u8 target = byteOf(to[i]);
                    const u8 next = byteOf(static_cast<float>(p[i]) / 255 * (1 - share) + to[i] * share);
                    p[i] = next == p[i] && next != target ? static_cast<u8>(next < target ? next + 1 : next - 1) : next;
                }
                p[3] = byteOf(now) == p[3] && p[3] < 255 ? static_cast<u8>(p[3] + 1) : byteOf(now);
            }
        }
        const float size = regionSize(), reach = (b.radius + 1) * m_landSpacing;
        for (Chunk& c : m_chunks)
        {
            const float lx = static_cast<float>(c.x) * size, lz = static_cast<float>(c.z) * size;
            c.changed = c.changed || (x + reach > lx && x - reach < lx + size && z + reach > lz && z - reach < lz + size);
        }
        return true;
    }

    // "1", the squares, then heights in centimetres, each from the one before, and paint; both with runs made
    // short. As letters, so that it can be in JSON.
    std::string Space::landEdits() const
    {
        bool raised = false, colored = false;
        for (const float h : m_sculpt)
        {
            raised = raised || fabsf(h) >= 0.005f;
        }
        for (u32 i = 3; i < m_paint.count(); i += 4)
        {
            colored = colored || m_paint[i];
        }
        bool open = false;
        for (const u8 h : m_holes)
        {
            open = open || h;
        }
        if (!raised && !colored && !open)
        {
            return "";
        }
        std::string out;
        out += 1;
        putNumber(out, m_landSquares);
        out += static_cast<char>((raised ? 1 : 0) | (colored ? 2 : 0) | (open ? 4 : 0));
        if (raised)
        {
            int last = 0;
            u32 run = 0;
            const auto endRun = [&]
            {
                if (run)
                {
                    putNumber(out, 0);
                    putNumber(out, run);
                    run = 0;
                }
            };
            for (const float h : m_sculpt)
            {
                const int now = static_cast<int>(floorf(h * 100 + 0.5f));
                const int by = now - last;
                last = now;
                if (by == 0)
                {
                    run++;
                    continue;
                }
                endRun();
                putNumber(out, by > 0 ? static_cast<u32>(by) * 2 : static_cast<u32>(-by) * 2 - 1);
            }
            endRun();
        }
        for (u32 i = 0; colored && i < m_paint.count();)
        {
            u32 run = 1;
            while (i + run * 4 < m_paint.count() && memcmp(&m_paint[i], &m_paint[i + run * 4], 4) == 0)
            {
                run++;
            }
            putNumber(out, run);
            for (u32 k = 0; k < 4; k++)
            {
                out += static_cast<char>(m_paint[i + k]);
            }
            i += run * 4;
        }
        // How many squares are closed, then open, and so on.
        for (u32 i = 0, now = 0; open && i < m_holes.count(); now ^= 1)
        {
            u32 run = 0;
            while (i < m_holes.count() && m_holes[i] == now)
            {
                run++;
                i++;
            }
            putNumber(out, run);
        }
        return toText(out);
    }

    bool Space::setLandEdits(const std::string& text)
    {
        if (!m_landOn || m_landSquares == 0)
        {
            return false;
        }
        const u32 corners = (m_landSquares + 1) * (m_landSquares + 1);
        GrowArray<float> heights;
        GrowArray<u8> paint, holes;
        if (!text.empty())
        {
            std::string data;
            if (!fromText(text, data))
            {
                return false;
            }
            Bytes in = { data, 0, true };
            const bool ours = in.byte() == 1 && in.number() == m_landSquares;
            const u8 what = in.byte();
            if (!ours || !in.ok || (what & 1 && !zeros(heights, corners)) || (what & 2 && !zeros(paint, corners * 4)) || (what & 4 && !zeros(holes, m_landSquares * m_landSquares)))
            {
                return false;
            }
            int last = 0;
            for (u32 i = 0; i < heights.count() && in.ok;)
            {
                const u32 v = in.number();
                const u32 run = v ? 1 : in.number();
                if (run == 0 || run > heights.count() - i || v > 200000)
                {
                    return false;
                }
                last += v & 1 ? -static_cast<int>(v / 2) - 1 : static_cast<int>(v / 2);
                last = last < -30000 ? -30000 : last > 30000 ? 30000 : last;
                for (u32 k = 0; k < run; k++)
                {
                    heights[i++] = static_cast<float>(last) * 0.01f;
                }
            }
            for (u32 i = 0; i < paint.count() && in.ok;)
            {
                const u32 run = in.number();
                u8 c[4];
                for (u8& part : c)
                {
                    part = in.byte();
                }
                if (run == 0 || run > (paint.count() - i) / 4)
                {
                    return false;
                }
                for (u32 k = 0; k < run; k++, i += 4)
                {
                    memcpy(&paint[i], c, 4);
                }
            }
            for (u32 i = 0, now = 0; i < holes.count() && in.ok; now ^= 1)
            {
                const u32 run = in.number();
                if (run > holes.count() - i || (run == 0 && i))
                {
                    return false;
                }
                for (u32 k = 0; k < run; k++)
                {
                    holes[i++] = static_cast<u8>(now);
                }
            }
            if (!in.ok || in.at != data.size())
            {
                return false;
            }
        }
        if (heights.count() || m_sculpt.count())
        {
            GrowArray<float> moved;
            if (!makeSculpt() || !zeros(moved, corners))
            {
                return false;
            }
            for (u32 i = 0; i < corners; i++)
            {
                moved[i] = (heights.count() ? heights[i] : 0) - m_sculpt[i];
            }
            const int half = static_cast<int>(m_landSquares) / 2;
            moveCorners(-half, -half, half, half, moved.begin());
        }
        if (holes.count() || m_holes.count())
        {
            if (holes.count())
            {
                m_holes.swap(holes);
            }
            else
            {
                m_holes.clear();
            }
            changeHoles(-200, -200, 200, 200);
        }
        if (paint.count() || m_paint.count())
        {
            if (!zeros(m_paint, corners * 4))
            {
                return false;
            }
            if (paint.count())
            {
                memcpy(m_paint.begin(), paint.begin(), corners * 4);
            }
            for (Chunk& c : m_chunks)
            {
                c.changed = true;
            }
        }
        return true;
    }
}
