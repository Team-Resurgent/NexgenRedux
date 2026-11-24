// Music is decoded as it plays. The bars are its spectrum and the circle is its wave.

class MusicScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    Font@ big = assets::font("global:freesans.sfn", 46);
    array<string> titles = { "Night Shift", "Slow Light" };
    array<Sound@> tunes = { assets::sound("asset:tune.ogg"), assets::sound("asset:ambient.mp3") };
    uint playing = 0;
    Voice voice;
    bool paused = false;
    array<float> peaks(32);
    float beat = 0;
    float turn = 0;

    void onEnter() { voice = tunes[0].playMusic(0.9f, 0.5f); }

    void update(float dt)
    {
        if (input::pressed("left") || input::pressed("right"))
        {
            // Crossfade.
            voice.stop(1.2f);
            playing = 1 - playing;
            voice = tunes[playing].playMusic(0.9f, 1.2f);
            paused = false;
        }
        if (input::pressed("accept"))
        {
            paused = !paused;
            voice.setPaused(paused);
        }
        float low = 0;
        for (uint i = 0; i < 4; i++) low += audio::spectrum(i) / 4;
        beat = low > beat ? low : beat * 0.92f;
        turn += dt * (6 + beat * 40);
        for (uint i = 0; i < 32; i++)
        {
            float now = audio::spectrum(i);
            peaks[i] = now > peaks[i] ? now : peaks[i] - dt * 0.35f;
        }
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.02f, 0.02f, 0.06f), Color(0.1f, 0.03f, 0.16f));

        // Rings that swell with the low notes.
        c.setBlend(BlendAdd);
        for (int i = 4; i > 0; i--)
            c.circle(640, 290, 120 + float(i) * 34 * (0.4f + beat), fade(hsv(0.6f + float(i) * 0.06f, 0.8f, 0.7f), 0.10f + beat * 0.08f));
        c.setBlend(BlendAlpha);

        c.push();
        c.translate(640, 290);
        c.rotate(turn);
        float lastX = 0, lastY = 0;
        for (uint i = 0; i <= 128; i++)
        {
            float a = float(i % 128) / 128 * 6.28318f;
            // Faded at the ends so the circle joins up.
            float ends = sin(float(i % 128) / 128 * 3.14159f);
            float r = 150 + audio::wave((i % 128) * 4) * 110 * ends + beat * 20;
            float x = cos(a) * r, y = sin(a) * r;
            if (i > 0) c.line(lastX, lastY, x, y, 4, hsv(0.5f + float(i) / 256, 0.6f, 1));
            lastX = x;
            lastY = y;
        }
        c.pop();

        for (uint i = 0; i < 32; i++)
        {
            float x = 80 + float(i) * 35;
            float h = audio::spectrum(i) * 190;
            Color color = hsv(0.52f + float(i) * 0.012f, 0.75f, 1);
            c.gradient(x, 600 - h, 28, h + 2, color, fade(color, 0.35f));
            c.rect(x, 600 - peaks[i] * 190 - 6, 28, 4, White);
            c.gradient(x, 604, 28, h * 0.25f, fade(color, 0.25f), fade(color, 0));
        }

        c.text(big, titles[playing], 60, 30);
        float length = tunes[playing].seconds;
        c.text(font, clock(voice.position) + (length > 0 ? " / " + clock(length) : "") + (paused ? "   paused" : ""), 62, 90, Color(0.7f, 0.75f, 0.9f));
        c.text(font, input::usingPad() ? "Left and right: change tune    A: pause" : "Left and right: change tune    Enter: pause", 640, 672, Color(0.7f, 0.75f, 0.9f), AlignCenter);
    }

    string clock(float t)
    {
        int s = int(t);
        return formatInt(s / 60) + ":" + formatInt(s % 60, "0", 2);
    }
}
