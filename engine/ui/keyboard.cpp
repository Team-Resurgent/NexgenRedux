#include "ui/ui.h"

#include <string.h>

// Typing, with a keyboard or with one on the screen.
namespace nx
{
    namespace
    {
        // A page of keys, in rows; each letter is a key.
        const char* const kPages[][4] = {
            { "1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.-" },
            { "!@#$%^&*()", "[]{}<>=+/\\", ";:\"?_|~`", "\xC2\xA3\xC2\xA7\xC2\xB0\xC2\xAB\xC2\xBB\xC2\xBF\xC2\xA1\xC2\xA9" },
            { "\xC3\xA0\xC3\xA1\xC3\xA2\xC3\xA3\xC3\xA4\xC3\xA5\xC3\xA6\xC3\xA7\xC3\xA8\xC3\xA9", "\xC3\xAA\xC3\xAB\xC3\xAC\xC3\xAD\xC3\xAE\xC3\xAF\xC3\xB1\xC3\xB2\xC3\xB3\xC3\xB4",
              "\xC3\xB5\xC3\xB6\xC3\xB8\xC3\xB9\xC3\xBA\xC3\xBB\xC3\xBC\xC3\xBD\xC3\xBF\xC3\x9F", "" },
        };
        const char* const kNumbers[4] = { "123", "456", "789", "-0." };
        const char* const kPageNames[] = { "#+=", "\xC3\xA0\xC3\xA9\xC3\xB6", "abc" };

        enum Special : int { kShift = -1, kPage = -2, kSpace = -3, kBack = -4, kDone = -5, kLeft = -6, kRight = -7 };

        u32 decode(const char*& p)
        {
            const u8 first = static_cast<u8>(*p++);
            if (first < 0x80 || *p == 0)
            {
                return first;
            }
            return ((first & 0x1F) << 6) | (static_cast<u8>(*p++) & 0x3F);
        }

        void encode(u32 code, char (&out)[4])
        {
            if (code < 0x80)
            {
                out[0] = static_cast<char>(code);
                out[1] = 0;
                return;
            }
            out[0] = static_cast<char>(0xC0 | (code >> 6));
            out[1] = static_cast<char>(0x80 | (code & 0x3F));
            out[2] = 0;
        }

        u32 upper(u32 code)
        {
            const bool letter = (code >= 'a' && code <= 'z') || (code >= 0xE0 && code <= 0xFE && code != 0xF7);
            return letter ? code - 32 : code;
        }

        bool startsLetter(char c) { return (static_cast<u8>(c) & 0xC0) != 0x80; }

        u32 letters(const std::string& text)
        {
            u32 count = 0;
            for (const char c : text)
            {
                count += startsLetter(c);
            }
            return count;
        }

        void caretLeft(Widget& w)
        {
            if (w.caret > 0)
            {
                w.caret--;
                while (w.caret > 0 && !startsLetter(w.text[w.caret]))
                {
                    w.caret--;
                }
            }
        }

        void caretRight(Widget& w)
        {
            if (w.caret < w.text.size())
            {
                w.caret++;
                while (w.caret < w.text.size() && !startsLetter(w.text[w.caret]))
                {
                    w.caret++;
                }
            }
        }
    }

    void Ui::editKey(u32 code)
    {
        Widget* w = m_editing;
        if (w == nullptr)
        {
            return;
        }
        if (code == 13)
        {
            endEdit();
            return;
        }
        if (code == 8)
        {
            if (w->caret > 0)
            {
                const u32 end = w->caret;
                caretLeft(*w);
                w->text.erase(w->caret, end - w->caret);
                queue(*w, UiEvent::Change);
            }
            return;
        }
        const bool allowed = code >= 32 && code != 127 && code <= 255 && !(code >= 128 && code < 160) && (!w->numeric || (code >= '0' && code <= '9') || code == '-' || code == '.');
        if (!allowed || letters(w->text) >= w->limit)
        {
            return;
        }
        char bytes[4];
        encode(code, bytes);
        w->caret = w->caret < w->text.size() ? w->caret : static_cast<u32>(w->text.size());
        w->text.insert(w->caret, bytes);
        w->caret += static_cast<u32>(strlen(bytes));
        queue(*w, UiEvent::Change);
    }

    void Ui::beginEdit(Widget& w)
    {
        if (m_editing == &w)
        {
            return;
        }
        endEdit();
        w.caret = static_cast<u32>(w.text.size());
        play(Theme::Accept);
        // With a gamepad in hand there are no keys to type with.
        if (m_input.usingPad())
        {
            keyboard(w);
            return;
        }
        m_editing = &w;
        m_input.setTyping(true);
    }

    void Ui::endEdit()
    {
        if (m_keyboardFor)
        {
            closeTop(-1);
        }
        else if (m_editing)
        {
            Widget* w = m_editing;
            m_editing = nullptr;
            m_input.setTyping(false);
            queue(*w, UiEvent::Done);
        }
        m_input.consume(Action::Accept);
        m_input.consume(Action::Cancel);
    }

    void keyPressed(Ui& ui, Widget& key)
    {
        const int code = key.tag;
        Widget* field = ui.m_keyboardFor;
        if (field == nullptr)
        {
            return;
        }
        ui.play(code == kDone ? Theme::Accept : Theme::Move);
        switch (code)
        {
        case kShift:
            ui.m_shift = !ui.m_shift;
            keyboardBuild(ui);
            break;
        case kPage:
            ui.m_keyPage = (ui.m_keyPage + 1) % 3;
            keyboardBuild(ui);
            break;
        case kSpace: ui.editKey(' '); break;
        case kBack: ui.editKey(8); break;
        case kDone: ui.endEdit(); return;
        case kLeft: caretLeft(*field); break;
        case kRight: caretRight(*field); break;
        default:
            ui.editKey(static_cast<u32>(code));
            if (ui.m_shift)
            {
                ui.m_shift = false;
                keyboardBuild(ui);
            }
            break;
        }
    }

    void keyboardBuild(Ui& ui)
    {
        Widget& root = *ui.m_layers[ui.m_layerCount - 1].root;
        Widget& field = *ui.m_keyboardFor;
        const std::string focused = ui.m_focus && ui.inside(*ui.m_focus, root) ? ui.m_focus->id : "";
        root.removeAll();

        Widget* top = root.add(WidgetKind::Label, "heading");
        top->id = "typed";
        top->release();
        const auto add = [&](Widget& row, const char* label, int code, float width, float grow)
        {
            Widget* key = row.add(WidgetKind::Button, "key");
            key->setText(label);
            key->tag = code;
            key->width = width;
            key->grow = grow;
            char id[24];
            snprintf(id, sizeof(id), "key%d", code > 0 ? static_cast<int>(upper(static_cast<u32>(code))) : code);
            key->id = id;
            key->native = keyPressed;
            key->release();
        };
        for (u32 r = 0; r < 4; r++)
        {
            const char* p = field.numeric ? kNumbers[r] : kPages[ui.m_keyPage][r];
            if (*p == 0)
            {
                continue;
            }
            Widget* row = root.add(WidgetKind::Row, "plain");
            row->gap = 8;
            row->justify = UiPlace::Center;
            while (*p)
            {
                u32 code = decode(p);
                code = ui.m_shift ? upper(code) : code;
                char label[4];
                encode(code, label);
                add(*row, label, static_cast<int>(code), field.numeric ? 110.0f : 72.0f, 0);
            }
            row->release();
        }
        Widget* row = root.add(WidgetKind::Row, "plain");
        row->gap = 8;
        if (!field.numeric)
        {
            add(*row, "@Shift", kShift, 100, 0);
            add(*row, kPageNames[ui.m_keyPage], kPage, 90, 0);
            add(*row, "@Space", kSpace, 0, 1);
        }
        add(*row, "<", kLeft, 60, 0);
        add(*row, ">", kRight, 60, 0);
        add(*row, "@Delete", kBack, 110, 0);
        add(*row, "@Done", kDone, 110, field.numeric ? 1.0f : 0.0f);
        row->release();
        ui.touch();
        ui.layout();
        Widget* again = nullptr;
        for (Widget* w = ui.m_all; w && !focused.empty(); w = w->nextAll)
        {
            if (w->id == focused && ui.inside(*w, root))
            {
                again = w;
            }
        }
        // In the middle of the letters to begin with.
        for (Widget* w = ui.m_all; w && again == nullptr; w = w->nextAll)
        {
            if (ui.inside(*w, root) && (w->id == (field.numeric ? "key53" : "key71")))
            {
                again = w;
            }
        }
        ui.m_focus = again;
    }

    void Ui::keyboard(Widget& field)
    {
        if (m_keyboardFor || field.kind != WidgetKind::Field)
        {
            return;
        }
        const u32 before = m_layerCount;
        open(make(WidgetKind::Column, "keyboard"), LayerPlace::Bottom, nullptr, true);
        if (m_layerCount == before)
        {
            return;
        }
        m_layers[m_layerCount - 1].focus = &field;
        m_keyboardFor = m_editing = &field;
        m_input.setTyping(true);
        m_shift = false;
        m_keyPage = 0;
        keyboardBuild(*this);
    }

    void Ui::updateEditing(float dt)
    {
        for (u32 i = 0; i < m_input.typedCount() && m_editing; i++)
        {
            editKey(m_input.typed(i));
        }
        if (m_editing == nullptr)
        {
            return;
        }
        const InputState& raw = m_input.raw();
        const auto key = [&](Key k) { return raw.keys[static_cast<size_t>(k)]; };
        const u32 held = (key(Key::Left) ? 1u : 0) | (key(Key::Right) ? 2u : 0) | (key(Key::Up) ? 4u : 0) | (key(Key::Down) ? 8u : 0) | (key(Key::Tab) ? 16u : 0);
        const u32 fresh = held & ~m_keyBefore;
        m_keyHeld = held && held == m_keyBefore ? m_keyHeld + dt : 0;
        m_keyBefore = held;
        bool again = false;
        if (m_keyHeld > 0.45f)
        {
            m_keyHeld -= 0.05f;
            again = true;
        }
        if ((fresh & 1) || (again && (held & 1)))
        {
            caretLeft(*m_editing);
        }
        if ((fresh & 2) || (again && (held & 2)))
        {
            caretRight(*m_editing);
        }
        if (m_keyboardFor == nullptr)
        {
            // Only a gamepad's count while typing.
            if ((fresh & (4 | 8 | 16)) || m_input.pressed(Action::Accept) || m_input.pressed(Action::Cancel))
            {
                const bool on = (fresh & (4 | 8 | 16)) != 0;
                endEdit();
                if (on)
                {
                    move(fresh & 4 ? Action::Up : Action::Down);
                }
            }
            return;
        }
        u32 pads = 0;
        for (const Gamepad& pad : raw.pads)
        {
            pads |= pad.connected ? pad.buttons : 0;
        }
        const u32 pressed = pads & ~m_padBefore;
        const auto button = [&](Button b) { return (pressed & (1u << static_cast<u32>(b))) != 0; };
        if (m_input.repeated(Action::Alternate))
        {
            editKey(8);
            play(Theme::Move);
        }
        if (button(Button::Y))
        {
            editKey(' ');
            play(Theme::Move);
        }
        if (button(Button::LeftShoulder))
        {
            caretLeft(*m_editing);
        }
        if (button(Button::RightShoulder))
        {
            caretRight(*m_editing);
        }
        // What has been typed, above the keys.
        Widget& root = *m_layers[m_layerCount - 1].root;
        for (Widget* child : root.children)
        {
            if (child->id == "typed")
            {
                const Widget& f = *m_editing;
                std::string text = f.text;
                u32 caret = f.caret < text.size() ? f.caret : static_cast<u32>(text.size());
                if (f.secret)
                {
                    const u32 before = letters(text.substr(0, caret));
                    text.assign(letters(text), '*');
                    caret = before;
                }
                text.insert(caret, "|");
                // A space first, so that it is never taken for a word to look up.
                child->setText((" " + text).c_str());
            }
        }
    }
}
