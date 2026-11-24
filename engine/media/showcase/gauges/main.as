// Dashboard pieces, showing live numbers from the engine itself.

class GaugesScreen : Screen
{
    Font@ big = assets::font("global:freesans.sfn", 44);
    Font@ small = assets::font("global:freesans.sfn", 20);
    array<float> history(120);
    uint at = 0;
    float t = 0;
    float shown = 0;

    void update(float dt)
    {
        t += dt;
        shown = lerp(shown, 1, 0.04f);
        history[at] = app::fps();
        at = (at + 1) % history.length();
    }

    void dial(Canvas@ c, float x, float y, float value, float most, const string &in label, const Color &in color)
    {
        c.arc(x, y, 110, 18, 135, 405, Color(0.15f, 0.18f, 0.26f));
        float part = clamp(value / most, 0, 1) * shown;
        c.arc(x, y, 110, 18, 135, 135 + 270 * part, color);
        for (int i = 0; i <= 10; i++)
        {
            float a = (135 + float(i) * 27) * 3.14159f / 180;
            c.line(x + cos(a) * 80, y + sin(a) * 80, x + cos(a) * 86, y + sin(a) * 86, 2, Gray);
        }
        float n = (135 + 270 * part) * 3.14159f / 180;
        c.line(x, y, x + cos(n) * 70, y + sin(n) * 70, 4, White);
        c.circle(x, y, 9, White);
        c.text(big, formatInt(int(value + 0.5f)), x, y + 34, White, AlignCenter);
        c.text(small, label, x, y + 92, Color(0.65f, 0.72f, 0.82f), AlignCenter);
    }

    void meter(Canvas@ c, float y, float value, float most, const string &in label, const Color &in color)
    {
        c.text(small, label, 60, y, Color(0.75f, 0.8f, 0.9f));
        c.text(small, formatInt(int(value)) + " KB", 600, y, White, AlignRight);
        c.roundRect(60, y + 30, 540, 18, 9, Color(0.15f, 0.18f, 0.26f));
        float w = 540 * clamp(value / most, 0, 1) * shown;
        c.roundRect(60, y + 30, w < 18 ? 18 : w, 18, 9, color);
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.06f, 0.08f, 0.13f), Color(0.03f, 0.04f, 0.07f));

        dial(c, 190, 190, app::fps(), 120, "frames a second", Green);
        dial(c, 500, 190, float(app::draws()), 100, "draw calls", Blue);
        dial(c, 810, 190, float(assets::inUse() + assets::warm()), 40, "assets loaded", Purple);
        dial(c, 1100, 190, 50 + 50 * sin(t), 100, "a sine wave", Orange);

        meter(c, 370, float(app::memoryKilobytes()), 8192, "Engine memory", Blue);
        meter(c, 440, float(app::scriptKilobytes()), 4096, "Scripts", Purple);
        meter(c, 510, float(assets::textureKilobytes()), float(assets::budgetKilobytes()), "Images and fonts, of their budget", Green);
        meter(c, 580, float(assets::soundKilobytes()), float(assets::soundBudgetKilobytes()), "Sounds, of their budget", Orange);

        // Frame rate over the last two seconds.
        c.roundRect(660, 370, 560, 270, 14, Color(0.09f, 0.11f, 0.17f));
        c.text(small, "Frame rate", 684, 384, Color(0.65f, 0.72f, 0.82f));
        for (int i = 0; i <= 4; i++)
        {
            float y = 620 - float(i) * 50;
            c.rect(684, y, 512, 1, Color(0.2f, 0.24f, 0.32f));
            c.text(small, formatInt(i * 30), 1208, y - 12, Gray, AlignLeft);
        }
        for (uint i = 0; i + 1 < history.length(); i++)
        {
            float a = clamp(history[(at + i) % history.length()], 0, 130);
            float b = clamp(history[(at + i + 1) % history.length()], 0, 130);
            float x = 684 + float(i) * 512 / float(history.length());
            c.line(x, 620 - a * 50 / 30, x + 4.4f, 620 - b * 50 / 30, 3, Green);
        }
    }
}
