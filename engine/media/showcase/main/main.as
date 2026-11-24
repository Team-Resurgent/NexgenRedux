// The home screen: a tile for every other screen that was found.

class MainScreen : Screen
{
    Font@ title = assets::font("global:freesans.sfn", 60);
    Font@ name = assets::font("global:freesans.sfn", 30);
    Font@ small = assets::font("global:freesans.sfn", 18);

    Sound@ moveSound = assets::sound("global:sound/move.wav");
    Sound@ openSound = assets::sound("global:sound/accept.wav");

    array<uint> tiles;      // indices of the screens on offer
    uint chosen = 0;
    float scroll = 0;
    float shown = 0;        // 0 to 1 as the screen opens
    Vec2 lastMouse;
    bool mouseWasDown = false;

    // Four columns on a wide screen, three on a square one.
    float screenW = 1280;
    uint columns = 4;
    float left = 60, top = 160, tileW = 278, tileH = 118, gap = 16;

    void layout(float width)
    {
        screenW = width;
        columns = width < 1100 ? 3 : 4;
        left = width < 1100 ? 36 : 60;
        tileW = (width - left * 2 - gap * float(columns - 1)) / float(columns);
    }

    void onEnter()
    {
        for (uint i = 0; i < screens::count(); i++)
        {
            if (screens::id(i) != "main")
                tiles.insertLast(i);
        }
    }

    void update(float dt)
    {
        shown = clamp(shown + dt * 2.5f, 0, 1);
        uint count = tiles.length();
        uint before = chosen;
        if (count == 0)
            return;

        if (input::repeated("right") && chosen + 1 < count) chosen++;
        if (input::repeated("left") && chosen > 0) chosen--;
        if (input::repeated("down")) chosen = chosen + columns < count ? chosen + columns : count - 1;
        if (input::repeated("up") && chosen >= columns) chosen -= columns;

        // The mouse takes over only when it moves.
        Vec2 mouse = input::mouse();
        bool down = input::mouseDown(0);
        int under = tileAt(mouse);
        if (input::hasMouse() && mouse != lastMouse && under >= 0)
            chosen = uint(under);
        lastMouse = mouse;
        bool clicked = down && !mouseWasDown && under >= 0;
        mouseWasDown = down;

        if (chosen != before)
            moveSound.play(0.6f);
        if (input::pressed("accept") || clicked)
        {
            openSound.play(0.6f);
            screens::push(screens::id(tiles[chosen]));
        }

        // Keep the chosen row in view.
        float row = float(chosen / columns) * (tileH + gap);
        float room = 720 - top - 64;
        float wanted = clamp(scroll, row + tileH - room, row);
        wanted = max(wanted, 0.0f);
        scroll = lerp(scroll, wanted, 0.2f);
    }

    float max(float a, float b) { return a > b ? a : b; }

    int tileAt(const Vec2 &in p)
    {
        for (uint i = 0; i < tiles.length(); i++)
        {
            float x = left + float(i % columns) * (tileW + gap);
            float y = top + float(i / columns) * (tileH + gap) - scroll;
            if (p.x >= x && p.x < x + tileW && p.y >= y && p.y < y + tileH)
                return int(i);
        }
        return -1;
    }

    Color colorOf(const string &in category)
    {
        if (category == "Drawing") return Blue;
        if (category == "Sprites") return Green;
        if (category == "Effects") return Purple;
        if (category == "Interface") return Orange;
        if (category == "Games") return Red;
        if (category == "Sound") return Yellow;
        return Gray;
    }

    void draw(Canvas@ c)
    {
        c.designHeight(720);
        layout(c.width);
        c.gradient(0, 0, screenW, 720, Color(0.07f, 0.09f, 0.16f), Color(0.03f, 0.03f, 0.06f));
        for (uint i = 0; i < 5; i++)
        {
            float t = app::time() * 0.15f + float(i) * 1.3f;
            c.circle(screenW * 0.5f + sin(t) * 620, 360 + cos(t * 1.3f) * 330, 160, fade(hsv(0.55f + float(i) * 0.05f, 0.7f, 0.9f), 0.05f));
        }

        c.text(title, "Nexgen", left, 46);
        c.text(small, "A showcase of the engine. Every tile is one script.", left + 4, 116, Color(0.6f, 0.68f, 0.8f));
        c.text(small, app::platform() + "  |  " + app::gpu() + "  |  " + formatInt(int(app::fps() + 0.5f)) + " fps", screenW - left, 66, Color(0.6f, 0.68f, 0.8f), AlignRight);

        c.push();
        c.clip(0, top - 14, screenW, 720 - top - 34);
        for (uint i = 0; i < tiles.length(); i++)
        {
            float x = left + float(i % columns) * (tileW + gap);
            float y = top + float(i / columns) * (tileH + gap) - scroll;
            float appear = smooth(shown * 2 - float(i) * 0.06f);
            drawTile(c, tiles[i], x, y + (1 - appear) * 40, i == chosen, appear);
        }
        c.pop();

        c.rect(0, 676, screenW, 44, Color(0.02f, 0.02f, 0.05f, 0.8f));
        string help = input::usingPad() ? "A  open     Start  menu     Hold Back  menu from anywhere" : "Enter  open     Arrows  move     Escape  menu from anywhere";
        c.text(small, help, screenW * 0.5f, 686, Color(0.65f, 0.72f, 0.85f), AlignCenter);
    }

    void drawTile(Canvas@ c, uint screen, float x, float y, bool chosen, float appear)
    {
        Color accent = colorOf(screens::category(screen));
        c.push();
        c.setAlpha(appear);
        c.translate(x + tileW / 2, y + tileH / 2);
        if (chosen)
        {
            float pulse = 1.03f + 0.01f * sin(app::time() * 5);
            c.scale(pulse, pulse);
            c.roundRect(-tileW / 2 - 4, -tileH / 2 - 4, tileW + 8, tileH + 8, 18, accent);
        }
        c.roundRect(-tileW / 2, -tileH / 2, tileW, tileH, 14, chosen ? Color(0.17f, 0.21f, 0.32f) : Color(0.11f, 0.13f, 0.2f));
        c.roundRect(-tileW / 2 + 14, -tileH / 2 + 16, 6, 28, 3, accent);
        c.text(name, screens::title(screen), -tileW / 2 + 32, -tileH / 2 + 12);
        c.paragraph(small, screens::description(screen), -tileW / 2 + 16, -tileH / 2 + 56, tileW - 28, Color(0.66f, 0.72f, 0.82f));
        c.pop();
    }
}
