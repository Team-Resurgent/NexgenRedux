// The same journey, six ways.

class EasingScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    array<string> names = { "linear", "smooth", "bounce", "ease in", "ease out", "overshoot" };
    float t = 0;

    void update(float dt) { t += dt; }

    float ease(uint kind, float v)
    {
        if (kind == 0) return v;
        if (kind == 1) return smooth(v);
        if (kind == 2) return bounce(v);
        if (kind == 3) return v * v * v;
        if (kind == 4) return 1 - (1 - v) * (1 - v) * (1 - v);
        float s = v - 1;
        return s * s * (2.7f * s + 1.7f) + 1;
    }

    // There, a pause, and back.
    float journey()
    {
        float phase = (t * 0.5f) % 2.6f;
        if (phase < 1) return phase;
        if (phase < 1.3f) return 1;
        if (phase < 2.3f) return 2.3f - phase;
        return 0;
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.06f, 0.07f, 0.11f));
        float v = journey();
        for (uint i = 0; i < names.length(); i++)
        {
            float y = 40 + float(i) * 112;
            Color color = hsv(0.02f + float(i) * 0.14f, 0.65f, 1);
            c.text(font, names[i], 40, y + 32, color);

            // Its curve.
            c.roundRect(200, y, 150, 96, 8, Color(0.1f, 0.12f, 0.18f));
            for (int s = 0; s < 30; s++)
            {
                float a = float(s) / 30, b = float(s + 1) / 30;
                c.line(210 + a * 130, y + 84 - ease(i, a) * 70, 210 + b * 130, y + 84 - ease(i, b) * 70, 2, fade(color, 0.6f));
            }
            c.circle(210 + v * 130, y + 84 - ease(i, v) * 70, 6, White);

            // And in motion.
            c.rect(400, y + 47, 820, 2, Color(0.2f, 0.24f, 0.34f));
            float e = ease(i, v);
            for (int g = 4; g > 0; g--)
                c.circle(420 + ease(i, clamp(v - float(g) * 0.02f, 0, 1)) * 780, y + 48, 24, fade(color, 0.08f));
            c.push();
            c.translate(420 + e * 780, y + 48);
            c.rotate(e * 360);
            c.roundRect(-24, -24, 48, 48, 10, color);
            c.pop();
        }
    }
}
