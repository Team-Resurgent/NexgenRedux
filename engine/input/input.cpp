#include "input/input.h"

#include <math.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr float kRepeatDelay = 0.40f;
        constexpr float kRepeatEvery = 0.12f;
        constexpr float kBackHold = 0.6f;
        constexpr float kStickAsButton = 0.5f;
        constexpr float kTriggerAsButton = 0.3f;

        const char* const kNames[] = { "up", "down", "left", "right", "accept", "cancel", "fire", "alternate", "menu" };
        static_assert(sizeof(kNames) / sizeof(kNames[0]) == static_cast<size_t>(Action::Count));

        const char* const kKeyNames[] = {
            "None", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "Up", "Down", "Left", "Right", "Space", "Enter", "Escape", "Tab", "Backspace", "Shift", "Control"
        };
        static_assert(sizeof(kKeyNames) / sizeof(kKeyNames[0]) == static_cast<size_t>(Key::Count));

        const char* const kButtonNames[] = { "D-pad up", "D-pad down", "D-pad left", "D-pad right", "Start", "Back", "Left stick", "Right stick", "A", "B", "X", "Y", "Left shoulder", "Right shoulder", "None" };
        static_assert(sizeof(kButtonNames) / sizeof(kButtonNames[0]) == static_cast<size_t>(Button::Count) + 1);
    }

    const char* Input::keyName(Key key)
    {
        return kKeyNames[static_cast<size_t>(key)];
    }

    const char* Input::buttonName(Button button)
    {
        return kButtonNames[static_cast<size_t>(button)];
    }

    bool Input::parse(const char* name, Action& out)
    {
        for (size_t i = 0; i < kCount; i++)
        {
            if (strcmp(name, kNames[i]) == 0)
            {
                out = static_cast<Action>(i);
                return true;
            }
        }
        return false;
    }

    // --- Bindings ---

    void Input::resetBindings()
    {
        const auto set = [&](Action a, Key k1, Key k2, Button b1, Button b2)
        {
            Binding& b = m_bindings[index(a)];
            b.keys[0] = k1;
            b.keys[1] = k2;
            b.buttons[0] = b1;
            b.buttons[1] = b2;
        };
        set(Action::Up, Key::Up, Key::W, Button::Up, Button::Count);
        set(Action::Down, Key::Down, Key::S, Button::Down, Button::Count);
        set(Action::Left, Key::Left, Key::A, Button::Left, Button::Count);
        set(Action::Right, Key::Right, Key::D, Button::Right, Button::Count);
        set(Action::Accept, Key::Enter, Key::Space, Button::A, Button::Count);
        set(Action::Cancel, Key::Backspace, Key::None, Button::B, Button::Count);
        set(Action::Fire, Key::Space, Key::None, Button::A, Button::Count);
        set(Action::Alternate, Key::Shift, Key::None, Button::X, Button::Count);
        // So that the way out can't be lost, this one is never changed.
        set(Action::Menu, Key::Escape, Key::None, Button::Start, Button::Count);
    }

    void Input::listen(Action a)
    {
        if (a < Action::Menu)
        {
            m_listening = a;
            m_listenBefore = m_raw;
        }
    }

    void Input::listenUpdate(const InputState& state)
    {
        Binding& b = m_bindings[index(m_listening)];
        bool done = false;
        for (size_t k = 1; k < static_cast<size_t>(Key::Count) && !done; k++)
        {
            if (state.keys[k] && !m_listenBefore.keys[k])
            {
                done = true;
                if (static_cast<Key>(k) != Key::Escape)
                {
                    b.keys[0] = static_cast<Key>(k);
                    b.keys[1] = b.keys[1] == b.keys[0] ? Key::None : b.keys[1];
                }
            }
        }
        for (u32 p = 0; p < kMaxPads && !done; p++)
        {
            const u32 fresh = state.pads[p].buttons & ~m_listenBefore.pads[p].buttons;
            for (u32 i = 0; i < static_cast<u32>(Button::Count) && !done; i++)
            {
                if (fresh & (1u << i))
                {
                    done = true;
                    if (static_cast<Button>(i) != Button::Start && static_cast<Button>(i) != Button::Back)
                    {
                        b.buttons[0] = static_cast<Button>(i);
                    }
                }
            }
        }
        m_listenBefore = state;
        if (done)
        {
            m_listening = Action::Count;
        }
    }

    std::string Input::describeKeys(Action a) const
    {
        std::string text;
        for (const Key k : m_bindings[index(a)].keys)
        {
            if (k != Key::None)
            {
                text += text.empty() ? "" : ", ";
                text += keyName(k);
            }
        }
        return text.empty() ? "None" : text;
    }

    std::string Input::describeButtons(Action a) const
    {
        std::string text;
        for (const Button b : m_bindings[index(a)].buttons)
        {
            if (b != Button::Count)
            {
                text += text.empty() ? "" : ", ";
                text += buttonName(b);
            }
        }
        return text.empty() ? "None" : text;
    }

    std::string Input::saveBindings() const
    {
        std::string text;
        for (size_t a = 0; a < index(Action::Menu); a++)
        {
            const Binding& b = m_bindings[a];
            text += std::string(kNames[a]) + "=" + keyName(b.keys[0]) + "," + keyName(b.keys[1]) + "," + buttonName(b.buttons[0]) + "," + buttonName(b.buttons[1]) + ";";
        }
        return text;
    }

    void Input::loadBindings(const char* text)
    {
        const std::string all = text;
        size_t at = 0;
        while (at < all.size())
        {
            size_t end = all.find(';', at);
            end = end == std::string::npos ? all.size() : end;
            const std::string item = all.substr(at, end - at);
            at = end + 1;
            const size_t equals = item.find('=');
            Action a = Action::Menu;
            if (equals == std::string::npos || !parse(item.substr(0, equals).c_str(), a) || a == Action::Menu)
            {
                continue;
            }
            std::string parts[4];
            size_t from = equals + 1;
            u32 count = 0;
            while (count < 4 && from <= item.size())
            {
                size_t comma = item.find(',', from);
                comma = comma == std::string::npos ? item.size() : comma;
                parts[count++] = item.substr(from, comma - from);
                from = comma + 1;
            }
            if (count != 4)
            {
                continue;
            }
            Binding& b = m_bindings[index(a)];
            for (u32 i = 0; i < 2; i++)
            {
                for (size_t k = 0; k < static_cast<size_t>(Key::Count); k++)
                {
                    b.keys[i] = parts[i] == kKeyNames[k] ? static_cast<Key>(k) : b.keys[i];
                }
                for (size_t k = 0; k <= static_cast<size_t>(Button::Count); k++)
                {
                    b.buttons[i] = parts[2 + i] == kButtonNames[k] ? static_cast<Button>(k) : b.buttons[i];
                }
            }
        }
    }

    // --- Actions ---

    void Input::update(const InputState& state, float dt)
    {
        m_raw = state;
        if (listening())
        {
            listenUpdate(state);
            consume();
            // Whatever was pressed to choose the binding isn't also an action.
            for (size_t i = 0; i < kCount; i++)
            {
                m_blocked[i] = true;
            }
        }

        bool now[kCount] = {};
        const auto set = [&](Action a, bool down) { now[index(a)] = now[index(a)] || down; };
        bool keys[kCount] = {};
        for (size_t a = 0; a < kCount; a++)
        {
            for (const Key k : m_bindings[a].keys)
            {
                keys[a] = keys[a] || (k != Key::None && state.keys[static_cast<size_t>(k)]);
            }
            now[a] = keys[a];
        }
        set(Action::Fire, state.mouseButtons[0]);

        Vec2 move(static_cast<float>(keys[index(Action::Right)]) - static_cast<float>(keys[index(Action::Left)]),
            static_cast<float>(keys[index(Action::Down)]) - static_cast<float>(keys[index(Action::Up)]));

        bool anyKey = false;
        for (bool k : state.keys)
        {
            anyKey = anyKey || k;
        }
        bool anyPad = false;
        bool connected = false;
        bool back = false;
        for (const Gamepad& pad : state.pads)
        {
            if (!pad.connected)
            {
                continue;
            }
            connected = true;
            bool buttons[kCount] = {};
            for (size_t a = 0; a < kCount; a++)
            {
                for (const Button b : m_bindings[a].buttons)
                {
                    buttons[a] = buttons[a] || (b != Button::Count && pad.down(b));
                }
                now[a] = now[a] || buttons[a];
            }
            set(Action::Up, pad.leftY > kStickAsButton);
            set(Action::Down, pad.leftY < -kStickAsButton);
            set(Action::Left, pad.leftX < -kStickAsButton);
            set(Action::Right, pad.leftX > kStickAsButton);
            set(Action::Fire, pad.rightTrigger > kTriggerAsButton);
            set(Action::Alternate, pad.leftTrigger > kTriggerAsButton);
            back = back || pad.down(Button::Back);
            move.x += pad.leftX + static_cast<float>(buttons[index(Action::Right)]) - static_cast<float>(buttons[index(Action::Left)]);
            move.y += -pad.leftY + static_cast<float>(buttons[index(Action::Down)]) - static_cast<float>(buttons[index(Action::Up)]);
            anyPad = anyPad || pad.buttons != 0 || pad.leftX != 0 || pad.leftY != 0;
        }

        // A short press of Back belongs to the screen; holding it always reaches the menu.
        m_backHeldFor = back ? m_backHeldFor + dt : 0;
        set(Action::Menu, m_backHeldFor >= kBackHold);

        for (size_t i = 0; i < kCount; i++)
        {
            now[i] = now[i] || m_simulated[i];
            m_simulated[i] = false;

            m_blocked[i] = m_blocked[i] && (now[i] || listening());
            m_before[i] = m_held[i];
            m_held[i] = now[i] && !m_blocked[i];

            m_repeat[i] = m_held[i] && !m_before[i];
            if (m_held[i])
            {
                const float until = m_heldFor[i];
                m_heldFor[i] += dt;
                if (m_heldFor[i] >= kRepeatDelay && floorf((m_heldFor[i] - kRepeatDelay) / kRepeatEvery) != floorf((until - kRepeatDelay) / kRepeatEvery))
                {
                    m_repeat[i] = true;
                }
            }
            else
            {
                m_heldFor[i] = 0;
            }
        }

        const float length = sqrtf(move.dot(move));
        m_move = length > 1 ? move / length : move;
        // Until something is pressed, a gamepad is more likely than not.
        m_usedKeys = m_usedKeys || anyKey || state.mouseButtons[0];
        m_usingPad = anyPad || (!m_usedKeys && connected) || (m_usingPad && !anyKey && !state.mouseButtons[0]);
    }

    void Input::consume()
    {
        for (size_t i = 0; i < kCount; i++)
        {
            m_blocked[i] = m_blocked[i] || m_held[i];
            m_held[i] = m_before[i] = m_repeat[i] = false;
            m_heldFor[i] = 0;
        }
        m_backHeldFor = 0;
    }
}
