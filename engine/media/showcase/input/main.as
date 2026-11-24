// Everything that is pressed, on every device.

class InputScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    Font@ small = assets::font("global:freesans.sfn", 18);
    array<string> actions = { "up", "down", "left", "right", "accept", "cancel", "fire", "alternate" };
    array<string> buttons = { "Up", "Down", "Left", "Right", "Start", "Back", "LS", "RS", "A", "B", "X", "Y", "LB", "RB" };
    array<float> glow(8);
    array<Vec2> trail;

    void update(float dt)
    {
        for (uint i = 0; i < actions.length(); i++)
        {
            glow[i] = clamp(glow[i] - dt * 2, 0, 1);
            if (input::pressed(actions[i]))
                glow[i] = 1;
        }
        if (input::hasMouse())
        {
            trail.insertLast(input::mouse());
            if (trail.length() > 24)
                trail.removeAt(0);
        }
    }

    void light(Canvas@ c, float x, float y, float w, const string &in label, bool on, float after)
    {
        Color off(0.13f, 0.15f, 0.22f);
        Color color = on ? Green : Color(lerp(off.r, 0.3f, after), lerp(off.g, 0.5f, after), lerp(off.b, 0.4f, after));
        c.roundRect(x, y, w, 48, 10, color);
        c.text(small, label, x + w / 2, y + 13, on ? Black : White, AlignCenter);
    }

    void stick(Canvas@ c, float x, float y, const Vec2 &in at)
    {
        c.ring(x, y, 42, 3, Gray);
        c.circle(x + at.x * 32, y - at.y * 32, 14, at.dot(at) > 0 ? Green : Color(0.3f, 0.34f, 0.44f));
    }

    void pad(Canvas@ c, uint index, float x, float y)
    {
        bool on = input::padConnected(index);
        c.roundRect(x, y, 590, 200, 14, Color(0.09f, 0.11f, 0.17f));
        c.text(small, "Gamepad " + formatInt(index + 1) + (on ? "" : "  (not connected)"), x + 16, y + 10, on ? White : Gray);
        if (!on)
            return;
        stick(c, x + 66, y + 96, input::padLeft(index));
        stick(c, x + 176, y + 96, input::padRight(index));
        Vec2 triggers = input::padTriggers(index);
        c.rect(x + 30, y + 160, 180, 10, Color(0.15f, 0.18f, 0.26f));
        c.rect(x + 30, y + 160, 180 * triggers.x, 10, Orange);
        c.rect(x + 30, y + 178, 180, 10, Color(0.15f, 0.18f, 0.26f));
        c.rect(x + 30, y + 178, 180 * triggers.y, 10, Orange);
        for (uint b = 0; b < buttons.length(); b++)
        {
            bool down = input::padButton(index, b);
            float bx = x + 240 + float(b % 5) * 68;
            float by = y + 44 + float(b / 5) * 50;
            c.roundRect(bx, by, 62, 42, 8, down ? Green : Color(0.15f, 0.18f, 0.26f));
            c.text(small, buttons[b], bx + 31, by + 10, down ? Black : White, AlignCenter);
        }
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.05f, 0.06f, 0.1f));
        c.text(font, "Actions, from any device", 40, 20, Color(0.7f, 0.76f, 0.86f));
        for (uint i = 0; i < actions.length(); i++)
            light(c, 40 + float(i) * 122, 60, 112, actions[i], input::held(actions[i]), glow[i]);

        c.roundRect(1040, 20, 200, 110, 12, Color(0.09f, 0.11f, 0.17f));
        Vec2 move = input::move();
        c.ring(1140, 75, 46, 2, Gray);
        c.line(1140, 75, 1140 + move.x * 44, 75 + move.y * 44, 4, Blue);
        c.circle(1140 + move.x * 44, 75 + move.y * 44, 8, Blue);

        pad(c, 0, 40, 150);
        pad(c, 1, 650, 150);
        pad(c, 2, 40, 366);
        pad(c, 3, 650, 366);

        c.text(font, "Mouse", 40, 590, Color(0.7f, 0.76f, 0.86f));
        for (uint i = 0; i < 3; i++)
            c.roundRect(130 + float(i) * 56, 586, 48, 40, 8, input::mouseDown(i) ? Green : Color(0.15f, 0.18f, 0.26f));
        for (uint i = 0; i < trail.length(); i++)
            c.circle(trail[i].x, trail[i].y, 2 + float(i) * 0.4f, fade(Yellow, float(i) / 24));
        c.text(small, "Last used: " + (input::usingPad() ? "gamepad" : "keyboard or mouse") + ".   Start, Escape, or holding Back opens the menu.", 40, 670, Gray);
    }
}
