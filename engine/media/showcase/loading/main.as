// Asked for now, here in a moment: files are read and decoded on another thread.

class LoadingScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    Sound@ pop = assets::sound("global:sound/pop.wav");
    array<Texture@> pictures(12);
    array<Tween> shown(12);
    array<bool> seen(12);
    array<float> frames(160);
    uint at = 0;
    float lastDraw = 0;
    float worst = 0;

    void onEnter() { start(Coroutine(this.fetch)); }

    void fetch()
    {
        for (uint i = 0; i < 12; i++)
        {
            @pictures[i] = null;
            shown[i] = Tween(0);
            seen[i] = false;
        }
        wait(0.2f);
        // Nothing holds them now, so this really does read them again.
        assets::trim();
        worst = 0;
        for (uint i = 0; i < 12; i++)
            @pictures[i] = assets::texture("asset:picture" + formatInt(i) + ".jpg");
    }

    void update(float dt)
    {
        if (input::pressed("accept")) start(Coroutine(this.fetch));
        for (uint i = 0; i < 12; i++)
        {
            if (!seen[i] && pictures[i] !is null && pictures[i].ready)
            {
                seen[i] = true;
                shown[i] = Tween(0, 1, 0.4f, EaseOvershoot);
                pop.play(0.5f, (float(i % 4) - 1.5f) / 2, 0.8f + float(i) * 0.06f);
            }
        }
    }

    void draw(Canvas@ c)
    {
        float now = app::time();
        float took = (now - lastDraw) * 1000;
        lastDraw = now;
        frames[at] = took;
        at = (at + 1) % frames.length();
        if (took > worst && took < 1000) worst = took;

        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.06f, 0.07f, 0.11f), Color(0.03f, 0.03f, 0.05f));
        for (uint i = 0; i < 12; i++)
        {
            float x = 60 + float(i % 4) * 190, y = 50 + float(i / 4) * 190;
            c.roundRect(x, y, 170, 170, 12, Color(0.11f, 0.13f, 0.2f));
            c.arc(x + 85, y + 85, 26, 5, now * 300, now * 300 + 250, Color(0.3f, 0.4f, 0.6f));
            float s = shown[i].value * 170;
            if (seen[i]) c.image(pictures[i], x + 85 - s / 2, y + 85 - s / 2, s, s);
        }

        // Something that would visibly stutter.
        for (uint i = 0; i < 12; i++)
        {
            float a = now * 2.4f + float(i) * 0.5236f;
            c.circle(1040 + cos(a) * 110, 200 + sin(a) * 110, 14, hsv(float(i) / 12, 0.6f, 1));
        }
        c.text(font, formatInt(assets::pending()) + " on their way", 1040, 186, White, AlignCenter);

        c.text(font, "Frame time", 860, 390, Color(0.7f, 0.76f, 0.88f));
        c.text(font, "longest " + formatInt(int(worst + 0.5f)) + " ms", 1230, 390, Yellow, AlignRight);
        c.roundRect(850, 430, 390, 130, 10, Color(0.1f, 0.11f, 0.17f));
        for (uint i = 0; i < frames.length(); i++)
        {
            float ms = clamp(frames[(at + i) % frames.length()], 0, 60);
            c.rect(860 + float(i) * 2.3f, 555 - ms * 2, 2, ms * 2, ms > 25 ? Red : Green);
        }
        c.text(font, input::usingPad() ? "A  load them again" : "Enter  load them again", 60, 650, Yellow);
    }
}
