#include "scene/space.h"

#include <math.h>
#include <string.h>

// Changes to the land by hand: how much each corner was raised, what it was painted, and where it is open.
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

    // --- Where what was done by hand is kept: by region of 32 squares, whether the land there is now or not ---

    Space::Edit* Space::editAt(int rx, int rz, bool make)
    {
        if (m_lastEdit && m_lastEdit->x == rx && m_lastEdit->z == rz)
        {
            return m_lastEdit;
        }
        for (Edit* e : m_edits)
        {
            if (e->x == rx && e->z == rz)
            {
                return m_lastEdit = e;
            }
        }
        if (!make || m_edits.count() >= kMostEdits)
        {
            return nullptr;
        }
        Edit* e = static_cast<Edit*>(memAlloc(sizeof(Edit), MemTag::Scene));
        if (e == nullptr || !m_edits.push(e))
        {
            memFree(e);
            return nullptr;
        }
        memset(e, 0, sizeof(Edit));
        e->x = rx;
        e->z = rz;
        return m_lastEdit = e;
    }

    const Space::Edit* Space::editAt(int rx, int rz) const
    {
        return const_cast<Space*>(this)->editAt(rx, rz, false);
    }

    void Space::clearEdits()
    {
        for (Edit* e : m_edits)
        {
            memFree(e);
        }
        m_edits.clear();
        m_lastEdit = nullptr;
        m_anyHoles = false;
    }

    // Corners and squares are numbered from the middle; each region has the 32 from its least one.
    float Space::sculpted(int x, int z) const
    {
        const Edit* e = m_edits.count() ? editAt(x >> 5, z >> 5) : nullptr;
        return e ? e->raised[(z & 31) * 32 + (x & 31)] : 0;
    }

    const u8* Space::painted(int x, int z) const
    {
        const Edit* e = m_edits.count() ? editAt(x >> 5, z >> 5) : nullptr;
        const u8* p = e ? &e->paint[((z & 31) * 32 + (x & 31)) * 4] : nullptr;
        return p && p[3] ? p : nullptr;
    }

    bool Space::isHole(int x, int z) const
    {
        if (!m_anyHoles)
        {
            return false;
        }
        const int half = static_cast<int>(m_landSquares) / 2;
        if (m_landSquares && (x < -half || z < -half || x >= half || z >= half))
        {
            return false;
        }
        const Edit* e = editAt(x >> 5, z >> 5);
        return e && e->holes[(z & 31) * 32 + (x & 31)];
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

    // Corners that a brush reaches, on the land there is. Endless land has an end too, far enough away.
    bool Space::brush(float x, float z, float radius, Brush& b)
    {
        if (!m_landOn || !(radius > 0) || !(x > -1e5f && x < 1e5f && z > -1e5f && z < 1e5f))
        {
            return false;
        }
        const int half = m_landSquares ? static_cast<int>(m_landSquares) / 2 : 30000;
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

    // How it moves each corner, from what is there now; and then all that have those corners.
    template <typename F>
    bool Space::change(float x, float z, float radius, const F& by)
    {
        Brush b;
        if (!brush(x, z, radius, b))
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
        return moveCorners(b.fromX, b.fromZ, b.toX, b.toZ, moved.begin());
    }

    bool Space::moveCorners(int fromX, int fromZ, int toX, int toZ, const float* moved)
    {
        const u32 wide = static_cast<u32>(toX - fromX + 1);
        bool room = true;
        for (int z = fromZ; z <= toZ; z++)
        {
            for (int x = fromX; x <= toX; x++)
            {
                const float by = moved[static_cast<u32>(z - fromZ) * wide + static_cast<u32>(x - fromX)];
                Edit* e = by != 0 ? editAt(x >> 5, z >> 5, true) : nullptr;
                room = room && (by == 0 || e);
                if (e)
                {
                    e->raised[(z & 31) * 32 + (x & 31)] += by;
                }
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
        return room;
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
                Edit* e = editAt(cx >> 5, cz >> 5, color.a > 0);
                if (e == nullptr)
                {
                    continue;
                }
                u8* p = &e->paint[((cz & 31) * 32 + (cx & 31)) * 4];
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

    bool Space::hole(float x, float z, float radius, bool open)
    {
        Brush b;
        if (!brush(x, z, radius, b))
        {
            return false;
        }
        const int half = m_landSquares ? static_cast<int>(m_landSquares) / 2 : 30000;
        for (int sz = b.fromZ - 1; sz <= b.toZ; sz++)
        {
            for (int sx = b.fromX - 1; sx <= b.toX; sx++)
            {
                const float dx = static_cast<float>(sx) + 0.5f - b.x, dz = static_cast<float>(sz) + 0.5f - b.z;
                if (sx < -half || sz < -half || sx >= half || sz >= half || dx * dx + dz * dz > b.radius * b.radius)
                {
                    continue;
                }
                if (Edit* e = editAt(sx >> 5, sz >> 5, open))
                {
                    e->holes[(sz & 31) * 32 + (sx & 31)] = open ? 1 : 0;
                    m_anyHoles = m_anyHoles || open;
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

    // --- As text, to keep ---

    namespace
    {
        // Each, then the next less it, in runs: heights in centimetres, paint as it is, and which squares are open.
        void putRegion(std::string& out, const float* raised, const u8* paint, const u8* holes, u32 count)
        {
            bool isRaised = false, isPainted = false, isOpen = false;
            for (u32 i = 0; i < count; i++)
            {
                isRaised = isRaised || fabsf(raised[i]) >= 0.005f;
                isPainted = isPainted || paint[i * 4 + 3];
                isOpen = isOpen || holes[i];
            }
            out += static_cast<char>((isRaised ? 1 : 0) | (isPainted ? 2 : 0) | (isOpen ? 4 : 0));
            if (isRaised)
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
                for (u32 i = 0; i < count; i++)
                {
                    const int now = static_cast<int>(floorf(raised[i] * 100 + 0.5f));
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
            for (u32 i = 0; isPainted && i < count;)
            {
                u32 run = 1;
                while (i + run < count && memcmp(&paint[i * 4], &paint[(i + run) * 4], 4) == 0)
                {
                    run++;
                }
                putNumber(out, run);
                out.append(reinterpret_cast<const char*>(&paint[i * 4]), 4);
                i += run;
            }
            for (u32 i = 0, now = 0; isOpen && i < count; now ^= 1)
            {
                u32 run = 0;
                while (i < count && (holes[i] != 0) == (now != 0))
                {
                    run++;
                    i++;
                }
                putNumber(out, run);
            }
        }

        bool getRegion(Bytes& in, float* raised, u8* paint, u8* holes, u32 count)
        {
            const u8 what = in.byte();
            int last = 0;
            for (u32 i = 0; what & 1 && i < count && in.ok;)
            {
                const u32 v = in.number();
                const u32 run = v ? 1 : in.number();
                if (run == 0 || run > count - i || v > 200000)
                {
                    return false;
                }
                last += v & 1 ? -static_cast<int>(v / 2) - 1 : static_cast<int>(v / 2);
                last = last < -30000 ? -30000 : last > 30000 ? 30000 : last;
                for (u32 k = 0; k < run; k++)
                {
                    raised[i++] = static_cast<float>(last) * 0.01f;
                }
            }
            for (u32 i = 0; what & 2 && i < count && in.ok;)
            {
                const u32 run = in.number();
                u8 c[4];
                for (u8& part : c)
                {
                    part = in.byte();
                }
                if (run == 0 || run > count - i)
                {
                    return false;
                }
                for (u32 k = 0; k < run; k++, i++)
                {
                    memcpy(&paint[i * 4], c, 4);
                }
            }
            for (u32 i = 0, now = 0; what & 4 && i < count && in.ok; now ^= 1)
            {
                const u32 run = in.number();
                if (run > count - i || (run == 0 && i))
                {
                    return false;
                }
                for (u32 k = 0; k < run; k++)
                {
                    holes[i++] = static_cast<u8>(now);
                }
            }
            return in.ok && what < 8;
        }

        u32 zigzag(int v) { return v >= 0 ? static_cast<u32>(v) * 2 : static_cast<u32>(-v) * 2 - 1; }
        int unzigzag(u32 v) { return v & 1 ? -static_cast<int>(v / 2) - 1 : static_cast<int>(v / 2); }
    }

    // "2", how many regions, and each: where it is, and then as putRegion has it. As letters, so that it can be in JSON.
    std::string Space::landEdits() const
    {
        std::string body;
        u32 count = 0;
        for (const Edit* e : m_edits)
        {
            std::string one;
            putRegion(one, e->raised, e->paint, e->holes, 1024);
            if (one.size() > 1)
            {
                putNumber(body, zigzag(e->x));
                putNumber(body, zigzag(e->z));
                body += one;
                count++;
            }
        }
        if (count == 0)
        {
            return "";
        }
        std::string out;
        out += 2;
        putNumber(out, count);
        return toText(out + body);
    }

    bool Space::setLandEdits(const std::string& text)
    {
        if (!m_landOn)
        {
            return false;
        }
        struct Read
        {
            int x, z;
            float raised[1024];
            u8 paint[4096];
            u8 holes[1024];
        };
        GrowArray<Read*> regions;
        const auto forget = [&] { for (Read* r : regions) memFree(r); };
        const auto fresh = [&](int x, int z) -> Read*
        {
            Read* r = static_cast<Read*>(memAlloc(sizeof(Read), MemTag::Scene));
            if (r && regions.push(r))
            {
                memset(r, 0, sizeof(Read));
                r->x = x;
                r->z = z;
                return r;
            }
            memFree(r);
            return nullptr;
        };
        std::string data;
        Bytes in = { data, 0, true };
        bool ok = text.empty() || fromText(text, data);
        const u8 version = ok && !text.empty() ? in.byte() : 2;
        if (ok && version == 2 && !text.empty())
        {
            const u32 count = in.number();
            ok = in.ok && count <= kMostEdits;
            for (u32 i = 0; ok && i < count; i++)
            {
                const int x = unzigzag(in.number()), z = unzigzag(in.number());
                Read* r = in.ok && x > -2000 && x < 2000 && z > -2000 && z < 2000 ? fresh(x, z) : nullptr;
                ok = r && getRegion(in, r->raised, r->paint, r->holes, 1024);
            }
        }
        else if (ok && version == 1)
        {
            // As the first of these had it: the whole land, which must have as many squares.
            const u32 squares = in.number();
            ok = in.ok && squares == m_landSquares && squares > 0;
            const u32 corners = (squares + 1) * (squares + 1);
            GrowArray<float> raised;
            GrowArray<u8> paint, holes;
            ok = ok && zeros(raised, corners) && zeros(paint, corners * 4) && zeros(holes, squares * squares);
            // In the old order: heights, paint, holes; each for every corner, or every square.
            const u8 what = ok ? in.byte() : 0;
            data.insert(in.at, 1, static_cast<char>(what & 3));
            ok = ok && getRegion(in, raised.begin(), paint.begin(), holes.begin(), corners);
            if (ok && what & 4)
            {
                data.insert(in.at, 1, static_cast<char>(4));
                ok = getRegion(in, raised.begin(), paint.begin(), holes.begin(), squares * squares);
            }
            const int half = static_cast<int>(squares) / 2;
            for (u32 i = 0; ok && i < corners; i++)
            {
                const int x = static_cast<int>(i % (squares + 1)) - half, z = static_cast<int>(i / (squares + 1)) - half;
                Read* r = nullptr;
                for (Read* o : regions)
                {
                    r = o->x == x >> 5 && o->z == z >> 5 ? o : r;
                }
                r = r ? r : fresh(x >> 5, z >> 5);
                ok = r != nullptr;
                if (r)
                {
                    r->raised[(z & 31) * 32 + (x & 31)] = raised[i];
                    memcpy(&r->paint[((z & 31) * 32 + (x & 31)) * 4], &paint[i * 4], 4);
                    if (i < squares * squares)
                    {
                        const int sx = static_cast<int>(i % squares) - half, sz = static_cast<int>(i / squares) - half;
                        Read* h = nullptr;
                        for (Read* o : regions)
                        {
                            h = o->x == sx >> 5 && o->z == sz >> 5 ? o : h;
                        }
                        h = h ? h : fresh(sx >> 5, sz >> 5);
                        ok = h != nullptr;
                        if (h)
                        {
                            h->holes[(sz & 31) * 32 + (sx & 31)] = holes[i];
                        }
                    }
                }
            }
        }
        else
        {
            ok = ok && text.empty();
        }
        if (!ok || !in.ok || in.at != data.size())
        {
            forget();
            return false;
        }

        // What it was raised by is taken away, and what it is now raised by added.
        const auto moveRegion = [&](int rx, int rz, const float* from, const float* to)
        {
            float moved[32 * 32];
            bool any = false;
            for (u32 i = 0; i < 1024; i++)
            {
                moved[i] = (to ? to[i] : 0) - (from ? from[i] : 0);
                any = any || moved[i] != 0;
            }
            if (any)
            {
                moveCorners(rx * 32, rz * 32, rx * 32 + 31, rz * 32 + 31, moved);
            }
        };
        for (Edit* e : m_edits)
        {
            const Read* now = nullptr;
            for (const Read* r : regions)
            {
                now = r->x == e->x && r->z == e->z ? r : now;
            }
            float was[1024];
            memcpy(was, e->raised, sizeof(was));
            memset(e->raised, 0, sizeof(e->raised));
            moveRegion(e->x, e->z, was, now ? now->raised : nullptr);
            changeHoles(e->x * 32 - 1, e->z * 32 - 1, e->x * 32 + 32, e->z * 32 + 32);
        }
        clearEdits();
        bool room = true;
        for (const Read* r : regions)
        {
            Edit* e = editAt(r->x, r->z, true);
            room = room && e;
            if (e == nullptr)
            {
                continue;
            }
            memcpy(e->raised, r->raised, sizeof(e->raised));
            memcpy(e->paint, r->paint, sizeof(e->paint));
            memcpy(e->holes, r->holes, sizeof(e->holes));
            for (const u8 h : e->holes)
            {
                m_anyHoles = m_anyHoles || h;
            }
            // The heights are in the edit already: the land is moved to them.
            float zero[1024] = {};
            memset(e->raised, 0, sizeof(e->raised));
            moveRegion(r->x, r->z, zero, r->raised);
            changeHoles(r->x * 32 - 1, r->z * 32 - 1, r->x * 32 + 32, r->z * 32 + 32);
        }
        forget();
        return room;
    }
}
