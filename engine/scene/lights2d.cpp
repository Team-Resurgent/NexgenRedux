#include "scene/world.h"
#include "assets/assets.h"
#include "canvas/canvas.h"

#include <algorithm>
#include <math.h>

// Lights on the plane. Each frame a picture the size of the screen is filled with the ambient light, and each light adds a
// fan of triangles to it: from the light out to whatever is in its way, fading to nothing at its radius, so that what is
// behind a wall or an occluder is in its shadow. Then the picture is multiplied over what was drawn.
namespace nx
{
    namespace
    {
        constexpr u32 kRound = 48;          // rays all round, so that a light with nothing near it is a circle
        constexpr float kAside = 0.0004f;    // radians either side of each corner, to see past it
        constexpr u32 kMostLights = 64;
        constexpr float kInto = 18;          // units a light reaches into what stops it, so that its face is lit

        // How far along a ray from `from`, in units of `way`, it meets a segment; or more than 1 if it doesn't.
        float meets(const Vec2& from, const Vec2& way, const Vec2& a, const Vec2& b)
        {
            const Vec2 edge = b - a;
            const float across = way.x * edge.y - way.y * edge.x;
            if (fabsf(across) < 1e-9f)
            {
                return 2;
            }
            const Vec2 d = a - from;
            const float t = (d.x * edge.y - d.y * edge.x) / across;
            const float u = (d.x * way.y - d.y * way.x) / across;
            return t >= 0 && u >= 0 && u <= 1 ? t : 2;
        }
    }

    void World::releaseLightMap()
    {
        if (m_lightMap)
        {
            m_assets.release(m_lightMap);
            m_lightMap = nullptr;
        }
    }

    // Edges of what is in the way, within a box: solid tiles, and colliders that cast shadows.
    void World::lightEdges(const float (&box)[4])
    {
        m_lightEdges.clear();
        const auto add = [&](const Vec2& a, const Vec2& b)
        {
            m_lightEdges.push(a);
            m_lightEdges.push(b);
        };
        for (const Box& w : m_walls)
        {
            if (w.maxX < box[0] || w.minX > box[2] || w.maxY < box[1] || w.minY > box[3])
            {
                continue;
            }
            const Vec2 a(w.minX, w.minY), b(w.maxX, w.minY), c(w.maxX, w.maxY), d(w.minX, w.maxY);
            add(a, b);
            add(b, c);
            add(c, d);
            add(d, a);
        }
        for (const Collider& c : m_colliders)
        {
            if (!c.occluder)
            {
                continue;
            }
            const Entity& e = m_entities[c.owner];
            if (!e.live || e.dying || !e.shown)
            {
                continue;
            }
            const float* m = e.world;
            const auto at = [&](float x, float y) { return Vec2(m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]); };
            Vec2 corners[8];
            u32 count = 0;
            if (c.shape == Shape::Circle)
            {
                for (u32 i = 0; i < 8; i++)
                {
                    const float a = static_cast<float>(i) * 0.785398f;
                    corners[count++] = at(cosf(a) * c.halfWidth, sinf(a) * c.halfWidth);
                }
            }
            else
            {
                corners[count++] = at(-c.halfWidth, -c.halfHeight);
                corners[count++] = at(c.halfWidth, -c.halfHeight);
                corners[count++] = at(c.halfWidth, c.halfHeight);
                corners[count++] = at(-c.halfWidth, c.halfHeight);
            }
            float low[2] = { 1e30f, 1e30f }, high[2] = { -1e30f, -1e30f };
            for (u32 i = 0; i < count; i++)
            {
                low[0] = fminf(low[0], corners[i].x);
                low[1] = fminf(low[1], corners[i].y);
                high[0] = fmaxf(high[0], corners[i].x);
                high[1] = fmaxf(high[1], corners[i].y);
            }
            if (high[0] < box[0] || low[0] > box[2] || high[1] < box[1] || low[1] > box[3])
            {
                continue;
            }
            for (u32 i = 0; i < count; i++)
            {
                add(corners[i], corners[(i + 1) % count]);
            }
        }
    }

    void World::drawLights(Canvas& canvas, const float (&view)[4])
    {
        // A picture as large as what is drawn into, made again if that changes size.
        const u32 width = static_cast<u32>(canvas.pixelWidth()), height = static_cast<u32>(canvas.pixelHeight());
        if (m_lightMap && (m_lightMap->width != width || m_lightMap->height != height))
        {
            releaseLightMap();
        }
        if (m_lightMap == nullptr && width && height)
        {
            m_lightMap = m_assets.target(width, height);
        }
        float transform[6];
        canvas.getTransform(transform);
        if (m_lightMap == nullptr || canvas.inTarget() || !canvas.beginTarget(*m_lightMap, m_ambient))
        {
            return;     // without pictures to draw into, what is drawn stays as it is
        }
        // The same place on the picture as on the screen.
        canvas.setTransform(transform);
        canvas.setBlend(gpu::Blend::Additive);
        u32 drawn = 0;
        for (const Light2& l : m_lights)
        {
            const Entity& e = m_entities[l.owner];
            if (!e.live || e.dying || !e.shown || l.radius <= 0 || drawn == kMostLights)
            {
                continue;
            }
            const Vec2 at(e.world[4], e.world[5]);
            const float r = l.radius;
            if (at.x + r < view[0] || at.x - r > view[2] || at.y + r < view[1] || at.y - r > view[3])
            {
                continue;
            }
            drawn++;
            const float box[4] = { at.x - r, at.y - r, at.x + r, at.y + r };
            lightEdges(box);
            // Rays all round, and either side of every corner near it, in order round the light.
            m_lightAngles.clear();
            for (u32 i = 0; i < kRound; i++)
            {
                m_lightAngles.push(static_cast<float>(i) * (6.2831853f / kRound));
            }
            for (const Vec2& p : m_lightEdges)
            {
                const Vec2 d = p - at;
                if (d.x * d.x + d.y * d.y < r * r)
                {
                    const float a = atan2f(d.y, d.x);
                    m_lightAngles.push(a - kAside);
                    m_lightAngles.push(a);
                    m_lightAngles.push(a + kAside);
                }
            }
            for (float& a : m_lightAngles)
            {
                a = a < 0 ? a + 6.2831853f : a;
            }
            std::sort(m_lightAngles.begin(), m_lightAngles.end());
            // As far as each ray goes, and a little into what stops it, so that the face of a wall is lit too.
            const auto reach = [&](float a, Color& color)
            {
                const Vec2 way(cosf(a) * r, sinf(a) * r);
                float nearest = 1;
                for (u32 i = 0; i + 1 < m_lightEdges.count(); i += 2)
                {
                    nearest = fminf(nearest, meets(at, way, m_lightEdges[i], m_lightEdges[i + 1]));
                }
                nearest = fminf(1, nearest + (nearest < 1 ? kInto / r : 0));
                const float left = 1 - nearest;
                color = { l.color.r * left, l.color.g * left, l.color.b * left, 1 };
                return at + way * nearest;
            };
            const Color middle = { l.color.r, l.color.g, l.color.b, 1 };
            Color firstColor, lastColor;
            const Vec2 first = reach(m_lightAngles[0], firstColor);
            Vec2 last = first;
            lastColor = firstColor;
            for (u32 i = 1; i <= m_lightAngles.count(); i++)
            {
                Color color;
                const Vec2 next = i < m_lightAngles.count() ? reach(m_lightAngles[i], color) : first;
                if (i == m_lightAngles.count())
                {
                    color = firstColor;
                }
                canvas.triangle(at, last, next, middle, lastColor, color);
                last = next;
                lastColor = color;
            }
        }
        canvas.endTarget();
        // Over everything drawn so far: darkened to the ambient light, and lit where the lights are.
        const float pixels[6] = { 1, 0, 0, 1, 0, 0 };
        canvas.setTransform(pixels);
        canvas.setBlend(gpu::Blend::Multiply);
        canvas.image(*m_lightMap, 0, 0, static_cast<float>(width), static_cast<float>(height), { 1, 1, 1, 1 });
        canvas.setTransform(transform);
    }
}
