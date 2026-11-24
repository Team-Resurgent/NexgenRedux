// What the asset cache is doing. Open other screens first and come back.

class CacheScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 26);
    Font@ big = assets::font("global:freesans.sfn", 54);
    Texture@ held;
    uint asked = 0;
    float t = 0;

    void update(float dt)
    {
        t += dt;
        // Asking again is free: the same image comes back without reading a file.
        for (int i = 0; i < 10; i++)
        {
            Font@ again = assets::font("global:freesans.sfn", 26);
            asked++;
        }
        if (input::pressed("accept"))
            assets::trim();
    }

    void number(Canvas@ c, float x, float y, uint value, const string &in label, const Color &in color)
    {
        c.roundRect(x, y, 280, 130, 14, Color(0.09f, 0.11f, 0.17f));
        c.text(big, formatInt(value), x + 140, y + 16, color, AlignCenter);
        c.text(font, label, x + 140, y + 84, Color(0.65f, 0.72f, 0.82f), AlignCenter);
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.05f, 0.06f, 0.1f));
        number(c, 50, 40, assets::inUse(), "in use", Green);
        number(c, 350, 40, assets::warm(), "kept warm", Orange);
        number(c, 650, 40, assets::loads(), "files read", Blue);
        number(c, 950, 40, assets::evictions(), "unloaded for room", Red);
        number(c, 50, 190, assets::hits(), "requests without reading", Purple);
        number(c, 350, 190, asked, "asked by this screen", Gray);

        float used = float(assets::textureKilobytes());
        float budget = float(assets::budgetKilobytes());
        c.text(font, "Budget", 660, 196, White);
        c.text(font, formatInt(int(used)) + " of " + formatInt(int(budget)) + " KB", 1230, 196, White, AlignRight);
        c.roundRect(660, 240, 570, 26, 13, Color(0.15f, 0.18f, 0.26f));
        float w = 570 * clamp(used / budget, 0, 1);
        c.roundRect(660, 240, w < 26 ? 26 : w, 26, 13, Green);

        c.paragraph(font, "Each file is loaded once and shared by everyone who asks for it. Letting go of it doesn't unload it: it stays warm, "
            "so going back to a screen is instant. Only when the budget is full is anything unloaded, starting with what was used longest ago.",
            60, 370, 1160, Color(0.85f, 0.88f, 0.95f));
        c.text(font, input::usingPad() ? "A  unload everything that isn't in use" : "Enter  unload everything that isn't in use", 60, 620, Yellow);
    }
}
