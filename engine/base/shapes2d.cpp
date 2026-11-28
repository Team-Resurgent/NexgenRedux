#include "base/shapes2d.h"
#include "base/memory.h"

#include <math.h>

namespace nx
{
    namespace
    {
        float turn(Vec2 o, Vec2 a, Vec2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }
    }

    u32 triangulate(const Vec2* points, u32 count, u16* out)
    {
        if (points == nullptr || out == nullptr || count < 3 || count > 0xFFFF)
        {
            return 0;
        }
        float area = 0;
        for (u32 i = 0; i < count; i++)
        {
            const Vec2 a = points[i], b = points[(i + 1) % count];
            area += a.x * b.y - a.y * b.x;
        }
        u16 small[256];
        u16* left = count <= 256 ? small : static_cast<u16*>(memAlloc(sizeof(u16) * count, MemTag::General));
        if (left == nullptr)
        {
            return 0;
        }
        for (u32 i = 0; i < count; i++)
        {
            left[i] = static_cast<u16>(i);
        }
        // Ears cut off one at a time: a corner that turns the way the whole does, with no other corner inside it.
        const float way = area < 0 ? -1.0f : 1.0f;
        u32 made = 0;
        u32 n = count, tries = 0, i = 0;
        while (n > 3 && tries < n)
        {
            const u32 ia = (i + n - 1) % n, ib = i % n, iq = (i + 1) % n;
            const Vec2 a = points[left[ia]], b = points[left[ib]], q = points[left[iq]];
            bool ear = turn(a, b, q) * way > 0;
            for (u32 k = 0; ear && k < n; k++)
            {
                if (k == ia || k == ib || k == iq)
                {
                    continue;
                }
                const Vec2 p = points[left[k]];
                ear = !(turn(a, b, p) * way >= 0 && turn(b, q, p) * way >= 0 && turn(q, a, p) * way >= 0);
            }
            if (ear)
            {
                out[made * 3] = left[ia];
                out[made * 3 + 1] = left[ib];
                out[made * 3 + 2] = left[iq];
                made++;
                for (u32 k = ib; k + 1 < n; k++)
                {
                    left[k] = left[k + 1];
                }
                n--;
                tries = 0;
            }
            else
            {
                i++;
                tries++;
            }
        }
        // The last, or what is left of one whose sides cross, as a fan.
        for (u32 k = 1; k + 1 < n; k++)
        {
            out[made * 3] = left[0];
            out[made * 3 + 1] = left[k];
            out[made * 3 + 2] = left[k + 1];
            made++;
        }
        if (left != small)
        {
            memFree(left);
        }
        return made;
    }

    bool isConvex(const Vec2* points, u32 count)
    {
        if (points == nullptr || count < 3)
        {
            return false;
        }
        float way = 0;
        for (u32 i = 0; i < count; i++)
        {
            const float t = turn(points[i], points[(i + 1) % count], points[(i + 2) % count]);
            if (t * way < 0)
            {
                return false;
            }
            if (t != 0)
            {
                way = t;
            }
        }
        // A star turns the same way at every corner but goes round more than once.
        float round = 0;
        for (u32 i = 0; i < count; i++)
        {
            const Vec2 a = points[(i + 1) % count] - points[i], b = points[(i + 2) % count] - points[(i + 1) % count];
            round += atan2f(a.x * b.y - a.y * b.x, a.dot(b));
        }
        return way != 0 && fabsf(round) < 6.2831853f * 1.5f;
    }
}
