// Clipping: a list that scrolls in a window, windows inside windows, and a spotlight.

class ClipScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    int chosen = 0;
    float scroll = 0;
    float t = 0;
    int rows = 500;
    float rowH = 44;
    float viewH = 560;

    void update(float dt)
    {
        t += dt;
        if (input::repeated("down") && chosen + 1 < rows) chosen++;
        if (input::repeated("up") && chosen > 0) chosen--;
        float wanted = clamp(scroll, float(chosen + 1) * rowH - viewH, float(chosen) * rowH);
        scroll = lerp(scroll, wanted < 0 ? 0 : wanted, 0.25f);
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.06f, 0.07f, 0.1f));

        // Only the rows in view are drawn, so five hundred cost the same as twelve.
        c.text(font, "500 rows: up and down", 60, 24, Color(0.7f, 0.76f, 0.86f));
        c.roundRect(50, 70, 440, viewH + 20, 12, Color(0.1f, 0.12f, 0.18f));
        c.push();
        c.clip(60, 80, 420, viewH);
        int first = int(scroll / rowH);
        for (int i = first; i < rows && i < first + 15; i++)
        {
            float y = 80 + float(i) * rowH - scroll;
            if (i == chosen)
                c.roundRect(64, y + 2, 412, rowH - 4, 8, Blue);
            c.text(font, "Row " + formatInt(i + 1), 80, y + 7, i == chosen ? White : Color(0.75f, 0.8f, 0.88f));
            c.circle(450, y + rowH / 2, 8, hsv(float(i) * 0.02f, 0.7f, 1));
        }
        c.pop();
        float bar = viewH * viewH / (float(rows) * rowH);
        c.roundRect(484, 80 + scroll / (float(rows) * rowH) * viewH, 6, bar < 24 ? 24 : bar, 3, Gray);

        // Each window clips what is inside it, to the overlap of them all.
        c.text(font, "windows in windows", 560, 24, Color(0.7f, 0.76f, 0.86f));
        c.push();
        windowIn(c, 560, 70, 320, 280, 4);
        c.pop();

        // A spotlight, from thin clips.
        c.text(font, "a round mask from strips", 930, 24, Color(0.7f, 0.76f, 0.86f));
        float cx = 1080 + cos(t) * 80;
        float cy = 210 + sin(t * 1.4f) * 70;
        c.rect(930, 70, 300, 280, Color(0.1f, 0.1f, 0.14f));
        for (float y = -60; y < 60; y += 5)
        {
            float half = sqrt(3600 - y * y);
            c.push();
            c.clip(930, 70, 300, 280);
            c.clip(cx - half, cy + y, half * 2, 5);
            picture(c, 930, 70, 300, 280);
            c.pop();
        }

        // A wipe between two pictures.
        c.text(font, "a wipe", 560, 400, Color(0.7f, 0.76f, 0.86f));
        float at = 670 * (0.5f + 0.5f * sin(t * 0.8f));
        c.push();
        c.clip(560, 440, at, 210);
        picture(c, 560, 440, 670, 210);
        c.pop();
        c.push();
        c.clip(560 + at, 440, 670 - at, 210);
        c.gradient(560, 440, 670, 210, Orange, Purple);
        c.text(font, "the other side", 895, 530, White, AlignCenter);
        c.pop();
        c.rect(560 + at - 2, 440, 4, 210, White);
    }

    void windowIn(Canvas@ c, float x, float y, float w, float h, int depth)
    {
        c.clip(x, y, w, h);
        c.roundRect(x, y, w, h, 10, hsv(0.55f + float(depth) * 0.08f, 0.5f, 0.35f + 0.1f * float(4 - depth)));
        c.rect(x, y, w, 26, Color(0, 0, 0, 0.3f));
        if (depth > 0)
            windowIn(c, x + 26 + sin(t + depth) * 50, y + 46 + cos(t * 1.2f + depth) * 30, w - 40, h - 60, depth - 1);
        else
            c.circle(x + w / 2 + sin(t * 2) * 90, y + h / 2, 34, Yellow);
    }

    void picture(Canvas@ c, float x, float y, float w, float h)
    {
        c.gradient(x, y, w, h, Color(0.3f, 0.6f, 0.95f), Color(0.8f, 0.9f, 1));
        c.circle(x + w * 0.75f, y + h * 0.3f, 34, Yellow);
        c.triangle(Vec2(x, y + h), Vec2(x + w * 0.35f, y + h * 0.35f), Vec2(x + w * 0.7f, y + h), Green, Color(0.6f, 0.9f, 0.6f), Green);
        c.triangle(Vec2(x + w * 0.4f, y + h), Vec2(x + w * 0.72f, y + h * 0.5f), Vec2(x + w, y + h), Color(0.15f, 0.55f, 0.3f), Green, Color(0.15f, 0.55f, 0.3f));
    }
}
