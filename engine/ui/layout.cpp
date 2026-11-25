#include "ui/ui.h"

#include <math.h>
#include <string.h>

// Widths first, from the smallest up and then from the page down; then heights the same way, as how tall
// some text is depends on how wide it may be.
namespace nx
{
    namespace
    {
        float larger(float a, float b) { return a > b ? a : b; }
        float smaller(float a, float b) { return a < b ? a : b; }

        float limited(const Widget& w, float width)
        {
            width = larger(width, w.minWidth);
            if (w.style && w.kind >= WidgetKind::Button)
            {
                width = larger(width, w.style->minWidth);
            }
            return w.maxWidth > 0 ? smaller(width, w.maxWidth) : width;
        }

        UiPlace across(const Widget& w, const Widget& child)
        {
            if (child.self != UiPlace::Inherit)
            {
                return child.self;
            }
            if (w.align != UiPlace::Inherit)
            {
                return w.align;
            }
            return w.kind == WidgetKind::Row ? UiPlace::Center : UiPlace::Stretch;
        }

        bool column(const Widget& w)
        {
            return w.kind == WidgetKind::Column || w.kind == WidgetKind::Scroll || w.kind == WidgetKind::Button || w.kind == WidgetKind::Tabs;
        }

        // Gives way when there isn't room.
        bool shrinks(const Widget& w)
        {
            return w.kind == WidgetKind::Scroll || w.kind == WidgetKind::List || w.kind == WidgetKind::Custom;
        }
    }

    float Ui::lineHeight(const Widget& w) const
    {
        const Asset* font = w.style ? w.style->font : nullptr;
        return font && font->font ? font->font->lineHeight * font->font->scale : 24;
    }

    float Ui::textWidth(const Widget& w, const char* text) const
    {
        return w.style && w.style->font ? m_canvas.measure(*w.style->font, text).x : 0;
    }

    float Ui::textHeight(const Widget& w, float width) const
    {
        const float line = lineHeight(w);
        if (!w.wrap || w.style == nullptr || w.style->font == nullptr)
        {
            u32 lines = 1;
            for (const char* p = shown(w); *p; p++)
            {
                lines += *p == '\n';
            }
            return line * static_cast<float>(lines);
        }
        return m_canvas.paragraphHeight(*w.style->font, shown(w), width);
    }

    u32 Ui::gridColumns(const Widget& w, float inner) const
    {
        if (w.columns)
        {
            return w.columns;
        }
        const float each = larger(w.columnWidth, 1);
        const float fit = w.columnWidth > 0 ? (inner + w.space()) / (each + w.space()) : 1;
        return fit < 1 ? 1 : static_cast<u32>(fit + 0.001f);
    }

    float Ui::naturalWidth(Widget& w)
    {
        if (!w.visible)
        {
            return w.naturalW = 0;
        }
        const float pad = w.pad(0) + w.pad(2);
        float width = 0;
        switch (w.kind)
        {
        case WidgetKind::Label: width = textWidth(w, shown(w)); break;
        case WidgetKind::Image: width = w.image ? static_cast<float>(w.image->width) : 0; break;
        case WidgetKind::Toggle: width = textWidth(w, shown(w)) + (w.text.empty() ? 0 : w.space()) + 58; break;
        case WidgetKind::Field: width = textWidth(w, "nnnnnnnnnnnnnnnn"); break;
        case WidgetKind::Slider:
        case WidgetKind::Progress: width = 200; break;
        case WidgetKind::Choice:
            for (const std::string& item : w.items)
            {
                width = larger(width, textWidth(w, item.empty() || item[0] != '@' ? item.c_str() : word(item.c_str() + 1)));
            }
            width += 56;
            break;
        case WidgetKind::List: width = 280; break;
        default: break;
        }
        if (w.kind == WidgetKind::Button)
        {
            width = textWidth(w, shown(w));
            if (w.image)
            {
                width += lineHeight(w) * 1.2f + (w.text.empty() ? 0 : w.space());
            }
        }
        float sum = 0, most = 0;
        u32 seen = 0;
        for (Widget* child : w.children)
        {
            const float n = naturalWidth(*child);
            if (child->visible)
            {
                sum += n;
                most = larger(most, n);
                seen++;
            }
        }
        if (w.kind == WidgetKind::Row)
        {
            width = sum + (seen ? static_cast<float>(seen - 1) * w.space() : 0);
        }
        else if (w.kind == WidgetKind::Grid)
        {
            const u32 columns = w.columns ? w.columns : 1;
            width = larger(most, w.columnWidth) * static_cast<float>(columns) + static_cast<float>(columns - 1) * w.space();
        }
        else if (w.kind != WidgetKind::List)
        {
            width = larger(width, most);
        }
        width = w.width > 0 ? w.width : limited(w, width + pad);
        return w.naturalW = width;
    }

    void Ui::giveWidth(Widget& w, float width)
    {
        w.w = width;
        const float inner = larger(width - w.pad(0) - w.pad(2), 0);
        if (w.kind == WidgetKind::Row)
        {
            float fixed = 0, grow = 0, soft = 0;
            u32 seen = 0;
            for (const Widget* child : w.children)
            {
                if (child->visible)
                {
                    seen++;
                    grow += child->grow;
                    fixed += child->grow > 0 ? 0 : child->naturalW;
                    soft += child->grow <= 0 && child->kind == WidgetKind::Label && child->width <= 0 ? child->naturalW : 0;
                }
            }
            const float left = inner - fixed - (seen ? static_cast<float>(seen - 1) * w.space() : 0);
            for (Widget* child : w.children)
            {
                float give = child->naturalW;
                if (child->grow > 0)
                {
                    give = larger(left, 0) * child->grow / grow;
                    give = child->maxWidth > 0 ? smaller(give, child->maxWidth) : give;
                }
                else if (left < 0 && soft > 0 && child->kind == WidgetKind::Label && child->width <= 0)
                {
                    // Words give way first.
                    give = larger(give + left * give / soft, 0);
                }
                giveWidth(*child, child->visible ? give : 0);
            }
            return;
        }
        float cell = inner;
        if (w.kind == WidgetKind::Grid || w.kind == WidgetKind::List)
        {
            const u32 columns = gridColumns(w, inner);
            cell = larger((inner - static_cast<float>(columns - 1) * w.space()) / static_cast<float>(columns), 0);
        }
        for (Widget* child : w.children)
        {
            const bool stretch = across(w, *child) == UiPlace::Stretch && child->width <= 0;
            float give = stretch ? cell : smaller(child->naturalW, cell);
            give = child->maxWidth > 0 ? smaller(give, child->maxWidth) : give;
            giveWidth(*child, child->visible ? give : 0);
        }
    }

    float Ui::naturalHeight(Widget& w)
    {
        if (!w.visible)
        {
            return w.naturalH = 0;
        }
        const float pad = w.pad(1) + w.pad(3);
        const float inner = larger(w.w - w.pad(0) - w.pad(2), 0);
        float height = 0;
        switch (w.kind)
        {
        case WidgetKind::Label: height = textHeight(w, inner); break;
        case WidgetKind::Image: height = w.image && w.image->width ? inner * static_cast<float>(w.image->height) / static_cast<float>(w.image->width) : 0; break;
        case WidgetKind::Toggle: height = larger(lineHeight(w), 28); break;
        case WidgetKind::Field:
        case WidgetKind::Choice: height = lineHeight(w); break;
        case WidgetKind::Slider: height = 26; break;
        case WidgetKind::Progress: height = 10; break;
        case WidgetKind::Line: height = 2; break;
        case WidgetKind::Button: height = w.children.count() && w.text.empty() ? 0 : lineHeight(w); break;
        default: break;
        }
        float sum = 0, most = 0, row = 0;
        u32 seen = 0, cells = 0;
        const u32 columns = w.kind == WidgetKind::Grid ? gridColumns(w, inner) : 1;
        for (Widget* child : w.children)
        {
            const float n = naturalHeight(*child);
            if (!child->visible)
            {
                continue;
            }
            most = larger(most, n);
            row = larger(row, n);
            cells++;
            if (cells % columns == 0)
            {
                sum += row;
                row = 0;
                seen++;
            }
        }
        if (cells % columns)
        {
            sum += row;
            seen++;
        }
        const float stacked = sum + (seen ? static_cast<float>(seen - 1) * w.space() : 0);
        if (w.kind == WidgetKind::List)
        {
            const u32 rows = (w.count + gridColumns(w, inner) - 1) / gridColumns(w, inner);
            height = rows ? static_cast<float>(rows) * (w.rowHeight + w.space()) - w.space() : 0;
        }
        else if (w.kind == WidgetKind::Row || w.kind == WidgetKind::Stack)
        {
            height = larger(height, most);
        }
        else if (column(w) || w.kind == WidgetKind::Grid)
        {
            height += w.children.count() ? stacked : 0;
        }
        height += pad;
        if (w.style && w.kind >= WidgetKind::Button && w.kind != WidgetKind::Space)
        {
            height = larger(height, w.style->minHeight);
        }
        return w.naturalH = w.height > 0 ? w.height : height;
    }

    void Ui::giveHeight(Widget& w, float height)
    {
        w.h = height;
        const float left = w.pad(0), top = w.pad(1);
        const float innerW = larger(w.w - left - w.pad(2), 0), innerH = larger(height - top - w.pad(3), 0);
        const float gap = w.space();
        if (w.kind == WidgetKind::List)
        {
            const u32 columns = gridColumns(w, innerW);
            const float cell = (innerW - static_cast<float>(columns - 1) * gap) / static_cast<float>(columns);
            const u32 rows = (w.count + columns - 1) / columns;
            w.contentH = (rows ? static_cast<float>(rows) * (w.rowHeight + gap) - gap : 0) + top + w.pad(3);
            for (Widget* child : w.children)
            {
                const u32 index = child->index < 0 ? 0 : static_cast<u32>(child->index);
                child->x = left + static_cast<float>(index % columns) * (cell + gap);
                child->y = top + static_cast<float>(index / columns) * (w.rowHeight + gap);
                giveHeight(*child, w.rowHeight);
            }
        }
        else if (w.kind == WidgetKind::Row)
        {
            float total = 0;
            u32 seen = 0;
            for (const Widget* child : w.children)
            {
                total += child->visible ? child->w : 0;
                seen += child->visible;
            }
            float spread = gap;
            const float over = innerW - total - (seen ? static_cast<float>(seen - 1) * gap : 0);
            float at = left;
            if (over > 0)
            {
                at += w.justify == UiPlace::Center ? over * 0.5f : w.justify == UiPlace::End ? over : 0;
                spread += w.justify == UiPlace::Spread && seen > 1 ? over / static_cast<float>(seen - 1) : 0;
            }
            for (Widget* child : w.children)
            {
                const UiPlace place = across(w, *child);
                const float tall = place == UiPlace::Stretch && child->height <= 0 ? innerH : smaller(child->naturalH, larger(innerH, child->naturalH));
                child->x = at;
                child->y = top + (place == UiPlace::Center ? (innerH - tall) * 0.5f : place == UiPlace::End ? innerH - tall : 0);
                giveHeight(*child, tall);
                at += child->visible ? child->w + spread : 0;
            }
        }
        else if (w.kind == WidgetKind::Grid)
        {
            const u32 columns = gridColumns(w, innerW);
            const float cell = (innerW - static_cast<float>(columns - 1) * gap) / static_cast<float>(columns);
            float y = top, row = 0;
            u32 cells = 0;
            for (u32 i = 0; i < w.children.count(); i++)
            {
                Widget& child = *w.children[i];
                if (!child.visible)
                {
                    continue;
                }
                if (cells % columns == 0)
                {
                    y += cells ? row + gap : 0;
                    row = 0;
                    // As tall as the tallest of it.
                    u32 ahead = 0;
                    for (u32 j = i; j < w.children.count() && ahead < columns; j++)
                    {
                        if (w.children[j]->visible)
                        {
                            row = larger(row, w.children[j]->naturalH);
                            ahead++;
                        }
                    }
                }
                const UiPlace place = across(w, child);
                const float tall = place == UiPlace::Stretch && child.height <= 0 ? row : child.naturalH;
                child.x = left + static_cast<float>(cells % columns) * (cell + gap) + (place == UiPlace::Center ? (cell - child.w) * 0.5f : place == UiPlace::End ? cell - child.w : 0);
                child.y = y + (place == UiPlace::Center ? (row - tall) * 0.5f : 0);
                giveHeight(child, tall);
                cells++;
            }
        }
        else if (w.kind == WidgetKind::Stack)
        {
            for (Widget* child : w.children)
            {
                const UiPlace place = across(w, *child);
                const float tall = place == UiPlace::Stretch && child->height <= 0 ? innerH : smaller(child->naturalH, innerH);
                child->x = left + (place == UiPlace::Center ? (innerW - child->w) * 0.5f : place == UiPlace::End ? innerW - child->w : 0);
                child->y = top + (place == UiPlace::Center ? (innerH - tall) * 0.5f : place == UiPlace::End ? innerH - tall : 0);
                giveHeight(*child, tall);
            }
        }
        else if (column(w))
        {
            const bool scrolls = w.kind == WidgetKind::Scroll;
            float total = 0, grow = 0, soft = 0;
            u32 seen = 0;
            for (const Widget* child : w.children)
            {
                if (child->visible)
                {
                    seen++;
                    grow += child->grow;
                    total += child->grow > 0 && !scrolls ? 0 : child->naturalH;
                    soft += child->grow <= 0 && shrinks(*child) ? child->naturalH : 0;
                }
            }
            const float above = w.kind == WidgetKind::Button && !w.text.empty() ? lineHeight(w) + gap : 0;
            const float over = innerH - above - total - (seen ? static_cast<float>(seen - 1) * gap : 0);
            w.contentH = scrolls ? total + (seen ? static_cast<float>(seen - 1) * gap : 0) + top + w.pad(3) : height;
            float spread = gap, at = top + above;
            if (over > 0 && grow <= 0 && !scrolls)
            {
                at += w.justify == UiPlace::Center ? over * 0.5f : w.justify == UiPlace::End ? over : 0;
                spread += w.justify == UiPlace::Spread && seen > 1 ? over / static_cast<float>(seen - 1) : 0;
            }
            for (Widget* child : w.children)
            {
                float tall = child->naturalH;
                if (!scrolls)
                {
                    if (child->grow > 0)
                    {
                        tall = larger(over, 0) * child->grow / grow;
                    }
                    else if (over < 0 && soft > 0 && shrinks(*child))
                    {
                        tall = larger(tall + over * tall / soft, 0);
                    }
                }
                const UiPlace place = across(w, *child);
                child->x = left + (place == UiPlace::Center ? (innerW - child->w) * 0.5f : place == UiPlace::End ? innerW - child->w : 0);
                child->y = at;
                giveHeight(*child, child->visible ? tall : 0);
                at += child->visible ? tall + spread : 0;
            }
        }
        const float furthest = larger(w.contentH - w.h, 0);
        if (w.kind == WidgetKind::Scroll || w.kind == WidgetKind::List)
        {
            w.scrollTo = smaller(larger(w.scrollTo, 0), furthest);
            w.scroll = smaller(larger(w.scroll, 0), furthest);
        }
    }

    void Ui::layoutLayer(Widget& root, float w, float h)
    {
        naturalWidth(root);
        giveWidth(root, w);
        naturalHeight(root);
        giveHeight(root, h);
    }

    void Ui::layout()
    {
        m_dirty = false;
        const float safe = safeArea * m_h;
        const float width = larger(m_w - safe * 2, 0), height = larger(m_h - safe * 2, 0);
        layoutLayer(*m_root, width, height);
        m_root->x = safe;
        m_root->y = safe;
        for (u32 i = 0; i < m_layerCount; i++)
        {
            Layer& layer = m_layers[i];
            Widget& root = *layer.root;
            naturalWidth(root);
            float wide = smaller(root.naturalW, width);
            float ax = 0, ay = 0;
            if (layer.place == LayerPlace::Below && layer.anchor)
            {
                corner(*layer.anchor, ax, ay);
                wide = smaller(larger(wide, layer.anchor->w), width);
            }
            giveWidth(root, wide);
            naturalHeight(root);
            const float tall = smaller(root.naturalH, height);
            giveHeight(root, tall);
            if (layer.place == LayerPlace::Below && layer.anchor)
            {
                const float below = ay + layer.anchor->h + 6;
                root.x = smaller(ax, safe + width - wide);
                // Above it, if there is more room there.
                root.y = below + tall <= safe + height ? below : larger(smaller(ay - 6 - tall, safe + height - tall), safe);
            }
            else
            {
                root.x = safe + (width - wide) * 0.5f;
                root.y = layer.place == LayerPlace::Bottom ? safe + height - tall - 12 : safe + (height - tall) * 0.5f;
            }
        }
    }
}
