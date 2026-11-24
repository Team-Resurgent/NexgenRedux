// Sixteen pads. Where a pad is decides its panning, and holding the second button lowers its pitch.

class SoundsScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 22);
    Font@ big = assets::font("global:freesans.sfn", 40);
    array<string> names = { "kick", "snare", "hat", "bell", "pluck", "pop", "hit", "wall",
                            "coin", "boom", "whoosh", "win", "lose", "move", "accept", "back" };
    array<Sound@> sounds;
    array<Tween> flashes(16);
    uint chosen = 0;
    bool beat = false;
    uint beatStep = 0;
    array<float> levels(90);
    uint at = 0;

    SoundsScreen()
    {
        for (uint i = 0; i < names.length(); i++)
            sounds.insertLast(assets::sound("global:sound/" + names[i] + ".wav"));
    }

    void onEnter() { start(Coroutine(this.drums)); }

    void hit(uint pad, float volume)
    {
        float pan = (float(pad % 4) - 1.5f) / 1.5f;
        float pitch = input::held("alternate") ? 0.6f : 1.0f;
        sounds[pad].play(volume, pan, pitch);
        flashes[pad] = Tween(1, 0, 0.35f, EaseOut);
    }

    // A drum machine: a coroutine that plays and waits.
    void drums()
    {
        while (true)
        {
            if (beat)
            {
                uint s = beatStep % 16;
                if (s == 0 || s == 6 || s == 8 || s == 14) hit(0, 0.9f);
                if (s == 4 || s == 12) hit(1, 0.7f);
                if (s % 2 == 0) hit(2, 0.4f);
                if (s == 3 || s == 10) hit(4, 0.5f);
                beatStep++;
            }
            wait(0.125f);
        }
    }

    void update(float dt)
    {
        uint column = chosen % 4, row = chosen / 4;
        if (input::repeated("left") && column > 0) chosen--;
        if (input::repeated("right") && column < 3) chosen++;
        if (input::repeated("up") && row > 0) chosen -= 4;
        if (input::repeated("down") && row < 3) chosen += 4;
        if (input::pressed("accept")) hit(chosen, 1);
        if (input::pressed("cancel"))
        {
            beat = !beat;
            beatStep = 0;
        }
        levels[at] = audio::level();
        at = (at + 1) % levels.length();
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.1f, 0.06f, 0.12f), Color(0.03f, 0.03f, 0.06f));
        for (uint i = 0; i < 16; i++)
        {
            float x = 50 + float(i % 4) * 175, y = 50 + float(i / 4) * 145;
            Color color = hsv(float(i) / 16, 0.65f, 0.95f);
            float f = flashes[i].value;
            if (i == chosen) c.roundRect(x - 5, y - 5, 170, 140, 18, White);
            c.roundRect(x, y, 160, 130, 14, Color(0.12f + color.r * f * 0.8f, 0.13f + color.g * f * 0.8f, 0.18f + color.b * f * 0.8f));
            c.roundRect(x + 14, y + 14, 40, 8, 4, color);
            c.text(font, names[i], x + 80, y + 54, White, AlignCenter);
        }

        c.roundRect(780, 50, 450, 130, 14, Color(0.09f, 0.09f, 0.15f));
        c.text(big, formatInt(audio::voices()) + " of " + formatInt(audio::voiceLimit()), 1005, 66, Green, AlignCenter);
        c.text(font, "voices, through " + audio::output(), 1005, 128, Gray, AlignCenter);

        c.roundRect(780, 200, 450, 150, 14, Color(0.09f, 0.09f, 0.15f));
        for (uint i = 0; i + 1 < levels.length(); i++)
        {
            float a = levels[(at + i) % levels.length()] * 120;
            c.rect(800 + float(i) * 4.6f, 335 - a, 4, a + 1, Orange);
        }
        c.roundRect(780, 370, 450, 150, 14, Color(0.09f, 0.09f, 0.15f));
        for (uint i = 0; i < 100; i++)
            c.line(800 + float(i) * 4.1f, 445 + audio::wave(i * 5) * 70, 804 + float(i) * 4.1f, 445 + audio::wave(i * 5 + 5) * 70, 2, Blue);

        c.text(font, input::buttons("accept") + " or " + input::keys("accept") + ": play", 790, 550, Yellow);
        c.text(font, "Hold " + input::buttons("alternate") + " or " + input::keys("alternate") + ": lower", 790, 584, Yellow);
        c.text(font, input::buttons("cancel") + " or " + input::keys("cancel") + ": drum machine " + (beat ? "off" : "on"), 790, 618, Yellow);
    }
}
