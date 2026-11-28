#include "base/curve.h"
#include "base/memory.h"

#include <math.h>

namespace nx
{
    namespace
    {
        // De Casteljau's: each pair of neighbours mixed, again and again, until one is left.
        Vec2 evaluate(const Vec2* points, u32 count, float t)
        {
            Vec2 work[Curve::kMostPoints];
            for (u32 i = 0; i < count; i++)
            {
                work[i] = points[i];
            }
            for (u32 level = count; level > 1; level--)
            {
                for (u32 i = 0; i + 1 < level; i++)
                {
                    work[i] = work[i] + (work[i + 1] - work[i]) * t;
                }
            }
            return count ? work[0] : Vec2();
        }
    }

    void Curve::release()
    {
        if (--m_refs == 0)
        {
            destroy(this);
        }
    }

    bool Curve::add(const Vec2& point)
    {
        return m_points.count() < kMostPoints && m_points.push(point);
    }

    bool Curve::insert(u32 index, const Vec2& point)
    {
        if (m_points.count() >= kMostPoints || !m_points.push(point))
        {
            return false;
        }
        index = index < m_points.count() - 1 ? index : m_points.count() - 1;
        for (u32 i = m_points.count() - 1; i > index; i--)
        {
            m_points[i] = m_points[i - 1];
        }
        m_points[index] = point;
        return true;
    }

    void Curve::remove(u32 index)
    {
        if (index >= m_points.count())
        {
            return;
        }
        for (u32 i = index; i + 1 < m_points.count(); i++)
        {
            m_points[i] = m_points[i + 1];
        }
        m_points.resize(m_points.count() - 1);
    }

    void Curve::setPoint(u32 index, const Vec2& point)
    {
        if (index < m_points.count())
        {
            m_points[index] = point;
        }
    }

    Vec2 Curve::at(float t) const
    {
        return m_points.count() ? evaluate(&m_points[0], m_points.count(), t) : Vec2();
    }

    Vec2 Curve::slope(float t) const
    {
        const u32 n = m_points.count();
        if (n < 2)
        {
            return {};
        }
        Vec2 d[kMostPoints];
        for (u32 i = 0; i + 1 < n; i++)
        {
            d[i] = (m_points[i + 1] - m_points[i]) * static_cast<float>(n - 1);
        }
        return evaluate(d, n - 1, t);
    }

    u32 Curve::points(u32 count, Vec2* out) const
    {
        if (m_points.count() == 0 || out == nullptr)
        {
            return 0;
        }
        for (u32 i = 0; i < count; i++)
        {
            out[i] = at(count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0f);
        }
        return count;
    }

    float Curve::length(u32 pieces) const
    {
        pieces = pieces ? pieces : 1;
        float total = 0;
        Vec2 last = at(0);
        for (u32 i = 1; i <= pieces; i++)
        {
            const Vec2 p = at(static_cast<float>(i) / static_cast<float>(pieces));
            const Vec2 d = p - last;
            total += sqrtf(d.dot(d));
            last = p;
        }
        return total;
    }

    void Curve::part(float from, float to, Curve& out) const
    {
        out.clear();
        const u32 n = m_points.count();
        if (n == 0)
        {
            return;
        }
        from = from < 0 ? 0 : from > 1 ? 1 : from;
        to = to < 0 ? 0 : to > 1 ? 1 : to;
        // The part after from, then of that the part before where to has become.
        Vec2 afterFrom[kMostPoints], head[kMostPoints];
        Vec2 work[kMostPoints];
        for (u32 i = 0; i < n; i++)
        {
            work[i] = m_points[i];
        }
        // The last of each level of mixing, from the one point at the end up to the whole, is the part after from.
        for (u32 level = n; level > 0; level--)
        {
            afterFrom[level - 1] = work[level - 1];
            for (u32 i = 0; i + 1 < level; i++)
            {
                work[i] = work[i] + (work[i + 1] - work[i]) * from;
            }
        }
        // And the first of each, from the whole down, is the part before where to has become.
        const float t = from < 1 ? (to - from) / (1 - from) : 0;
        for (u32 i = 0; i < n; i++)
        {
            work[i] = afterFrom[i];
        }
        for (u32 level = n; level > 0; level--)
        {
            head[n - level] = work[0];
            for (u32 i = 0; i + 1 < level; i++)
            {
                work[i] = work[i] + (work[i + 1] - work[i]) * t;
            }
        }
        for (u32 i = 0; i < n; i++)
        {
            out.add(head[i]);
        }
    }
}
