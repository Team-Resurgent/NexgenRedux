#include "ui/ui.h"
#include "base/log.h"

#include <angelscript.h>

#include <stdio.h>
#include <string.h>

// Pages, themes and words from files. A name that means nothing is an error with its line.
namespace nx
{
    using Json = JsonTree::Value;

    namespace
    {
        const struct { const char* name; WidgetKind kind; } kKinds[] = {
            { "column", WidgetKind::Column }, { "row", WidgetKind::Row }, { "grid", WidgetKind::Grid }, { "stack", WidgetKind::Stack }, { "scroll", WidgetKind::Scroll },
            { "label", WidgetKind::Label }, { "paragraph", WidgetKind::Label }, { "image", WidgetKind::Image }, { "button", WidgetKind::Button }, { "toggle", WidgetKind::Toggle },
            { "slider", WidgetKind::Slider }, { "progress", WidgetKind::Progress }, { "choice", WidgetKind::Choice }, { "field", WidgetKind::Field }, { "list", WidgetKind::List },
            { "tabs", WidgetKind::Tabs }, { "space", WidgetKind::Space }, { "line", WidgetKind::Line }, { "custom", WidgetKind::Custom },
        };

        bool readPlace(Json v, UiPlace& out)
        {
            static const char* const names[] = { "start", "center", "end", "stretch", "spread" };
            for (u32 i = 0; i < 5; i++)
            {
                if (strcmp(v.text(), names[i]) == 0)
                {
                    out = static_cast<UiPlace>(i);
                    return true;
                }
            }
            return false;
        }
    }

    bool Ui::fail(const char* file, u32 line, const char* what, const char* name)
    {
        snprintf(m_error, sizeof(m_error), "%s:%u: %s %s", file, static_cast<unsigned>(line), name, what);
        return false;
    }

    bool Ui::watch(const char* path)
    {
        for (u32 i = 0; i < m_watchedCount; i++)
        {
            if (strcmp(m_watched[i].path, path) == 0)
            {
                m_watched[i].time = m_platform.fileTime(path);
                return true;
            }
        }
        if (m_watchedCount < sizeof(m_watched) / sizeof(m_watched[0]))
        {
            Watched& w = m_watched[m_watchedCount++];
            snprintf(w.path, sizeof(w.path), "%s", path);
            w.time = m_platform.fileTime(path);
        }
        return true;
    }

    bool Ui::changed()
    {
        for (u32 i = 0; i < m_watchedCount; i++)
        {
            const u64 time = m_platform.fileTime(m_watched[i].path);
            if (time && time != m_watched[i].time)
            {
                return true;
            }
        }
        return false;
    }

    bool Ui::readFile(const char* name, FileData& out, char (&path)[192])
    {
        m_error[0] = 0;
        if (!Theme::path(m_folder, name, path, sizeof(path)) || strncmp(path, "http", 4) == 0)
        {
            snprintf(m_error, sizeof(m_error), "file names start with asset: or global: (%s)", name);
            return false;
        }
        watch(path);
        if (!m_platform.readFile(path, out, MemTag::UI))
        {
            snprintf(m_error, sizeof(m_error), "cannot read %s", path);
            return false;
        }
        return true;
    }

    bool Ui::loadTheme(const char* name)
    {
        FileData file;
        char path[192];
        bool ok = true;
        m_themeReady = true;
        if (*name == 0)
        {
            ok = m_theme.load(m_assets, nullptr, 0, "", m_folder);
        }
        else if (!readFile(name, file, path))
        {
            return false;
        }
        else
        {
            ok = m_theme.load(m_assets, reinterpret_cast<const char*>(file.data), file.size, path, m_folder);
            if (!ok)
            {
                snprintf(m_error, sizeof(m_error), "%s", m_theme.error());
            }
        }
        for (Widget* w = m_all; w; w = w->nextAll)
        {
            restyle(*w);
        }
        return ok;
    }

    bool Ui::words(const char* name)
    {
        FileData file;
        char path[192];
        JsonTree tree;
        if (!readFile(name, file, path))
        {
            return false;
        }
        if (!tree.parse(reinterpret_cast<const char*>(file.data), file.size))
        {
            return fail(path, tree.errorLine(), tree.error(), "");
        }
        m_words.clear();
        for (Json v = tree.root().first(); v.exists(); v = v.next())
        {
            if (!v.isString())
            {
                return fail(path, v.line(), "must be text", v.key());
            }
            m_words.push_back({ v.key(), v.text() });
        }
        m_dirty = true;
        return true;
    }

    bool Ui::load(const char* name, Widget& into, asIScriptObject* screen)
    {
        FileData file;
        char path[192];
        JsonTree tree;
        if (!readFile(name, file, path))
        {
            return false;
        }
        if (!tree.parse(reinterpret_cast<const char*>(file.data), file.size))
        {
            return fail(path, tree.errorLine(), tree.error(), "");
        }
        const Json root = tree.root();
        if (!root.isArray())
        {
            return build(root, into, path, screen) != nullptr;
        }
        for (Json v = root.first(); v.exists(); v = v.next())
        {
            if (build(v, into, path, screen) == nullptr)
            {
                return false;
            }
        }
        return true;
    }

    Widget* Ui::build(Json v, Widget& into, const char* file, asIScriptObject* screen)
    {
        u32 which = 0;
        constexpr u32 kCount = sizeof(kKinds) / sizeof(kKinds[0]);
        while (which < kCount && !v[kKinds[which].name].exists())
        {
            which++;
        }
        if (!v.isObject() || which == kCount)
        {
            fail(file, v.line(), "what kind of widget it is must be said: \"column\", \"button\" and so on", "");
            return nullptr;
        }
        const WidgetKind kind = kKinds[which].kind;
        const Json main = v[kKinds[which].name];
        Widget* w = kind == WidgetKind::Tabs ? into.addTabs() : into.add(kind, v["style"].text());
        if (w == nullptr)
        {
            return nullptr;
        }
        w->release();   // its parent has it
        w->wrap = strcmp(kKinds[which].name, "paragraph") == 0;
        w->gap = kind <= WidgetKind::Scroll && !v["style"].exists() ? 12.0f : -1.0f;
        if (kind == WidgetKind::Tabs)
        {
            for (Json t = main.first(); t.exists(); t = t.next())
            {
                Widget* page = w->addTab(t["title"].text());
                page->release();
                for (Json c = t["page"].first(); c.exists(); c = c.next())
                {
                    if (build(c, *page, file, screen) == nullptr)
                    {
                        return nullptr;
                    }
                }
            }
        }
        else if (kind <= WidgetKind::Scroll || (kind == WidgetKind::Button && main.isArray()))
        {
            for (Json c = main.first(); c.exists(); c = c.next())
            {
                if (build(c, *w, file, screen) == nullptr)
                {
                    return nullptr;
                }
            }
        }
        else if (kind == WidgetKind::Image)
        {
            char path[192];
            if (!Theme::path(m_folder, main.text(), path, sizeof(path)))
            {
                fail(file, main.line(), "isn't the name of an image", main.text());
                return nullptr;
            }
            w->setImage(m_assets.texture(path), m_assets);
        }
        else if (kind == WidgetKind::Choice)
        {
            for (Json c = main.first(); c.exists(); c = c.next())
            {
                w->items.push_back(c.text());
            }
        }
        else if (kind == WidgetKind::Space)
        {
            w->width = w->height = main.number();
            w->grow = main.isNumber() ? 0.0f : 1.0f;
        }
        else if (kind == WidgetKind::Slider || kind == WidgetKind::Progress)
        {
            w->value = main.number();
        }
        else if (kind == WidgetKind::List)
        {
            w->rowHeight = main.number(56);
        }
        else if (main.isString())
        {
            w->setText(main.text());
        }

        for (Json p = v.first(); p.exists(); p = p.next())
        {
            const char* key = p.key();
            const struct { const char* name; float* number; } numbers[] = {
                { "width", &w->width }, { "height", &w->height }, { "minWidth", &w->minWidth }, { "maxWidth", &w->maxWidth }, { "grow", &w->grow }, { "gap", &w->gap },
                { "columnWidth", &w->columnWidth }, { "value", &w->value }, { "low", &w->low }, { "high", &w->high }, { "step", &w->step }, { "rowHeight", &w->rowHeight },
            };
            const struct { const char* name; bool* flag; } flags[] = { { "visible", &w->visible }, { "enabled", &w->enabled }, { "checked", &w->checked }, { "wrap", &w->wrap }, { "secret", &w->secret }, { "numeric", &w->numeric } };
            const struct { const char* name; UiPlace* place; } places[] = { { "align", &w->align }, { "justify", &w->justify }, { "self", &w->self } };
            const struct { const char* name; UiEvent event; } events[] = { { "click", UiEvent::Click }, { "change", UiEvent::Change }, { "focus", UiEvent::Focus }, { "done", UiEvent::Done }, { "draw", UiEvent::Draw }, { "makeRow", UiEvent::Row } };
            bool known = strcmp(key, kKinds[which].name) == 0 || strcmp(key, "style") == 0;
            for (const auto& n : numbers)
            {
                if (strcmp(key, n.name) == 0)
                {
                    *n.number = p.number();
                    known = true;
                }
            }
            for (const auto& f : flags)
            {
                if (strcmp(key, f.name) == 0)
                {
                    *f.flag = p.flag();
                    known = true;
                }
            }
            for (const auto& x : places)
            {
                if (strcmp(key, x.name) == 0)
                {
                    known = true;
                    if (!readPlace(p, *x.place))
                    {
                        fail(file, p.line(), "isn't start, center, end, stretch or spread", p.text());
                        return nullptr;
                    }
                }
            }
            for (const auto& e : events)
            {
                if (strcmp(key, e.name) == 0)
                {
                    known = true;
                    char declaration[96];
                    snprintf(declaration, sizeof(declaration), e.event == UiEvent::Draw ? "void %s(Canvas@, Widget@)" : e.event == UiEvent::Row ? "void %s(Widget@, int)" : "void %s(Widget@)", p.text());
                    asIScriptFunction* method = screen ? screen->GetObjectType()->GetMethodByDecl(declaration) : nullptr;
                    asIScriptFunction* bound = method ? m_host.engine()->CreateDelegate(method, screen) : nullptr;
                    if (bound == nullptr)
                    {
                        fail(file, p.line(), "isn't a method of the screen that takes what it should", p.text());
                        return nullptr;
                    }
                    w->setEvent(e.event, bound);
                }
            }
            if (strcmp(key, "id") == 0) w->id = p.text();
            else if (strcmp(key, "text") == 0) w->setText(p.text());
            else if (strcmp(key, "hint") == 0) w->hint = p.text();
            else if (strcmp(key, "icon") == 0)
            {
                char where[192];
                if (!Theme::path(m_folder, p.text(), where, sizeof(where)))
                {
                    fail(file, p.line(), "isn't the name of an image", p.text());
                    return nullptr;
                }
                w->setImage(m_assets.texture(where), m_assets);
            }
            else if (strcmp(key, "chosen") == 0)
            {
                w->chosen = static_cast<int>(p.number());
                if (kind == WidgetKind::Tabs)
                {
                    setTab(*w, w->chosen);
                }
            }
            else if (strcmp(key, "count") == 0) w->count = static_cast<u32>(p.number());
            else if (strcmp(key, "columns") == 0) w->columns = static_cast<u32>(p.number());
            else if (strcmp(key, "limit") == 0) w->limit = static_cast<u32>(p.number());
            else if (strcmp(key, "tag") == 0) w->tag = static_cast<int>(p.number());
            else if (strcmp(key, "padding") == 0)
            {
                for (u32 i = 0; i < 4; i++)
                {
                    w->padding[i] = p.isArray() ? p[p.count() == 4 ? i : i % 2].number() : p.number();
                }
            }
            else if (!known)
            {
                fail(file, p.line(), "isn't part of a widget", key);
                return nullptr;
            }
        }
        touch();
        return w;
    }
}
