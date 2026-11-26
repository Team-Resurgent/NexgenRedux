#include "base/path.h"
#include "base/memory.h"

#include <math.h>

namespace nx
{
    void Path::release()
    {
        if (--m_refs == 0)
        {
            destroy(this);
        }
    }

    void Path::add(const Vec3& point)
    {
        m_points.push(point);
        m_measured = false;
    }

    void Path::clear()
    {
        m_points.clear();
        m_measured = false;
    }

    void Path::setPoint(u32 index, const Vec3& point)
    {
        if (index < m_points.count())
        {
            m_points[index] = point;
            m_measured = false;
        }
    }

    void Path::setLoop(bool loop)
    {
        m_loop = loop;
        m_measured = false;
    }

    u32 Path::pieces() const
    {
        const u32 n = m_points.count();
        return n < 2 ? 0 : m_loop ? n : n - 1;
    }

    Vec3 Path::curve(u32 piece, float t) const
    {
        const u32 n = m_points.count();
        const auto pointAt = [&](int i)
        {
            if (m_loop)
            {
                return m_points[static_cast<u32>((i % static_cast<int>(n) + static_cast<int>(n)) % static_cast<int>(n))];
            }
            // Past the ends, carried on in a straight line, so the curve starts and ends going the right way.
            if (i < 0)
            {
                return m_points[0] * 2 - m_points[1];
            }
            if (i >= static_cast<int>(n))
            {
                return m_points[n - 1] * 2 - m_points[n - 2];
            }
            return m_points[static_cast<u32>(i)];
        };
        const int i = static_cast<int>(piece);
        const Vec3 p0 = pointAt(i - 1), p1 = pointAt(i), p2 = pointAt(i + 1), p3 = pointAt(i + 2);
        const float t2 = t * t, t3 = t2 * t;
        return (p1 * 2 + (p2 - p0) * t + (p0 * 2 - p1 * 5 + p2 * 4 - p3) * t2 + (p1 * 3 - p0 - p2 * 3 + p3) * t3) * 0.5f;
    }

    void Path::measure()
    {
        if (m_measured)
        {
            return;
        }
        m_lengths.clear();
        const u32 steps = pieces() * kSteps;
        float total = 0;
        m_lengths.push(0);
        Vec3 was = pieces() ? curve(0, 0) : Vec3();
        for (u32 s = 1; s <= steps; s++)
        {
            const Vec3 now = curve((s - 1) / kSteps, static_cast<float>((s - 1) % kSteps + 1) / kSteps);
            total += (now - was).length();
            m_lengths.push(total);
            was = now;
        }
        m_measured = true;
    }

    float Path::length()
    {
        measure();
        return m_lengths.count() ? m_lengths[m_lengths.count() - 1] : 0;
    }

    void Path::find(float distance, u32& piece, float& t)
    {
        measure();
        const float total = length();
        piece = 0;
        t = 0;
        if (pieces() == 0 || total <= 0)
        {
            return;
        }
        if (m_loop)
        {
            distance = fmodf(distance, total);
            distance = distance < 0 ? distance + total : distance;
        }
        else
        {
            distance = distance < 0 ? 0 : distance > total ? total : distance;
        }
        // The step it falls in, then how far into that step.
        u32 low = 0, high = m_lengths.count() - 1;
        while (high - low > 1)
        {
            const u32 middle = (low + high) / 2;
            (m_lengths[middle] <= distance ? low : high) = middle;
        }
        const float span = m_lengths[high] - m_lengths[low];
        const float part = span > 0 ? (distance - m_lengths[low]) / span : 0;
        const float step = (static_cast<float>(low) + part) / kSteps;
        piece = static_cast<u32>(step);
        piece = piece >= pieces() ? pieces() - 1 : piece;
        t = step - static_cast<float>(piece);
    }

    Vec3 Path::at(float distance)
    {
        if (m_points.count() < 2)
        {
            return point(0);
        }
        u32 piece;
        float t;
        find(distance, piece, t);
        return curve(piece, t);
    }

    Vec3 Path::direction(float distance)
    {
        if (m_points.count() < 2)
        {
            return Vec3(1, 0, 0);
        }
        u32 piece;
        float t;
        find(distance, piece, t);
        const float a = t > 0.01f ? t - 0.01f : t, b = t < 0.99f ? t + 0.01f : t;
        const Vec3 d = curve(piece, b) - curve(piece, a);
        const float far = d.length();
        return far > 1e-6f ? d * (1 / far) : Vec3(1, 0, 0);
    }

    float Path::nearest(const Vec3& place)
    {
        measure();
        if (m_lengths.count() < 2)
        {
            return 0;
        }
        // The nearest step, then closer within the steps beside it.
        float best = 1e30f, bestAt = 0;
        const u32 steps = m_lengths.count() - 1;
        for (u32 s = 0; s <= steps; s++)
        {
            const u32 piece = s / kSteps < pieces() ? s / kSteps : pieces() - 1;
            const float t = static_cast<float>(s) / kSteps - static_cast<float>(piece);
            const Vec3 d = curve(piece, t) - place;
            if (d.dot(d) < best)
            {
                best = d.dot(d);
                bestAt = m_lengths[s];
            }
        }
        const float step = length() / static_cast<float>(steps);
        float from = bestAt - step, to = bestAt + step;
        for (int i = 0; i < 12; i++)
        {
            const float a = from + (to - from) / 3, b = to - (to - from) / 3;
            const Vec3 da = at(a) - place, db = at(b) - place;
            (da.dot(da) < db.dot(db) ? to : from) = da.dot(da) < db.dot(db) ? b : a;
        }
        const float found = (from + to) / 2;
        return m_loop ? found : (found < 0 ? 0 : found > length() ? length() : found);
    }
}
