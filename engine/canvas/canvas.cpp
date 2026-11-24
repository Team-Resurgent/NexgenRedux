#include "canvas/canvas.h"

#include <math.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr float kPi = 3.14159265358979f;

        u8 toByte(float v)
        {
            return static_cast<u8>(v <= 0 ? 0 : v >= 1 ? 255 : v * 255.0f + 0.5f);
        }

        bool same(const gpu::Scissor& a, const gpu::Scissor& b)
        {
            return a.enabled == b.enabled && a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
        }

        // Next character, moving past it. Anything malformed is one '?', and never reads past the end.
        u32 nextChar(const char*& p, const char* end)
        {
            const u8 first = static_cast<u8>(*p++);
            if (first < 0x80)
            {
                return first;
            }
            const u32 extra = first >= 0xF0 ? 3 : first >= 0xE0 ? 2 : first >= 0xC0 ? 1 : 0;
            u32 code = first & (0x3F >> extra);
            for (u32 i = 0; i < extra; i++)
            {
                if (p >= end || (static_cast<u8>(*p) & 0xC0) != 0x80)
                {
                    return '?';
                }
                code = (code << 6) | (static_cast<u8>(*p++) & 0x3F);
            }
            return extra ? code : '?';
        }

        const Glyph& glyph(const FontData& font, u32 code)
        {
            if (code < kFirstGlyph || code > kLastGlyph)
            {
                code = '?';
            }
            return font.glyphs[code - kFirstGlyph];
        }
    }

    bool Canvas::init(gpu::Device& device, Assets& assets)
    {
        m_device = &device;
        m_assets = &assets;
        m_vertices = static_cast<gpu::Vertex*>(memAlloc(sizeof(gpu::Vertex) * kMaxVertices, MemTag::Frame));
        bool ok = m_vertices != nullptr;
        for (u32 i = 0; i < 3; i++)
        {
            gpu::PipelineDesc desc;
            desc.blend = static_cast<gpu::Blend>(i);
            m_pipelines[i] = device.createPipeline(desc);
            ok = ok && m_pipelines[i];
        }
        // A dot that fades to nothing, for glows and particles.
        u8 soft[32 * 32 * 4];
        for (u32 i = 0; i < 32 * 32; i++)
        {
            const float dx = (static_cast<float>(i % 32) - 15.5f) / 15.5f;
            const float dy = (static_cast<float>(i / 32) - 15.5f) / 15.5f;
            const float left = 1 - sqrtf(dx * dx + dy * dy);
            soft[i * 4 + 0] = soft[i * 4 + 1] = soft[i * 4 + 2] = 255;
            soft[i * 4 + 3] = left > 0 ? static_cast<u8>(left * left * (3 - 2 * left) * 255) : 0;
        }
        m_soft = device.createTexture(gpu::rgbaTexture(soft, 32, 32));
        ok = ok && m_soft;
        u32 w = 1, h = 1;
        device.size(w, h);
        m_pixelWidth = static_cast<float>(w);
        m_pixelHeight = static_cast<float>(h);
        design(m_pixelWidth, m_pixelHeight);
        return ok;
    }

    void Canvas::shutdown()
    {
        if (m_device)
        {
            for (gpu::Pipeline p : m_pipelines)
            {
                m_device->destroyPipeline(p);
            }
            m_device->destroyTexture(m_soft);
        }
        memFree(m_vertices);
        m_vertices = nullptr;
        m_device = nullptr;
    }

    void Canvas::begin()
    {
        u32 w = 1, h = 1;
        m_device->size(w, h);
        m_pixelWidth = static_cast<float>(w);
        m_pixelHeight = static_cast<float>(h);
        m_count = 0;
        m_draws = 0;
        m_depth = 0;
        m_overflowed = false;
        m_texture = {};
        design(m_pixelWidth, m_pixelHeight);
    }

    void Canvas::end()
    {
        flush();
    }

    void Canvas::design(float width, float height)
    {
        m_width = width > 1 ? width : 1;
        m_height = height > 1 ? height : 1;
        const float sx = m_pixelWidth / m_width;
        const float sy = m_pixelHeight / m_height;
        m_scale = sx < sy ? sx : sy;
        m_offsetX = floorf((m_pixelWidth - m_width * m_scale) * 0.5f);
        m_offsetY = floorf((m_pixelHeight - m_height * m_scale) * 0.5f);
        m_state = State();
        m_state.m[0] = m_state.m[3] = m_scale;
        m_state.m[4] = m_offsetX;
        m_state.m[5] = m_offsetY;
        m_depth = 0;
        noClip();
        m_assets->setFontScale(m_scale);
    }

    void Canvas::designHeight(float height)
    {
        design(height * m_pixelWidth / m_pixelHeight, height);
    }

    Vec2 Canvas::toDesign(float x, float y) const
    {
        return { (x - m_offsetX) / m_scale, (y - m_offsetY) / m_scale };
    }

    bool Canvas::effect(const Asset& effect, const float (&values)[16], float time)
    {
        flush();
        return effect.effect && m_device->applyEffect(effect.effect, values, time, {});
    }

    bool Canvas::bloom(float strength, float threshold, float spread)
    {
        flush();
        gpu::Bloom b;
        b.strength = strength;
        b.threshold = threshold;
        b.spread = spread;
        return m_device->bloom(b, {});
    }

    gpu::Scissor Canvas::toPixels(float x, float y, float w, float h) const
    {
        gpu::Scissor s;
        s.enabled = true;
        s.x = static_cast<i32>(floorf(m_offsetX + x * m_scale + 0.5f));
        s.y = static_cast<i32>(floorf(m_offsetY + y * m_scale + 0.5f));
        s.width = static_cast<i32>(floorf(m_offsetX + (x + w) * m_scale + 0.5f)) - s.x;
        s.height = static_cast<i32>(floorf(m_offsetY + (y + h) * m_scale + 0.5f)) - s.y;
        return s;
    }

    void Canvas::push()
    {
        if (m_depth < kMaxStates)
        {
            m_states[m_depth] = m_state;
        }
        m_depth++;
    }

    void Canvas::pop()
    {
        if (m_depth == 0)
        {
            return;
        }
        m_depth--;
        if (m_depth < kMaxStates)
        {
            m_state = m_states[m_depth];
        }
    }

    void Canvas::translate(float x, float y)
    {
        float* m = m_state.m;
        m[4] += m[0] * x + m[2] * y;
        m[5] += m[1] * x + m[3] * y;
    }

    void Canvas::rotate(float degrees)
    {
        const float c = cosf(degrees * kPi / 180.0f);
        const float s = sinf(degrees * kPi / 180.0f);
        float* m = m_state.m;
        const float a = m[0] * c + m[2] * s;
        const float b = m[1] * c + m[3] * s;
        m[2] = m[2] * c - m[0] * s;
        m[3] = m[3] * c - m[1] * s;
        m[0] = a;
        m[1] = b;
    }

    void Canvas::scale(float x, float y)
    {
        float* m = m_state.m;
        m[0] *= x;
        m[1] *= x;
        m[2] *= y;
        m[3] *= y;
    }

    void Canvas::transform(const float (&t)[6])
    {
        float* m = m_state.m;
        const float a = m[0] * t[0] + m[2] * t[1];
        const float b = m[1] * t[0] + m[3] * t[1];
        const float c = m[0] * t[2] + m[2] * t[3];
        const float d = m[1] * t[2] + m[3] * t[3];
        m[4] += m[0] * t[4] + m[2] * t[5];
        m[5] += m[1] * t[4] + m[3] * t[5];
        m[0] = a;
        m[1] = b;
        m[2] = c;
        m[3] = d;
    }

    void Canvas::clip(float x, float y, float w, float h)
    {
        const float* m = m_state.m;
        float x0 = m[0] * x + m[2] * y + m[4];
        float y0 = m[1] * x + m[3] * y + m[5];
        float x1 = m[0] * (x + w) + m[2] * (y + h) + m[4];
        float y1 = m[1] * (x + w) + m[3] * (y + h) + m[5];
        if (x1 < x0)
        {
            std::swap(x0, x1);
        }
        if (y1 < y0)
        {
            std::swap(y0, y1);
        }
        gpu::Scissor& s = m_state.scissor;
        const float left = x0 > static_cast<float>(s.x) ? x0 : static_cast<float>(s.x);
        const float top = y0 > static_cast<float>(s.y) ? y0 : static_cast<float>(s.y);
        const float right = x1 < static_cast<float>(s.x + s.width) ? x1 : static_cast<float>(s.x + s.width);
        const float bottom = y1 < static_cast<float>(s.y + s.height) ? y1 : static_cast<float>(s.y + s.height);
        s.enabled = true;
        s.x = static_cast<i32>(floorf(left + 0.5f));
        s.y = static_cast<i32>(floorf(top + 0.5f));
        s.width = right > left ? static_cast<i32>(floorf(right + 0.5f)) - s.x : 0;
        s.height = bottom > top ? static_cast<i32>(floorf(bottom + 0.5f)) - s.y : 0;
    }

    void Canvas::noClip()
    {
        // The design area: what is drawn outside it would land in the borders.
        gpu::Scissor& s = m_state.scissor;
        s.enabled = true;
        s.x = static_cast<i32>(m_offsetX);
        s.y = static_cast<i32>(m_offsetY);
        s.width = static_cast<i32>(floorf(m_width * m_scale + 0.5f));
        s.height = static_cast<i32>(floorf(m_height * m_scale + 0.5f));
    }

    // --- Batching ---

    u32 Canvas::pack(const Color& c) const
    {
        return static_cast<u32>(toByte(c.r)) | (static_cast<u32>(toByte(c.g)) << 8) | (static_cast<u32>(toByte(c.b)) << 16) | (static_cast<u32>(toByte(c.a * m_state.alpha)) << 24);
    }

    void Canvas::use(gpu::Texture texture, float uMax, float vMax)
    {
        if (m_count && (texture != m_texture || m_state.blend != m_batchBlend || m_state.filter != m_batchFilter || !same(m_state.scissor, m_batchScissor)))
        {
            flush();
        }
        m_texture = texture;
        m_uMax = uMax;
        m_vMax = vMax;
        m_batchBlend = m_state.blend;
        m_batchFilter = m_state.filter;
        m_batchScissor = m_state.scissor;
    }

    void Canvas::flush()
    {
        if (m_count == 0)
        {
            return;
        }
        gpu::Draw draw;
        draw.first = m_device->stream(m_vertices, m_count);
        draw.count = m_count;
        m_count = 0;
        if (draw.first == gpu::kStreamFull)
        {
            if (!m_overflowed)
            {
                NX_WARN("too much drawn in one frame: the rest is left out");
                m_overflowed = true;
            }
            return;
        }
        draw.pipeline = m_pipelines[static_cast<u32>(m_batchBlend)];
        draw.texture = m_texture;
        draw.filter = m_batchFilter;
        draw.scissor = m_batchScissor;
        draw.transform = Mat4::ortho(0, m_pixelWidth, m_pixelHeight, 0, 0, 1);
        m_device->draw(draw);
        m_draws++;
    }

    void Canvas::addTriangle(const Point& a, const Point& b, const Point& c)
    {
        if (m_count + 3 > kMaxVertices)
        {
            flush();
        }
        const float* m = m_state.m;
        for (const Point* p : { &a, &b, &c })
        {
            gpu::Vertex& v = m_vertices[m_count++];
            v.x = m[0] * p->x + m[2] * p->y + m[4];
            v.y = m[1] * p->x + m[3] * p->y + m[5];
            v.z = 0;
            v.r = static_cast<u8>(p->color);
            v.g = static_cast<u8>(p->color >> 8);
            v.b = static_cast<u8>(p->color >> 16);
            v.a = static_cast<u8>(p->color >> 24);
            v.u = p->u * m_uMax;
            v.v = p->v * m_vMax;
        }
    }

    void Canvas::addQuad(const Point& a, const Point& b, const Point& c, const Point& d)
    {
        if (m_count + 6 > kMaxVertices)
        {
            flush();
        }
        addTriangle(a, b, c);
        addTriangle(a, c, d);
    }

    void Canvas::quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, u32 color)
    {
        addQuad({ x, y, u0, v0, color }, { x + w, y, u1, v0, color }, { x + w, y + h, u1, v1, color }, { x, y + h, u0, v1, color });
    }

    // --- Shapes ---

    void Canvas::rect(float x, float y, float w, float h, const Color& color)
    {
        use({}, 1, 1);
        quad(x, y, w, h, 0, 0, 1, 1, pack(color));
    }

    void Canvas::rectLine(float x, float y, float w, float h, float t, const Color& color)
    {
        use({}, 1, 1);
        const u32 c = pack(color);
        quad(x, y, w, t, 0, 0, 1, 1, c);
        quad(x, y + h - t, w, t, 0, 0, 1, 1, c);
        quad(x, y + t, t, h - 2 * t, 0, 0, 1, 1, c);
        quad(x + w - t, y + t, t, h - 2 * t, 0, 0, 1, 1, c);
    }

    void Canvas::gradient(float x, float y, float w, float h, const Color& top, const Color& bottom)
    {
        use({}, 1, 1);
        const u32 a = pack(top);
        const u32 b = pack(bottom);
        addQuad({ x, y, 0, 0, a }, { x + w, y, 0, 0, a }, { x + w, y + h, 0, 0, b }, { x, y + h, 0, 0, b });
    }

    void Canvas::roundRect(float x, float y, float w, float h, float radius, const Color& color)
    {
        use({}, 1, 1);
        const u32 c = pack(color);
        float r = radius < w * 0.5f ? radius : w * 0.5f;
        r = r < h * 0.5f ? r : h * 0.5f;
        quad(x + r, y, w - 2 * r, h, 0, 0, 0, 0, c);
        quad(x, y + r, r, h - 2 * r, 0, 0, 0, 0, c);
        quad(x + w - r, y + r, r, h - 2 * r, 0, 0, 0, 0, c);
        const float cx[4] = { x + w - r, x + r, x + r, x + w - r };
        const float cy[4] = { y + h - r, y + h - r, y + r, y + r };
        constexpr int kSteps = 6;
        for (int corner = 0; corner < 4; corner++)
        {
            for (int i = 0; i < kSteps; i++)
            {
                const float a0 = (static_cast<float>(corner) + static_cast<float>(i) / kSteps) * kPi * 0.5f;
                const float a1 = (static_cast<float>(corner) + static_cast<float>(i + 1) / kSteps) * kPi * 0.5f;
                addTriangle({ cx[corner], cy[corner], 0, 0, c }, { cx[corner] + cosf(a0) * r, cy[corner] + sinf(a0) * r, 0, 0, c }, { cx[corner] + cosf(a1) * r, cy[corner] + sinf(a1) * r, 0, 0, c });
            }
        }
    }

    void Canvas::line(float x1, float y1, float x2, float y2, float thickness, const Color& color)
    {
        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float length = sqrtf(dx * dx + dy * dy);
        if (length <= 0)
        {
            return;
        }
        const float nx_ = -dy / length * thickness * 0.5f;
        const float ny_ = dx / length * thickness * 0.5f;
        use({}, 1, 1);
        const u32 c = pack(color);
        addQuad({ x1 + nx_, y1 + ny_, 0, 0, c }, { x2 + nx_, y2 + ny_, 0, 0, c }, { x2 - nx_, y2 - ny_, 0, 0, c }, { x1 - nx_, y1 - ny_, 0, 0, c });
    }

    void Canvas::circle(float x, float y, float radius, const Color& color)
    {
        arc(x, y, radius, radius, 0, 360, color);
    }

    void Canvas::ring(float x, float y, float radius, float thickness, const Color& color)
    {
        arc(x, y, radius, thickness, 0, 360, color);
    }

    void Canvas::arc(float x, float y, float radius, float thickness, float startDegrees, float endDegrees, const Color& color)
    {
        if (radius <= 0 || endDegrees <= startDegrees)
        {
            return;
        }
        const float inner = thickness >= radius ? 0 : radius - thickness;
        const float sweep = (endDegrees - startDegrees) * kPi / 180.0f;
        const float start = startDegrees * kPi / 180.0f;
        // As few sides as stay within a quarter of a pixel of the true curve.
        const float* m = m_state.m;
        const float pixels = radius * sqrtf(fabsf(m[0] * m[3] - m[1] * m[2]));
        const float side = pixels > 1 ? 2 * acosf(1 - 0.25f / pixels) : 1.5f;
        int steps = static_cast<int>(ceilf(sweep / side));
        const int fewest = static_cast<int>(ceilf(sweep / 1.5f));
        steps = steps < fewest ? fewest : steps;
        steps = steps > 160 ? 160 : steps;
        use({}, 1, 1);
        const u32 c = pack(color);
        float c0 = cosf(start), s0 = sinf(start);
        for (int i = 1; i <= steps; i++)
        {
            const float a = start + sweep * static_cast<float>(i) / static_cast<float>(steps);
            const float c1 = cosf(a), s1 = sinf(a);
            const Point o0 = { x + c0 * radius, y + s0 * radius, 0, 0, c };
            const Point o1 = { x + c1 * radius, y + s1 * radius, 0, 0, c };
            if (inner <= 0)
            {
                addTriangle({ x, y, 0, 0, c }, o0, o1);
            }
            else
            {
                addQuad({ x + c0 * inner, y + s0 * inner, 0, 0, c }, o0, o1, { x + c1 * inner, y + s1 * inner, 0, 0, c });
            }
            c0 = c1;
            s0 = s1;
        }
    }

    void Canvas::triangle(const Vec2& a, const Vec2& b, const Vec2& c, const Color& ca, const Color& cb, const Color& cc)
    {
        use({}, 1, 1);
        addTriangle({ a.x, a.y, 0, 0, pack(ca) }, { b.x, b.y, 0, 0, pack(cb) }, { c.x, c.y, 0, 0, pack(cc) });
    }

    // --- Images ---

    void Canvas::image(const Asset& t, float x, float y, float w, float h, const Color& tint)
    {
        const gpu::TextureInfo info = m_device->textureInfo(t.texture);
        use(t.texture, info.uMax, info.vMax);
        quad(x, y, w, h, 0, 0, 1, 1, pack(tint));
    }

    void Canvas::imagePart(const Asset& t, float x, float y, float w, float h, float sx, float sy, float sw, float sh, const Color& tint)
    {
        if (t.width == 0 || t.height == 0)
        {
            return;
        }
        const gpu::TextureInfo info = m_device->textureInfo(t.texture);
        use(t.texture, info.uMax, info.vMax);
        const float iw = 1.0f / static_cast<float>(t.width);
        const float ih = 1.0f / static_cast<float>(t.height);
        quad(x, y, w, h, sx * iw, sy * ih, (sx + sw) * iw, (sy + sh) * ih, pack(tint));
    }

    void Canvas::placed(const Asset* texture, const float (&t)[6], float w, float h, float u0, float v0, float u1, float v1, const Color& color)
    {
        if (texture)
        {
            const gpu::TextureInfo info = m_device->textureInfo(texture->texture);
            use(texture->texture, info.uMax, info.vMax);
        }
        else
        {
            use({}, 1, 1);
        }
        const float x = w * 0.5f, y = h * 0.5f;
        const float ax = t[0] * x, ay = t[1] * x;
        const float bx = t[2] * y, by = t[3] * y;
        const u32 c = pack(color);
        addQuad({ t[4] - ax - bx, t[5] - ay - by, u0, v0, c }, { t[4] + ax - bx, t[5] + ay - by, u1, v0, c },
            { t[4] + ax + bx, t[5] + ay + by, u1, v1, c }, { t[4] - ax + bx, t[5] - ay + by, u0, v1, c });
    }

    void Canvas::glow(float x, float y, float radius, const Color& color)
    {
        use(m_soft, 1, 1);
        quad(x - radius, y - radius, radius * 2, radius * 2, 0, 0, 1, 1, pack(color));
    }

    void Canvas::sprite(const Asset& t, u32 columns, u32 rows, u32 frame, float x, float y, float w, float h, const Color& tint)
    {
        if (columns == 0 || rows == 0)
        {
            return;
        }
        frame %= columns * rows;
        const float fw = static_cast<float>(t.width) / static_cast<float>(columns);
        const float fh = static_cast<float>(t.height) / static_cast<float>(rows);
        imagePart(t, x, y, w, h, static_cast<float>(frame % columns) * fw, static_cast<float>(frame / columns) * fh, fw, fh, tint);
    }

    void Canvas::nineSlice(const Asset& t, float x, float y, float w, float h, float corner, const Color& tint)
    {
        if (t.width == 0 || t.height == 0)
        {
            return;
        }
        const gpu::TextureInfo info = m_device->textureInfo(t.texture);
        use(t.texture, info.uMax, info.vMax);
        const float cw = corner < w * 0.5f ? corner : w * 0.5f;
        const float ch = corner < h * 0.5f ? corner : h * 0.5f;
        const float px[4] = { x, x + cw, x + w - cw, x + w };
        const float py[4] = { y, y + ch, y + h - ch, y + h };
        const float cu = corner / static_cast<float>(t.width);
        const float cv = corner / static_cast<float>(t.height);
        const float u[4] = { 0, cu, 1 - cu, 1 };
        const float v[4] = { 0, cv, 1 - cv, 1 };
        const u32 c = pack(tint);
        for (int row = 0; row < 3; row++)
        {
            for (int column = 0; column < 3; column++)
            {
                quad(px[column], py[row], px[column + 1] - px[column], py[row + 1] - py[row], u[column], v[row], u[column + 1], v[row + 1], c);
            }
        }
    }

    void Canvas::tile(const Asset& t, float x, float y, float w, float h, float tileW, float tileH, float ox, float oy, const Color& tint)
    {
        if (tileW < 1 || tileH < 1 || t.width == 0 || t.height == 0)
        {
            return;
        }
        const gpu::TextureInfo info = m_device->textureInfo(t.texture);
        use(t.texture, info.uMax, info.vMax);
        const u32 c = pack(tint);
        const float startX = ox - ceilf(ox / tileW) * tileW;
        const float startY = oy - ceilf(oy / tileH) * tileH;
        // Each tile is cut to the area here rather than by a clip, so it all stays one draw.
        for (float ty = startY; ty < h; ty += tileH)
        {
            const float y0 = ty < 0 ? 0 : ty;
            const float y1 = ty + tileH > h ? h : ty + tileH;
            for (float tx = startX; tx < w; tx += tileW)
            {
                const float x0 = tx < 0 ? 0 : tx;
                const float x1 = tx + tileW > w ? w : tx + tileW;
                if (x1 > x0 && y1 > y0)
                {
                    quad(x + x0, y + y0, x1 - x0, y1 - y0, (x0 - tx) / tileW, (y0 - ty) / tileH, (x1 - tx) / tileW, (y1 - ty) / tileH, c);
                }
            }
        }
    }

    // --- Text ---

    float Canvas::lineWidth(const FontData& font, const char* text, const char* end) const
    {
        float width = 0;
        while (text < end)
        {
            width += glyph(font, nextChar(text, end)).advance;
        }
        return width * font.scale;
    }

    float Canvas::textRun(const Asset& asset, const char* text, const char* end, float x, float y, u32 color)
    {
        const FontData& font = *asset.font;
        const float s = font.scale;
        float baseline = y + font.ascent * s;
        // Drawn at the size it was made for, text is sharp only when it starts on a whole pixel.
        const float* m = m_state.m;
        if (fabsf(m[1]) < 1e-4f && fabsf(m[2]) < 1e-4f && fabsf(m[0] - m[3]) < 1e-4f && (fabsf(m[0] * s - 1) < 0.02f || fabsf(m[0] * s - 0.5f) < 0.02f))
        {
            x = (floorf(m[0] * x + m[4] + 0.5f) - m[4]) / m[0];
            baseline = (floorf(m[3] * baseline + m[5] + 0.5f) - m[5]) / m[3];
        }
        while (text < end)
        {
            const Glyph& g = glyph(font, nextChar(text, end));
            if (g.width > 0)
            {
                quad(x + g.x * s, baseline + g.y * s, g.width * s, g.height * s, g.u0, g.v0, g.u1, g.v1, color);
            }
            x += g.advance * s;
        }
        return x;
    }

    float Canvas::text(const Asset& font, const char* text, float x, float y, const Color& color, Align align)
    {
        if (font.font == nullptr)
        {
            return 0;
        }
        const gpu::TextureInfo info = m_device->textureInfo(font.texture);
        use(font.texture, info.uMax, info.vMax);
        const u32 c = pack(color);
        float widest = 0;
        for (const char* p = text;;)
        {
            const char* end = strchr(p, '\n');
            const char* stop = end ? end : p + strlen(p);
            const float width = lineWidth(*font.font, p, stop);
            const float start = align == Align::Center ? x - width * 0.5f : align == Align::Right ? x - width : x;
            textRun(font, p, stop, start, y, c);
            widest = width > widest ? width : widest;
            if (end == nullptr)
            {
                return widest;
            }
            p = end + 1;
            y += font.font->lineHeight * font.font->scale;
        }
    }

    Vec2 Canvas::measure(const Asset& font, const char* text) const
    {
        if (font.font == nullptr)
        {
            return {};
        }
        Vec2 size;
        for (const char* p = text;;)
        {
            const char* end = strchr(p, '\n');
            const float width = lineWidth(*font.font, p, end ? end : p + strlen(p));
            size.x = width > size.x ? width : size.x;
            size.y += font.font->lineHeight * font.font->scale;
            if (end == nullptr)
            {
                return size;
            }
            p = end + 1;
        }
    }

    float Canvas::paragraph(const Asset& font, const char* text, float x, float y, float width, const Color& color)
    {
        if (font.font == nullptr)
        {
            return 0;
        }
        const gpu::TextureInfo info = m_device->textureInfo(font.texture);
        use(font.texture, info.uMax, info.vMax);
        const u32 c = pack(color);
        const float lineHeight = font.font->lineHeight * font.font->scale;
        const float top = y;
        const char* line = text;
        while (*line)
        {
            // As many whole words as fit, but always at least one.
            const char* best = nullptr;
            const char* p = line;
            while (*p && *p != '\n')
            {
                const char* wordEnd = p;
                while (*wordEnd == ' ')
                {
                    wordEnd++;
                }
                while (*wordEnd && *wordEnd != ' ' && *wordEnd != '\n')
                {
                    wordEnd++;
                }
                if (best && lineWidth(*font.font, line, wordEnd) > width)
                {
                    break;
                }
                best = p = wordEnd;
            }
            const char* stop = best ? best : line;
            textRun(font, line, stop, x, y, c);
            y += lineHeight;
            line = stop;
            while (*line == ' ')
            {
                line++;
            }
            if (*line == '\n')
            {
                line++;
            }
        }
        return y - top;
    }
}
