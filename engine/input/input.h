#pragma once

#include "base/math.h"
#include "platform/platform.h"

#include <string>

namespace nx
{
    // Things a player can do, whatever device they do it with.
    enum class Action : u8
    {
        Up, Down, Left, Right,
        Accept,   // A, Enter, Space
        Cancel,   // B, Backspace
        Fire,     // A, right trigger, Space, left mouse button
        Alternate,  // X, left trigger, Shift
        Menu,     // Start, Escape, or holding Back: always opens the engine's menu
        Count
    };

    // Turns raw devices into actions, with press and release edges and menu repeat. Every action but
    // Menu can be given other keys and buttons.
    class Input
    {
    public:
        Input() { resetBindings(); }

        // Once per update step.
        void update(const InputState& state, float dt);

        bool held(Action a) const { return m_held[index(a)]; }
        bool pressed(Action a) const { return m_held[index(a)] && !m_before[index(a)]; }
        bool released(Action a) const { return !m_held[index(a)] && m_before[index(a)]; }
        // pressed(), and then again at intervals while held: for moving through a list.
        bool repeated(Action a) const { return m_repeat[index(a)]; }

        // Directions from keys, d-pad and left stick. x right, y down; never longer than 1.
        Vec2 move() const { return m_move; }

        const InputState& raw() const { return m_raw; }

        // The last device used, so prompts can match it.
        bool usingPad() const { return m_usingPad; }

        // Forgets what is held, so a button that opened a screen doesn't also act in it.
        // Each action counts again once it has been let go.
        void consume();
        // As above, but only one.
        void consume(Action a) { m_blocked[index(a)] = true; m_held[index(a)] = m_before[index(a)] = m_repeat[index(a)] = false; }

        // Characters from a keyboard in this step. Backspace is 8, Enter 13.
        u32 typedCount() const { return m_typedCount; }
        u32 typed(u32 i) const { return m_typed[i]; }
        // How far the wheel turned in this step.
        float wheel() const { return m_wheel; }
        // While someone is typing, keys aren't actions, but for the way to the menu.
        void setTyping(bool typing) { m_typing = typing; }
        bool typing() const { return m_typing; }
        // For tests and unattended runs.
        void simulateTyping(u32 code) { m_raw.type(code); m_simulatedTyping = true; }

        // --- Bindings ---

        // The next key or button pressed becomes the action's first for that kind of device. Escape
        // or Start calls it off.
        void listen(Action a);
        bool listening() const { return m_listening != Action::Count; }
        void resetBindings();
        std::string describeKeys(Action a) const;
        std::string describeButtons(Action a) const;
        // As text, for saving. Anything unreadable in it is left as it was.
        std::string saveBindings() const;
        void loadBindings(const char* text);

        // For tests and unattended runs: holds an action for the next update.
        void simulate(Action a) { m_simulated[index(a)] = true; }

        static bool parse(const char* name, Action& out);
        static const char* keyName(Key key);
        static const char* buttonName(Button button);

    private:
        static constexpr size_t kCount = static_cast<size_t>(Action::Count);
        static size_t index(Action a) { return static_cast<size_t>(a); }

        struct Binding
        {
            Key keys[2] = { Key::None, Key::None };
            Button buttons[2] = { Button::Count, Button::Count };
        };

        void listenUpdate(const InputState& state);

        Binding m_bindings[kCount];
        Action m_listening = Action::Count;
        InputState m_listenBefore;
        bool m_held[kCount] = {};
        bool m_before[kCount] = {};
        bool m_repeat[kCount] = {};
        bool m_blocked[kCount] = {};
        bool m_simulated[kCount] = {};
        float m_heldFor[kCount] = {};
        float m_backHeldFor = 0;
        Vec2 m_move;
        InputState m_raw;
        bool m_usingPad = false;
        bool m_usedKeys = false;
        bool m_typing = false;
        bool m_simulatedTyping = false;
        u32 m_typed[InputState::kTyped] = {};
        u32 m_typedCount = 0;
        u32 m_typedSeen = 0;
        float m_wheelSeen = 0;
        float m_wheel = 0;
    };
}
