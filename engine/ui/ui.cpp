#include "ui/ui.h"
#include "base/log.h"

#include <angelscript.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr float kPressedFor = 0.12f;
        constexpr float kWheelStep = 90;

        const char* const kStyles[] = { "plain", "plain", "plain", "plain", "plain", "plain", "plain", "button", "toggle", "slider", "progress", "choice", "field", "plain", "plain", "plain", "line", "plain" };
        static_assert(sizeof(kStyles) / sizeof(kStyles[0]) == static_cast<size_t>(WidgetKind::Count));

        float larger(float a, float b) { return a > b ? a : b; }
        float smaller(float a, float b) { return a < b ? a : b; }
    }

    // --- Widgets ---

    Widget::~Widget()
    {
        for (Widget* child : children)
        {
            child->parent = nullptr;
            child->release();
        }
        for (asIScriptFunction* f : events)
        {
            if (f)
            {
                f->Release();
            }
        }
    }

    void Widget::release()
    {
        if (--refs > 0)
        {
            return;
        }
        if (ui)
        {
            ui->forget(*this);
            if (image)
            {
                ui->assets().release(image);
            }
            if (previousAll)
            {
                previousAll->nextAll = nextAll;
            }
            else
            {
                ui->m_all = nextAll;
            }
            if (nextAll)
            {
                nextAll->previousAll = previousAll;
            }
        }
        destroy(this);
    }

    bool Widget::focusable() const
    {
        if (!visible || !enabled)
        {
            return false;
        }
        switch (kind)
        {
        case WidgetKind::Button:
        case WidgetKind::Toggle:
        case WidgetKind::Slider:
        case WidgetKind::Choice:
        case WidgetKind::Field: return true;
        default: return index >= 0 && parent && parent->kind == WidgetKind::List;
        }
    }

    void Widget::changed()
    {
        if (ui)
        {
            ui->touch();
        }
    }

    Widget* Widget::add(WidgetKind what, const char* style_)
    {
        if (ui == nullptr)
        {
            return nullptr;
        }
        Widget* child = ui->make(what, style_);
        if (child)
        {
            child->parent = this;
            children.push(child);
            child->addRef();   // the caller's
            ui->touch();
        }
        return child;
    }

    Widget* Widget::addTabs()
    {
        Widget* tabs = add(WidgetKind::Tabs, "");
        if (tabs)
        {
            tabs->grow = 1;
            tabs->gap = 14;
            Widget* header = tabs->add(WidgetKind::Row, "");
            header->gap = 8;
            header->release();
            Widget* pages = tabs->add(WidgetKind::Stack, "");
            pages->grow = 1;
            pages->release();
        }
        return tabs;
    }

    Widget* Widget::addTab(const char* title)
    {
        if (kind != WidgetKind::Tabs || ui == nullptr || children.count() < 2)
        {
            return nullptr;
        }
        const u32 at = children[1]->children.count();
        Widget* button = children[0]->add(WidgetKind::Button, "tab");
        button->setText(title);
        button->tag = static_cast<int>(at);
        button->release();
        Widget* page = children[1]->add(WidgetKind::Column, "");
        page->gap = 12;
        page->visible = static_cast<int>(at) == chosen;
        return page;
    }

    void Widget::remove()
    {
        Widget* from = parent;
        if (from == nullptr)
        {
            return;
        }
        for (u32 i = 0; i < from->children.count(); i++)
        {
            if (from->children[i] == this)
            {
                for (u32 j = i + 1; j < from->children.count(); j++)
                {
                    from->children[j - 1] = from->children[j];
                }
                from->children.resize(from->children.count() - 1);
                break;
            }
        }
        parent = nullptr;
        if (ui)
        {
            ui->forget(*this);
            ui->touch();
        }
        release();
    }

    void Widget::removeAll()
    {
        while (children.count())
        {
            children[children.count() - 1]->remove();
        }
    }

    void Widget::setText(const char* t)
    {
        if (text != t)
        {
            text = t;
            caret = caret < text.size() ? caret : static_cast<u32>(text.size());
            changed();
        }
    }

    void Widget::setEvent(UiEvent event, asIScriptFunction* function)
    {
        asIScriptFunction*& slot = events[static_cast<size_t>(event)];
        if (slot)
        {
            slot->Release();
        }
        slot = function;
        if (ui == nullptr && slot)
        {
            slot->Release();
            slot = nullptr;
        }
    }

    void Widget::setImage(Asset* asset, Assets& assets)
    {
        if (image)
        {
            assets.release(image);
        }
        image = ui ? asset : nullptr;
        if (ui == nullptr && asset)
        {
            assets.release(asset);
        }
        changed();
    }

    Widget* Widget::row(int which)
    {
        for (Widget* child : children)
        {
            if (child->index == which)
            {
                return child;
            }
        }
        return nullptr;
    }

    // --- Ui ---

    Ui::Ui(Canvas& canvas, Assets& assets, Audio& audio, Input& input, ScriptHost& host, Platform& platform, const char* folder)
        : m_canvas(canvas), m_assets(assets), m_audio(audio), m_input(input), m_host(host), m_platform(platform)
    {
        snprintf(m_folder, sizeof(m_folder), "%s", folder);
        m_root = make(WidgetKind::Column, "");
    }

    Ui::~Ui()
    {
        m_input.setTyping(false);
        for (Waiting& w : m_waiting)
        {
            if (w.widget)
            {
                w.widget->release();
            }
            w.function->Release();
        }
        m_waiting.clear();
        for (u32 i = 0; i < m_layerCount; i++)
        {
            if (m_layers[i].answer)
            {
                m_layers[i].answer->Release();
            }
        }
        // Scripts may keep theirs, with nothing left in them that would keep this screen's code.
        for (Widget* w = m_all; w; w = w->nextAll)
        {
            w->addRef();
        }
        for (Widget* w = m_all; w; w = w->nextAll)
        {
            for (u32 i = 0; i < static_cast<u32>(UiEvent::Count); i++)
            {
                w->setEvent(static_cast<UiEvent>(i), nullptr);
            }
            if (w->image)
            {
                m_assets.release(w->image);
                w->image = nullptr;
            }
            for (Widget* child : w->children)
            {
                child->parent = nullptr;
                child->refs--;   // it is held here until the end of this
            }
            w->children.clear();
            w->style = nullptr;
            w->ui = nullptr;
        }
        for (u32 i = 0; i < m_layerCount; i++)
        {
            m_layers[i].root->refs--;
        }
        m_root->refs--;
        for (Widget* w = m_all; w;)
        {
            Widget* next = w->nextAll;
            w->nextAll = w->previousAll = nullptr;
            w->release();
            w = next;
        }
    }

    Widget* Ui::make(WidgetKind kind, const char* style)
    {
        if (!m_themeReady)
        {
            // Only once there is something to draw with it.
            m_themeReady = true;
            m_theme.load(m_assets, nullptr, 0, "", m_folder);
        }
        Widget* w = create<Widget>(MemTag::UI);
        if (w == nullptr)
        {
            return nullptr;
        }
        w->ui = this;
        w->kind = kind;
        w->nextAll = m_all;
        if (m_all)
        {
            m_all->previousAll = w;
        }
        m_all = w;
        snprintf(w->styleName, sizeof(w->styleName), "%s", style && *style ? style : kStyles[static_cast<size_t>(kind)]);
        restyle(*w);
        return w;
    }

    void Ui::restyle(Widget& w)
    {
        w.style = m_theme.find(w.styleName);
        w.lookReady = false;
        m_dirty = true;
    }

    // Nothing may be left that points at it.
    void Ui::forget(Widget& w)
    {
        const auto drop = [&](Widget*& p)
        {
            if (p && (p == &w || inside(*p, w)))
            {
                p = nullptr;
            }
        };
        if (m_editing && (m_editing == &w || inside(*m_editing, w)))
        {
            m_input.setTyping(false);
        }
        drop(m_focus);
        drop(m_hovered);
        drop(m_wanted);
        drop(m_held);
        drop(m_dragged);
        drop(m_editing);
        drop(m_keyboardFor);
        for (u32 i = 0; i < m_layerCount; i++)
        {
            drop(m_layers[i].focus);
            drop(m_layers[i].anchor);
        }
    }

    void Ui::queue(Widget& w, UiEvent event, int number)
    {
        asIScriptFunction* f = w.events[static_cast<size_t>(event)];
        if (f)
        {
            w.addRef();
            f->AddRef();
            m_waiting.push({ &w, f, event, number });
        }
    }

    bool Ui::dispatch()
    {
        bool ok = true;
        // What they ask for may add more, which wait for the next step.
        GrowArray<Waiting, MemTag::UI> now;
        now.swap(m_waiting);
        for (Waiting& w : now)
        {
            if (ok && (w.widget == nullptr || w.widget->ui))
            {
                ok = m_host.callEvent(w.function, w.widget, nullptr, w.widget ? nullptr : &w.number);
            }
            if (w.widget)
            {
                w.widget->release();
            }
            w.function->Release();
        }
        return ok;
    }

    const char* Ui::word(const char* key) const
    {
        for (const Word& w : m_words)
        {
            if (w.key == key)
            {
                return w.text.c_str();
            }
        }
        return key;
    }

    const char* Ui::shown(const Widget& w) const
    {
        return !w.text.empty() && w.text[0] == '@' && w.kind != WidgetKind::Field ? word(w.text.c_str() + 1) : w.text.c_str();
    }

    void Ui::play(Theme::Sound which)
    {
        Asset* sound = m_theme.sound(which);
        if (sound && sound->sound)
        {
            PlayOptions o;
            o.volume = which == Theme::Accept ? 0.6f : 0.4f;
            o.bus = Bus::Interface;
            m_assets.addRef(sound);
            if (!m_audio.play(*sound->sound, sound, o))
            {
                m_assets.release(sound);
            }
        }
    }

    bool Ui::attached(const Widget& w) const
    {
        bool found = inside(w, *m_root);
        for (u32 i = 0; i < m_layerCount; i++)
        {
            found = found || inside(w, *m_layers[i].root);
        }
        return found;
    }

    Widget* Ui::find(const char* id)
    {
        for (Widget* w = m_all; w; w = w->nextAll)
        {
            if (w->id == id && attached(*w))
            {
                return w;
            }
        }
        return nullptr;
    }

    // --- Where things are ---

    void Ui::corner(const Widget& w, float& x, float& y) const
    {
        x = w.x;
        y = w.y;
        for (const Widget* p = w.parent; p; p = p->parent)
        {
            x += p->x;
            y += p->y - p->scroll;
        }
    }

    void Ui::place(const Widget& w, float& x, float& y) const
    {
        corner(w, x, y);
        x += m_x;
        y += m_y;
    }

    bool Ui::inside(const Widget& w, const Widget& root) const
    {
        for (const Widget* p = &w; p; p = p->parent)
        {
            if (p == &root)
            {
                return true;
            }
        }
        return false;
    }

    bool Ui::reachable(const Widget& w) const
    {
        for (const Widget* p = &w; p; p = p->parent)
        {
            if (!p->visible || !p->enabled)
            {
                return false;
            }
            if (p == (m_layerCount ? m_layers[m_layerCount - 1].root : m_root))
            {
                return true;
            }
        }
        return false;
    }

    void Ui::collect(Widget& w, GrowArray<Widget*, MemTag::UI>& out) const
    {
        if (!w.visible || !w.enabled)
        {
            return;
        }
        if (w.focusable())
        {
            out.push(&w);
            return;
        }
        for (Widget* child : w.children)
        {
            collect(*child, out);
        }
    }

    Widget* Ui::firstFocusable(Widget& w) const
    {
        GrowArray<Widget*, MemTag::UI> all;
        collect(w, all);
        // In a list, where it had got to.
        for (Widget* each : all)
        {
            if (each->parent && each->parent->kind == WidgetKind::List && each->index == each->parent->chosen)
            {
                return each;
            }
            if (each->index < 0)
            {
                return each;
            }
        }
        return all.count() ? all[0] : nullptr;
    }

    // --- Focus ---

    void Ui::focus(Widget* w, bool sound)
    {
        if (w == m_focus)
        {
            return;
        }
        if (m_editing && m_editing != w && m_keyboardFor == nullptr)
        {
            endEdit();
        }
        if (w && !w->focusable())
        {
            m_wanted = w;
            w = firstFocusable(*w);
            m_wanted = w ? nullptr : m_wanted;
        }
        m_focus = w;
        if (w)
        {
            if (w->index >= 0 && w->parent && w->parent->kind == WidgetKind::List)
            {
                w->parent->chosen = w->index;
                queue(*w->parent, UiEvent::Focus);
            }
            queue(*w, UiEvent::Focus);
            reveal(*w);
            if (sound)
            {
                play(Theme::Move);
            }
        }
    }

    // Scrolls whatever it is in until it can be seen.
    void Ui::reveal(Widget& w)
    {
        float y = w.y;
        for (Widget* p = w.parent; p; p = p->parent)
        {
            if (p->kind == WidgetKind::Scroll || p->kind == WidgetKind::List)
            {
                const float furthest = larger(p->contentH - p->h, 0);
                const float margin = smaller(p->h * 0.18f, 60);
                if (y - margin < p->scrollTo)
                {
                    p->scrollTo = y - margin;
                }
                else if (y + w.h + margin > p->scrollTo + p->h)
                {
                    p->scrollTo = y + w.h + margin - p->h;
                }
                p->scrollTo = smaller(larger(p->scrollTo, 0), furthest);
                y -= p->scrollTo;
            }
            y += p->y;
        }
    }

    Widget* Ui::neighbour(Widget& from, Action way)
    {
        GrowArray<Widget*, MemTag::UI> all;
        collect(top(), all);
        float fx, fy;
        corner(from, fx, fy);
        Widget* best = nullptr;
        float bestScore = 1e9f;
        for (Widget* w : all)
        {
            if (w == &from)
            {
                continue;
            }
            float x, y;
            corner(*w, x, y);
            // How far along that way, edge to edge, and how far to the side, not counting what overlaps.
            float along, centre, side, sideways;
            const bool level = way == Action::Left || way == Action::Right;
            if (level)
            {
                along = way == Action::Right ? x - (fx + from.w) : fx - (x + w->w);
                centre = (x + w->w * 0.5f - fx - from.w * 0.5f) * (way == Action::Right ? 1.0f : -1.0f);
                side = larger(larger(y - (fy + from.h), fy - (y + w->h)), 0);
                sideways = fabsf(y + w->h * 0.5f - fy - from.h * 0.5f);
            }
            else
            {
                along = way == Action::Down ? y - (fy + from.h) : fy - (y + w->h);
                centre = (y + w->h * 0.5f - fy - from.h * 0.5f) * (way == Action::Down ? 1.0f : -1.0f);
                side = larger(larger(x - (fx + from.w), fx - (x + w->w)), 0);
                sideways = fabsf(x + w->w * 0.5f - fx - from.w * 0.5f);
            }
            if (centre < 2 || along < -smaller(level ? w->w : w->h, level ? from.w : from.h) * 0.5f)
            {
                continue;
            }
            const float score = larger(along, 0) + side * 3 + sideways * 0.15f + (level && side > 0 ? 400.0f : 0.0f);
            if (score < bestScore)
            {
                bestScore = score;
                best = w;
            }
        }
        // Into a list where it was left, rather than at its edge.
        if (best && best->index >= 0 && best->parent && best->parent->kind == WidgetKind::List && (from.parent != best->parent))
        {
            if (Widget* left = best->parent->row(best->parent->chosen))
            {
                best = left;
            }
        }
        return best;
    }

    void Ui::choose(Widget& list, int index, bool sound)
    {
        if (list.kind != WidgetKind::List || list.count == 0)
        {
            return;
        }
        list.chosen = index < 0 ? 0 : index >= static_cast<int>(list.count) ? static_cast<int>(list.count) - 1 : index;
        // The row may not exist yet: it will when the list has come to it.
        const float inner = list.w - list.pad(0) - list.pad(2);
        const u32 columns = gridColumns(list, inner);
        const float y = list.pad(1) + static_cast<float>(static_cast<u32>(list.chosen) / columns) * (list.rowHeight + list.space());
        const float margin = smaller(list.h * 0.18f, list.rowHeight);
        if (y - margin < list.scrollTo)
        {
            list.scrollTo = y - margin;
        }
        else if (y + list.rowHeight + margin > list.scrollTo + list.h)
        {
            list.scrollTo = y + list.rowHeight + margin - list.h;
        }
        list.scrollTo = smaller(larger(list.scrollTo, 0), larger(list.contentH - list.h, 0));
        if (Widget* row = list.row(list.chosen))
        {
            m_focus = row;
        }
        queue(list, UiEvent::Focus);
        if (sound)
        {
            play(Theme::Move);
        }
    }

    void Ui::move(Action way)
    {
        Widget* f = m_focus;
        if (f == nullptr)
        {
            focus(firstFocusable(top()), true);
            return;
        }
        if (f->kind == WidgetKind::Slider && (way == Action::Left || way == Action::Right))
        {
            step(*f, way == Action::Right ? 1 : -1);
            return;
        }
        if (f->kind == WidgetKind::Choice && (way == Action::Left || way == Action::Right))
        {
            step(*f, way == Action::Right ? 1 : -1);
            return;
        }
        if (f->index >= 0 && f->parent && f->parent->kind == WidgetKind::List)
        {
            Widget& list = *f->parent;
            const int columns = static_cast<int>(gridColumns(list, list.w - list.pad(0) - list.pad(2)));
            const int at = list.chosen, count = static_cast<int>(list.count);
            int to = at;
            if (way == Action::Up) to = at - columns;
            else if (way == Action::Down) to = at + columns < count ? at + columns : at / columns < (count - 1) / columns ? count - 1 : at + columns;
            else if (way == Action::Left) to = at % columns ? at - 1 : -1;
            else to = at % columns < columns - 1 && at + 1 < count ? at + 1 : -1;
            if (to >= 0 && to < count)
            {
                choose(list, to, true);
                return;
            }
        }
        if (Widget* next = neighbour(*f, way))
        {
            focus(next, true);
        }
    }

    // --- Doing things ---

    void Ui::slideTo(Widget& w, float value)
    {
        const float low = smaller(w.low, w.high), high = larger(w.low, w.high);
        value = w.step > 0 ? w.low + floorf((value - w.low) / w.step + 0.5f) * w.step : value;
        value = smaller(larger(value, low), high);
        if (value != w.value)
        {
            w.value = value;
            queue(w, UiEvent::Change);
        }
    }

    void Ui::step(Widget& w, int way)
    {
        if (w.kind == WidgetKind::Slider)
        {
            const float before = w.value;
            slideTo(w, w.value + static_cast<float>(way) * (w.step > 0 ? w.step : (w.high - w.low) * 0.05f));
            if (w.value != before)
            {
                play(Theme::Change);
            }
        }
        else if (w.kind == WidgetKind::Choice)
        {
            const int to = w.chosen + way;
            if (to >= 0 && to < static_cast<int>(w.items.size()))
            {
                w.chosen = to;
                queue(w, UiEvent::Change);
                play(Theme::Change);
            }
        }
    }

    void Ui::setTab(Widget& tabs, int index)
    {
        if (tabs.kind != WidgetKind::Tabs || tabs.children.count() < 2)
        {
            return;
        }
        const u32 count = tabs.children[1]->children.count();
        if (index < 0 || static_cast<u32>(index) >= count)
        {
            return;
        }
        tabs.chosen = index;
        for (u32 i = 0; i < count; i++)
        {
            tabs.children[1]->children[i]->visible = static_cast<int>(i) == index;
        }
        if (m_focus && !reachable(*m_focus))
        {
            m_focus = nullptr;
            focus(static_cast<u32>(index) < tabs.children[0]->children.count() ? tabs.children[0]->children[static_cast<u32>(index)] : nullptr);
        }
        queue(tabs, UiEvent::Change);
        m_dirty = true;
    }

    void Ui::act(Widget& w)
    {
        if (!w.enabled)
        {
            return;
        }
        w.pressed = kPressedFor;
        if (w.native)
        {
            w.native(*this, w);
            return;
        }
        if (w.kind == WidgetKind::Button && w.parent && w.parent->parent && w.parent->parent->kind == WidgetKind::Tabs && w.parent == w.parent->parent->children[0])
        {
            if (w.parent->parent->chosen != w.tag)
            {
                setTab(*w.parent->parent, w.tag);
                play(Theme::Change);
            }
            return;
        }
        switch (w.kind)
        {
        case WidgetKind::Toggle:
            w.checked = !w.checked;
            play(Theme::Change);
            queue(w, UiEvent::Change);
            break;
        case WidgetKind::Choice:
            openChoice(w);
            return;
        case WidgetKind::Field:
            beginEdit(w);
            return;
        default:
            play(Theme::Accept);
            break;
        }
        if (w.index >= 0 && w.parent && w.parent->kind == WidgetKind::List)
        {
            w.parent->chosen = w.index;
            queue(*w.parent, UiEvent::Click);
        }
        queue(w, UiEvent::Click);
    }

    // --- In front ---

    void Ui::open(Widget* root, LayerPlace place, Widget* anchor, bool dim)
    {
        if (m_layerCount == kMaxLayers)
        {
            root->release();
            return;
        }
        Layer& layer = m_layers[m_layerCount++];
        layer = {};
        layer.root = root;
        layer.focus = m_focus;
        layer.anchor = anchor;
        layer.place = place;
        layer.dim = dim;
        m_focus = nullptr;
        m_held = m_dragged = nullptr;
        m_dirty = true;
    }

    void Ui::closeTop(int number)
    {
        if (m_layerCount == 0)
        {
            return;
        }
        const Layer layer = m_layers[--m_layerCount];
        if (layer.answer)
        {
            m_waiting.push({ nullptr, layer.answer, UiEvent::Click, number });
        }
        if (m_keyboardFor && layer.place == LayerPlace::Bottom)
        {
            Widget* field = m_keyboardFor;
            m_keyboardFor = nullptr;
            m_editing = nullptr;
            m_input.setTyping(false);
            queue(*field, UiEvent::Done);
        }
        m_focus = nullptr;
        Widget* back = layer.focus;
        layer.root->release();
        m_focus = back && back->focusable() ? back : nullptr;
        m_dirty = true;
    }

    Widget* Ui::popup()
    {
        Widget* root = make(WidgetKind::Column, "dialog");
        root->addRef();   // the caller's
        open(root, LayerPlace::Center, nullptr, true);
        return root;
    }

    void Ui::close(Widget& popup)
    {
        // And whatever is over it.
        for (u32 i = 0; i < m_layerCount; i++)
        {
            if (m_layers[i].root == &popup)
            {
                while (m_layerCount > i)
                {
                    closeTop(-1);
                }
                return;
            }
        }
    }

    void Ui::dialog(const char* title, const char* text, const std::vector<std::string>& buttons, asIScriptFunction* answer)
    {
        Widget* root = make(WidgetKind::Column, "dialog");
        root->maxWidth = 760;
        if (*title)
        {
            Widget* label = root->add(WidgetKind::Label, "heading");
            label->setText(title);
            label->release();
        }
        if (*text)
        {
            Widget* label = root->add(WidgetKind::Label, "plain");
            label->wrap = true;
            label->setText(text);
            label->release();
        }
        Widget* row = root->add(WidgetKind::Row, "plain");
        row->gap = 12;
        row->justify = UiPlace::End;
        for (u32 i = 0; i < buttons.size(); i++)
        {
            Widget* button = row->add(WidgetKind::Button, "button");
            button->setText(buttons[i].c_str());
            button->minWidth = 130;
            button->tag = static_cast<int>(i);
            button->native = [](Ui& ui, Widget& w)
            {
                ui.play(Theme::Accept);
                ui.closeTop(w.tag);
            };
            button->release();
        }
        row->release();
        open(root, LayerPlace::Center, nullptr, true);
        if (m_layerCount && m_layers[m_layerCount - 1].root == root)
        {
            m_layers[m_layerCount - 1].answer = answer;
        }
        else if (answer)
        {
            answer->Release();
        }
    }

    void Ui::openChoice(Widget& w)
    {
        if (w.items.empty())
        {
            return;
        }
        play(Theme::Accept);
        Widget* root = make(WidgetKind::Column, "menu");
        Widget* scroll = root->add(WidgetKind::Scroll, "plain");
        scroll->gap = 4;
        Widget* now = nullptr;
        for (u32 i = 0; i < w.items.size(); i++)
        {
            Widget* item = scroll->add(WidgetKind::Button, "quiet");
            item->setText(w.items[i].c_str());
            item->tag = static_cast<int>(i);
            item->native = [](Ui& ui, Widget& chosen)
            {
                Widget* from = ui.m_layers[ui.m_layerCount - 1].anchor;
                const int which = chosen.tag;
                ui.play(Theme::Change);
                ui.closeTop(-1);
                if (from && from->chosen != which)
                {
                    from->chosen = which;
                    ui.queue(*from, UiEvent::Change);
                }
            };
            now = static_cast<int>(i) == w.chosen ? item : now;
            item->release();
        }
        scroll->release();
        open(root, LayerPlace::Below, &w, false);
        layout();
        focus(now);
    }

    void Ui::toast(const char* text, float seconds)
    {
        if (m_toastCount == kMaxToasts)
        {
            memmove(m_toasts, m_toasts + 1, sizeof(Toast) * (kMaxToasts - 1));
            m_toastCount--;
        }
        Toast& toast = m_toasts[m_toastCount++];
        snprintf(toast.text, sizeof(toast.text), "%s", *text == '@' ? word(text + 1) : text);
        toast.left = seconds;
        toast.shown = 0;
        if (!m_themeReady)
        {
            m_themeReady = true;
            m_theme.load(m_assets, nullptr, 0, "", m_folder);
        }
    }

    // --- Lists ---

    // Rows come into being as they come into view, and are used again for others as they leave.
    bool Ui::fillLists(Widget& w)
    {
        // A script that fills a row may do away with anything.
        w.addRef();
        const bool ok = !w.visible || w.ui == nullptr || fillList(w);
        w.release();
        return ok;
    }

    bool Ui::fillList(Widget& w)
    {
        if (w.kind != WidgetKind::List)
        {
            for (u32 i = 0; i < w.children.count(); i++)
            {
                if (!fillLists(*w.children[i]))
                {
                    return false;
                }
            }
            return true;
        }
        const u32 columns = gridColumns(w, w.w - w.pad(0) - w.pad(2));
        const float each = w.rowHeight + w.space();
        const float from = smaller(w.scroll, w.scrollTo), to = larger(w.scroll, w.scrollTo) + w.h;
        int first = each > 0 ? static_cast<int>(floorf((from - w.pad(1)) / each)) : 0;
        int last = each > 0 ? static_cast<int>(floorf((to - w.pad(1)) / each)) : 0;
        // A long way is scrolled without making all that is passed.
        if (w.scrollTo - w.scroll > w.h * 3 || w.scroll - w.scrollTo > w.h * 3)
        {
            first = static_cast<int>(floorf((w.scrollTo - w.pad(1)) / each));
            last = static_cast<int>(floorf((w.scrollTo + w.h - w.pad(1)) / each));
        }
        first = (first < 0 ? 0 : first) * static_cast<int>(columns);
        last = (last + 1) * static_cast<int>(columns) - 1;
        last = last >= static_cast<int>(w.count) ? static_cast<int>(w.count) - 1 : last;
        Widget* focused = m_focus && m_focus->parent == &w ? m_focus : nullptr;
        bool added = false;
        for (int index = w.h > 0 ? first : 0; index <= last && w.h > 0; index++)
        {
            if (w.row(index))
            {
                continue;
            }
            Widget* row = nullptr;
            for (Widget* child : w.children)
            {
                if (child->index < first || child->index > last)
                {
                    row = child;
                    row->addRef();
                    break;
                }
            }
            if (row == nullptr)
            {
                row = w.add(WidgetKind::Row, "row");
                if (row == nullptr)
                {
                    return true;
                }
                row->align = UiPlace::Center;
            }
            row->removeAll();
            row->index = index;
            row->visible = true;
            row->lookReady = false;
            added = true;
            bool ok = true;
            if (asIScriptFunction* f = w.events[static_cast<size_t>(UiEvent::Row)])
            {
                f->AddRef();
                ok = m_host.callEvent(f, row, nullptr, &index);
                f->Release();
            }
            row->release();
            if (!ok)
            {
                return false;
            }
        }
        for (Widget* child : w.children)
        {
            const bool seen = child->index >= first && child->index <= last;
            if (child->visible != seen)
            {
                child->visible = seen;
                added = true;
            }
        }
        if (added)
        {
            m_dirty = true;
            if (focused)
            {
                // It may have been used for another.
                Widget* row = w.row(w.chosen);
                m_focus = row ? row : focused;
            }
        }
        else if (focused && focused->index != w.chosen)
        {
            if (Widget* row = w.row(w.chosen))
            {
                m_focus = row;
            }
        }
        return true;
    }

    // --- The mouse ---

    Widget* Ui::hit(Widget& w, float px, float py, float mx, float my, bool focusableOnly)
    {
        if (!w.visible)
        {
            return nullptr;
        }
        const float x = px + w.x, y = py + w.y;
        const bool within = mx >= x && my >= y && mx < x + w.w && my < y + w.h;
        if (!within && (w.kind == WidgetKind::Scroll || w.kind == WidgetKind::List || &w == &top()))
        {
            return nullptr;
        }
        if (within && w.focusable())
        {
            return &w;
        }
        for (u32 i = w.children.count(); i-- > 0;)
        {
            if (Widget* found = hit(*w.children[i], x, y - w.scroll, mx, my, focusableOnly))
            {
                return found;
            }
        }
        return within && !focusableOnly ? &w : nullptr;
    }

    void Ui::updateMouse(float dt)
    {
        const InputState& raw = m_input.raw();
        const float wheel = m_input.wheel();
        if (!raw.hasMouse)
        {
            return;
        }
        const Vec2 at = m_canvas.toDesign(raw.mouseX, raw.mouseY);
        const float mx = at.x - m_x, my = at.y - m_y;
        const bool moved = mx != m_mouseX || my != m_mouseY;
        const float movedY = my - m_mouseY;
        m_mouseX = mx;
        m_mouseY = my;
        const bool down = raw.mouseButtons[0];
        Widget* under = hit(top(), 0, 0, mx, my, true);
        if (moved || down != m_mouseDown)
        {
            m_hovered = under;
            // The mouse takes over only when it moves.
            if (moved && under && !down && under != m_focus && m_editing == nullptr)
            {
                focus(under, true);
            }
        }
        if (wheel != 0)
        {
            for (Widget* p = hit(top(), 0, 0, mx, my, false); p; p = p->parent)
            {
                if ((p->kind == WidgetKind::Scroll || p->kind == WidgetKind::List) && p->contentH > p->h)
                {
                    p->scrollTo = smaller(larger(p->scrollTo - wheel * kWheelStep, 0), p->contentH - p->h);
                    break;
                }
            }
        }
        if (down && !m_mouseDown)
        {
            m_held = under;
            m_dragFrom = my;
            m_dragged = nullptr;
            if (under == nullptr && m_layerCount)
            {
                // Outside what is in front: a menu goes away, and a dialog must be answered.
                const Layer& layer = m_layers[m_layerCount - 1];
                const Widget& r = *layer.root;
                const bool outside = mx < r.x || my < r.y || mx >= r.x + r.w || my >= r.y + r.h;
                if (outside && layer.place != LayerPlace::Center)
                {
                    closeTop(-1);
                }
            }
            if (m_editing && m_editing != under && m_keyboardFor == nullptr)
            {
                endEdit();
            }
            if (under && under != m_focus)
            {
                focus(under);
            }
            for (Widget* p = hit(top(), 0, 0, mx, my, false); p; p = p->parent)
            {
                if ((p->kind == WidgetKind::Scroll || p->kind == WidgetKind::List) && p->contentH > p->h)
                {
                    m_dragged = p;
                    m_dragScroll = p->scroll;
                    break;
                }
            }
            if (under && under->kind == WidgetKind::Slider)
            {
                m_dragged = nullptr;
            }
        }
        if (down && m_held && m_held->kind == WidgetKind::Slider)
        {
            float x, y;
            corner(*m_held, x, y);
            const float left = x + m_held->pad(0) + 11, length = m_held->w - m_held->pad(0) - m_held->pad(2) - 22;
            slideTo(*m_held, m_held->low + (m_held->high - m_held->low) * (length > 0 ? smaller(larger((mx - left) / length, 0), 1) : 0));
        }
        else if (down && m_dragged)
        {
            // Far enough to mean it: then it isn't a press on what is under it.
            if (m_held && fabsf(my - m_dragFrom) > 12)
            {
                m_held = nullptr;
            }
            if (m_held == nullptr && moved)
            {
                m_dragged->scrollTo = smaller(larger(m_dragScroll - (my - m_dragFrom), 0), larger(m_dragged->contentH - m_dragged->h, 0));
            }
        }
        if (!down && m_mouseDown)
        {
            if (m_dragged && m_held == nullptr && dt > 0)
            {
                // It goes on a little.
                m_dragged->scrollTo = smaller(larger(m_dragged->scrollTo - movedY * 8, 0), larger(m_dragged->contentH - m_dragged->h, 0));
            }
            m_dragged = nullptr;
            Widget* pressed = m_held;
            m_held = nullptr;
            if (pressed && pressed == under && pressed->kind != WidgetKind::Slider)
            {
                if (pressed->kind == WidgetKind::Choice)
                {
                    float x, y;
                    corner(*pressed, x, y);
                    const float edge = pressed->pad(0) + 34;
                    if (mx < x + edge || mx > x + pressed->w - edge)
                    {
                        step(*pressed, mx < x + edge ? -1 : 1);
                        m_mouseDown = down;
                        return;
                    }
                }
                act(*pressed);
            }
        }
        m_mouseDown = down;
    }

    // --- Each step ---

    bool Ui::update(float dt)
    {
        m_time += dt;
        for (u32 i = 0; i < m_toastCount;)
        {
            Toast& toast = m_toasts[i];
            toast.left -= dt;
            toast.shown = toast.shown < 1 ? toast.shown + dt * 6 : 1;
            if (toast.left <= 0)
            {
                memmove(&m_toasts[i], &m_toasts[i + 1], sizeof(Toast) * (m_toastCount - i - 1));
                m_toastCount--;
            }
            else
            {
                i++;
            }
        }
        if (m_root->children.count() == 0 && m_layerCount == 0)
        {
            return dispatch();
        }
        if (m_dirty)
        {
            layout();
        }
        if (!fillLists(*m_root))
        {
            return false;
        }
        for (u32 i = 0; i < m_layerCount; i++)
        {
            m_layers[i].shown = smaller(m_layers[i].shown + dt * 6, 1);
            if (!fillLists(*m_layers[i].root))
            {
                return false;
            }
        }
        if (m_dirty)
        {
            layout();
        }
        if (m_focus && (!reachable(*m_focus) || !m_focus->focusable()))
        {
            m_focus = nullptr;
        }
        if (m_focus == nullptr)
        {
            Widget* wanted = m_wanted && reachable(*m_wanted) ? firstFocusable(*m_wanted) : nullptr;
            m_wanted = wanted ? nullptr : m_wanted;
            focus(wanted ? wanted : firstFocusable(top()));
        }
        updateMouse(dt);
        if (m_editing)
        {
            updateEditing(dt);
        }
        const bool typingKeys = m_editing && m_keyboardFor == nullptr;
        if (!typingKeys)
        {
            for (const Action way : { Action::Up, Action::Down, Action::Left, Action::Right })
            {
                if (m_input.repeated(way))
                {
                    move(way);
                }
            }
            if (m_input.pressed(Action::Accept) && m_focus)
            {
                m_input.consume(Action::Accept);
                m_input.consume(Action::Fire);
                act(*m_focus);
            }
            else if (m_input.pressed(Action::Cancel) && m_layerCount)
            {
                m_input.consume(Action::Cancel);
                play(Theme::Back);
                closeTop(-1);
            }
        }
        // The shoulder buttons turn to the next tab, or a page of a list.
        u32 pads = 0;
        for (const Gamepad& pad : m_input.raw().pads)
        {
            pads |= pad.connected ? pad.buttons : 0;
        }
        const u32 fresh = pads & ~m_padBefore;
        m_padBefore = pads;
        const u32 leftBit = 1u << static_cast<u32>(Button::LeftShoulder), rightBit = 1u << static_cast<u32>(Button::RightShoulder);
        if ((fresh & (leftBit | rightBit)) && m_keyboardFor == nullptr)
        {
            const int way = fresh & rightBit ? 1 : -1;
            bool done = false;
            for (Widget* p = m_focus; p && !done; p = p->parent)
            {
                if (p->kind == WidgetKind::Tabs)
                {
                    const int to = p->chosen + way;
                    if (to >= 0 && p->children.count() > 1 && static_cast<u32>(to) < p->children[1]->children.count())
                    {
                        Widget& tabs = *p;
                        setTab(tabs, to);
                        focus(firstFocusable(*tabs.children[1]->children[static_cast<u32>(to)]) ? firstFocusable(*tabs.children[1]->children[static_cast<u32>(to)]) : tabs.children[0]->children[static_cast<u32>(to)]);
                        play(Theme::Change);
                    }
                    done = true;
                }
                else if (p->kind == WidgetKind::List)
                {
                    const int columns = static_cast<int>(gridColumns(*p, p->w - p->pad(0) - p->pad(2)));
                    const int page = static_cast<int>(p->h / (p->rowHeight + p->space())) * columns;
                    choose(*p, p->chosen + way * (page > columns ? page : columns), true);
                    done = true;
                }
            }
        }
        if (m_dirty)
        {
            layout();
        }
        animate(*m_root, dt);
        for (u32 i = 0; i < m_layerCount; i++)
        {
            animate(*m_layers[i].root, dt);
        }
        return dispatch();
    }
}
