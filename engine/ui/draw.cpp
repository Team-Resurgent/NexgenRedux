#include "ui/ui.h"

#include <math.h>
#include <string.h>

namespace nx
{
    namespace
    {
        float mix(float a, float b, float t) { return a + (b - a) * t; }
        Color mix(const Color& a, const Color& b, float t) { return { mix(a.r, b.r, t), mix(a.g, b.g, t), mix(a.b, b.b, t), mix(a.a, b.a, t) }; }
        Color times(const Color& a, const Color& b) { return { a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a }; }
        Color faded(const Color& c, float a) { return { c.r, c.g, c.b, c.a * a }; }

        // In the order of the bits that theme.cpp gives them.
        void put(UiLook& out, const UiLook& from, u16 given)
        {
            if (given & 1) out.back = from.back;
            if (given & 2) out.text = from.text;
            if (given & 4) out.line = from.line;
            if (given & 8) out.track = from.track;
            if (given & 16) out.lineWidth = from.lineWidth;
            if (given & 32) out.radius = from.radius;
            if (given & 64) out.scale = from.scale;
            if (given & 128) out.shadow = from.shadow;
        }
    }

    void Ui::animate(Widget& w, float dt)
    {
        if (!w.visible)
        {
            return;
        }
        if (w.style)
        {
            const UiStyle& s = *w.style;
            const bool on[] = { true, m_hovered == &w, w.checked || (w.kind == WidgetKind::Button && w.parent && w.parent->parent && w.parent->parent->kind == WidgetKind::Tabs && w.parent->parent->chosen == w.tag),
                m_focus == &w || m_editing == &w, w.pressed > 0 || (m_held == &w && m_hovered == &w), !w.enabled };
            UiLook wanted = s.looks[0];
            for (u32 i = 1; i < static_cast<u32>(UiState::Count); i++)
            {
                if (on[i])
                {
                    put(wanted, s.looks[i], s.given[i]);
                }
            }
            const float k = !w.lookReady || s.seconds <= 0 ? 1 : 1 - powf(0.05f, dt / s.seconds);
            UiLook& l = w.look;
            l.back = mix(l.back, wanted.back, k);
            l.text = mix(l.text, wanted.text, k);
            l.line = mix(l.line, wanted.line, k);
            l.track = mix(l.track, wanted.track, k);
            l.lineWidth = mix(l.lineWidth, wanted.lineWidth, k);
            l.radius = mix(l.radius, wanted.radius, k);
            l.scale = mix(l.scale, wanted.scale, k);
            l.shadow = mix(l.shadow, wanted.shadow, k);
            const float range = w.high - w.low;
            const float part = w.kind == WidgetKind::Toggle ? (w.checked ? 1.0f : 0.0f) : range != 0 ? (w.value - w.low) / range : 0;
            w.amount = mix(w.amount, part, !w.lookReady || m_held == &w ? 1 : 1 - powf(0.02f, dt / 0.15f));
            w.lookReady = true;
        }
        w.pressed = w.pressed > dt ? w.pressed - dt : 0;
        w.scroll = fabsf(w.scrollTo - w.scroll) < 0.3f ? w.scrollTo : mix(w.scroll, w.scrollTo, m_dragged == &w ? 1 : 1 - powf(0.001f, dt / 0.35f));
        for (Widget* child : w.children)
        {
            animate(*child, dt);
        }
    }

    void Ui::drawBox(Canvas& c, const UiLook& look, const UiStyle& style, float x, float y, float w, float h)
    {
        if (look.shadow > 0)
        {
            c.roundRect(x - 2, y + 5, w + 4, h + 3, look.radius + 3, { 0, 0, 0, look.shadow * 0.5f });
            c.roundRect(x + 1, y + 2, w - 2, h + 3, look.radius + 1, { 0, 0, 0, look.shadow * 0.5f });
        }
        if (style.image && style.image->ready)
        {
            c.nineSlice(*style.image, x, y, w, h, style.slice, look.back);
        }
        else if (look.back.a > 0.003f)
        {
            if (look.radius > 0.5f)
            {
                c.roundRect(x, y, w, h, look.radius, look.back);
            }
            else
            {
                c.rect(x, y, w, h, look.back);
            }
        }
        if (look.lineWidth > 0.2f && look.line.a > 0.003f)
        {
            c.roundLine(x, y, w, h, look.radius, look.lineWidth, look.line);
        }
    }

    void Ui::drawText(Canvas& c, const Widget& w, const char* text, float x, float y, float width, float height, const Color& color, Align align)
    {
        if (w.style == nullptr || w.style->font == nullptr || *text == 0)
        {
            return;
        }
        const Asset& font = *w.style->font;
        if (w.wrap)
        {
            c.paragraph(font, text, x, y, width, color, align);
            return;
        }
        u32 lines = 1;
        for (const char* p = text; *p; p++)
        {
            lines += *p == '\n';
        }
        // The font's line is taller below the letters than above them.
        const float tall = lineHeight(w) * static_cast<float>(lines);
        const float top = y + (height - tall) * 0.5f;
        c.text(font, text, align == Align::Center ? x + width * 0.5f : align == Align::Right ? x + width : x, top, color, align);
    }

    void Ui::drawField(Canvas& c, Widget& w, float x, float y)
    {
        const float left = w.pad(0), inner = w.w - left - w.pad(2);
        const bool editing = m_editing == &w;
        std::string text = w.text;
        u32 caret = w.caret < text.size() ? w.caret : static_cast<u32>(text.size());
        if (w.secret)
        {
            u32 letters = 0, before = 0;
            for (u32 i = 0; i < text.size(); i++)
            {
                const bool first = (static_cast<u8>(text[i]) & 0xC0) != 0x80;
                letters += first;
                before += first && i < caret;
            }
            text.assign(letters, '*');
            caret = before;
        }
        const float at = textWidth(w, text.substr(0, caret).c_str());
        if (editing)
        {
            w.textShift = at - w.textShift > inner - 4 ? at - inner + 4 : at < w.textShift ? at : w.textShift;
        }
        else
        {
            w.textShift = 0;
        }
        c.push();
        c.clip(x + left - 2, y, inner + 4, w.h);
        if (text.empty() && !w.hint.empty())
        {
            const char* hint = w.hint[0] == '@' ? word(w.hint.c_str() + 1) : w.hint.c_str();
            drawText(c, w, hint, x + left, y, inner, w.h, faded(w.look.text, 0.45f), Align::Left);
        }
        drawText(c, w, text.c_str(), x + left - w.textShift, y, inner, w.h, w.look.text, Align::Left);
        if (editing && fmodf(m_time, 1.0f) < 0.55f)
        {
            const float tall = lineHeight(w) * 0.95f;
            c.rect(x + left + at - w.textShift, y + (w.h - tall) * 0.5f, 2, tall, w.look.text);
        }
        c.pop();
    }

    void Ui::drawScrollBar(Canvas& c, const Widget& w, float x, float y)
    {
        if (w.contentH <= w.h + 1)
        {
            return;
        }
        const UiStyle* bar = m_theme.find("bar");
        const float length = w.h * w.h / w.contentH;
        const float tall = length < 28 ? 28 : length;
        const float at = w.scroll / (w.contentH - w.h) * (w.h - tall);
        c.roundRect(x + w.w + 5, y, 5, w.h, 2.5f, bar->looks[0].track);
        c.roundRect(x + w.w + 5, y + at, 5, tall, 2.5f, bar->looks[0].line);
    }

    void Ui::drawWidget(Canvas& c, Widget& w, float px, float py, float alpha)
    {
        if (!w.visible || w.style == nullptr)
        {
            return;
        }
        float x = px + w.x, y = py + w.y;
        const UiLook& look = w.look;
        const UiStyle& style = *w.style;
        const bool scaled = fabsf(look.scale - 1) > 0.001f;
        if (scaled)
        {
            c.push();
            c.translate(x + w.w * 0.5f, y + w.h * 0.5f);
            c.scale(look.scale, look.scale);
            c.translate(-(x + w.w * 0.5f), -(y + w.h * 0.5f));
        }
        const bool boxed = w.kind != WidgetKind::Progress && w.kind != WidgetKind::Line && w.kind != WidgetKind::Slider;
        if (boxed || w.kind == WidgetKind::Slider)
        {
            drawBox(c, look, style, x, y, w.w, w.h);
        }
        const float left = x + w.pad(0), top = y + w.pad(1);
        const float inner = w.w - w.pad(0) - w.pad(2), innerH = w.h - w.pad(1) - w.pad(3);
        const Color ink = times(look.text, w.kind == WidgetKind::Image ? Color{ 1, 1, 1, 1 } : w.color);
        switch (w.kind)
        {
        case WidgetKind::Label:
            drawText(c, w, shown(w), left, top, inner, innerH, ink, style.align);
            break;
        case WidgetKind::Image:
            if (w.image && w.image->ready)
            {
                c.image(*w.image, left, top, inner, innerH, w.color);
            }
            break;
        case WidgetKind::Button:
        {
            float textX = left, wide = inner;
            const float tall = w.children.count() ? lineHeight(w) : innerH;
            if (w.image && w.image->ready)
            {
                const float size = lineHeight(w) * 1.2f;
                const float whole = size + (w.text.empty() ? 0 : w.space() + textWidth(w, shown(w)));
                const float from = style.align == Align::Center ? left + (inner - whole) * 0.5f : left;
                c.image(*w.image, from, top + (tall - size) * 0.5f, size, size, w.color);
                textX = from + size + w.space();
                wide = left + inner - textX;
                drawText(c, w, shown(w), textX, top, wide, tall, ink, Align::Left);
            }
            else
            {
                drawText(c, w, shown(w), textX, top, wide, tall, ink, style.align);
            }
            break;
        }
        case WidgetKind::Toggle:
        {
            drawText(c, w, shown(w), left, top, inner, innerH, ink, Align::Left);
            const float sx = left + inner - 54, sy = y + w.h * 0.5f - 14;
            c.roundRect(sx, sy, 54, 28, 14, mix(look.track, look.line, w.amount));
            c.circle(sx + 14 + 26 * w.amount, sy + 14, 10, { 1, 1, 1, look.text.a });
            break;
        }
        case WidgetKind::Slider:
        case WidgetKind::Progress:
        {
            const bool slider = w.kind == WidgetKind::Slider;
            const float thick = slider ? 8 : w.h;
            const float knob = slider ? 11.0f : 0.0f;
            const float from = left + knob, length = inner - knob * 2;
            const float ty = y + (w.h - thick) * 0.5f;
            const float part = w.amount < 0 ? 0 : w.amount > 1 ? 1 : w.amount;
            c.roundRect(from, ty, length, thick, thick * 0.5f, look.track);
            if (length * part > thick)
            {
                c.roundRect(from, ty, length * part, thick, thick * 0.5f, look.line);
            }
            if (slider)
            {
                c.circle(from + length * part, y + w.h * 0.5f, knob, { 1, 1, 1, look.text.a });
            }
            break;
        }
        case WidgetKind::Choice:
        {
            const std::string none;
            const std::string& item = w.chosen >= 0 && static_cast<size_t>(w.chosen) < w.items.size() ? w.items[static_cast<size_t>(w.chosen)] : none;
            drawText(c, w, !item.empty() && item[0] == '@' ? word(item.c_str() + 1) : item.c_str(), left + 22, top, inner - 44, innerH, ink, style.align);
            const float cy = y + w.h * 0.5f;
            const Color arrow = faded(ink, m_focus == &w ? 1.0f : 0.5f);
            const Color first = faded(arrow, w.chosen > 0 ? 1.0f : 0.25f), last = faded(arrow, static_cast<size_t>(w.chosen + 1) < w.items.size() ? 1.0f : 0.25f);
            c.triangle({ left, cy }, { left + 11, cy - 8 }, { left + 11, cy + 8 }, first, first, first);
            c.triangle({ left + inner, cy }, { left + inner - 11, cy - 8 }, { left + inner - 11, cy + 8 }, last, last, last);
            break;
        }
        case WidgetKind::Field:
            drawField(c, w, x, y);
            break;
        case WidgetKind::Line:
            c.rect(left, y + w.h * 0.5f - 1, inner, 2, look.line);
            break;
        case WidgetKind::Custom:
            if (asIScriptFunction* f = w.events[static_cast<size_t>(UiEvent::Draw)])
            {
                c.push();
                c.clip(x, y, w.w, w.h);
                const bool ok = m_host.callEvent(f, &c, &w, nullptr);
                c.pop();
                if (!ok && m_error[0] == 0)
                {
                    snprintf(m_error, sizeof(m_error), "%s", m_host.lastError());
                }
            }
            break;
        default:
            break;
        }
        const bool clips = w.kind == WidgetKind::Scroll || w.kind == WidgetKind::List;
        if (clips)
        {
            c.push();
            // Room for what is in it to swell a little.
            c.clip(x - 6, y, w.w + 12, w.h);
        }
        Widget* last = nullptr;
        for (Widget* child : w.children)
        {
            const float cy = child->y - w.scroll;
            if (clips && (cy > w.h || cy + child->h < 0))
            {
                continue;
            }
            // Over its neighbours, as it swells.
            if (child == m_focus)
            {
                last = child;
                continue;
            }
            drawWidget(c, *child, x, y - w.scroll, alpha);
        }
        if (last)
        {
            drawWidget(c, *last, x, y - w.scroll, alpha);
        }
        if (clips)
        {
            c.pop();
            drawScrollBar(c, w, x, y);
        }
        if (scaled)
        {
            c.pop();
        }
    }

    void Ui::draw(Canvas& c, float x, float y, float w, float h)
    {
        if (x != m_x || y != m_y || w != m_w || h != m_h || !m_drawn)
        {
            m_x = x;
            m_y = y;
            m_w = w;
            m_h = h;
            m_drawn = true;
            m_dirty = true;
        }
        if (m_dirty)
        {
            layout();
        }
        c.push();
        c.translate(x, y);
        drawWidget(c, *m_root, 0, 0, 1);
        for (u32 i = 0; i < m_layerCount; i++)
        {
            Layer& layer = m_layers[i];
            const float t = layer.shown * layer.shown * (3 - 2 * layer.shown);
            const float before = c.alpha();
            c.push();
            c.setAlpha(before * t);
            if (layer.dim)
            {
                c.rect(0, 0, m_w, m_h, m_theme.find("shade")->looks[0].back);
            }
            Widget& root = *layer.root;
            const float grow = layer.place == LayerPlace::Center ? 0.94f + 0.06f * t : 1;
            const float cx = root.x + root.w * 0.5f, cy = root.y + root.h * 0.5f;
            c.translate(cx, cy + (layer.place == LayerPlace::Bottom ? (1 - t) * 60 : 0));
            c.scale(grow, grow);
            c.translate(-cx, -cy);
            drawWidget(c, root, 0, 0, t);
            c.pop();
        }
        const UiStyle* style = m_theme.find("toast");
        float at = m_h - 70 - safeArea * m_h;
        for (u32 i = 0; i < m_toastCount; i++)
        {
            const Toast& toast = m_toasts[i];
            const float a = toast.shown < toast.left * 4 ? toast.shown : toast.left * 4;
            const float t = a < 0 ? 0 : a > 1 ? 1 : a;
            if (style && style->font)
            {
                const Vec2 size = m_canvas.measure(*style->font, toast.text);
                const float tw = size.x + style->padding[0] + style->padding[2], th = size.y + style->padding[1] + style->padding[3];
                c.push();
                c.setAlpha(c.alpha() * t);
                drawBox(c, style->looks[0], *style, (m_w - tw) * 0.5f, at - th * 0.5f + (1 - t) * 20, tw, th);
                c.text(*style->font, toast.text, m_w * 0.5f, at - size.y * 0.5f + (1 - t) * 20, style->looks[0].text, Align::Center);
                c.pop();
                at -= (th + 10) * t;
            }
        }
        c.pop();
    }
}
