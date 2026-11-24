// A Tween is a number that moves on its own. There is nothing to update: read its value and draw.

class TweensScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 22);
    Font@ big = assets::font("global:freesans.sfn", 34);
    array<string> names = { "Linear", "Smooth", "In", "Out", "Bounce", "Overshoot", "Elastic" };
    array<Ease> kinds = { EaseLinear, EaseSmooth, EaseIn, EaseOut, EaseBounce, EaseOvershoot, EaseElastic };
    array<Tween> balls(7);
    array<Tween> cardX(6);
    array<Tween> cardY(6);
    array<Tween> cardTurn(6);
    bool atRight = false;
    bool fanned = false;
    Tween panel(-60);

    void onEnter()
    {
        for (uint i = 0; i < 6; i++)
        {
            cardX[i] = Tween(1050);
            cardY[i] = Tween(900);
        }
        panel.go(0, 0.8f, EaseOvershoot);
        start(Coroutine(this.show));
    }

    void show()
    {
        while (true)
        {
            atRight = !atRight;
            for (uint i = 0; i < 7; i++)
                balls[i].go(atRight ? 1 : 0, 1.4f, kinds[i]);
            fanned = !fanned;
            for (uint i = 0; i < 6; i++)
            {
                float spread = float(i) - 2.5f;
                cardX[i].go(fanned ? 1050 + spread * 62 : 1050 + float(i) * 3, 0.5f, EaseOvershoot);
                cardY[i].go(fanned ? 360 + spread * spread * 7 : 360 - float(i) * 3, 0.5f, EaseOut);
                cardTurn[i].go(fanned ? spread * 9 : 0, 0.5f, EaseOvershoot);
                wait(0.07f);
            }
            wait(1.5f);
        }
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.05f, 0.09f, 0.12f), Color(0.03f, 0.04f, 0.07f));
        c.push();
        c.translate(0, panel.value);
        c.text(big, "Seven ways to get there", 60, 36);
        for (uint i = 0; i < 7; i++)
        {
            float y = 130 + float(i) * 76;
            Color color = hsv(float(i) / 7, 0.65f, 1);
            c.text(font, names[i], 60, y - 14, Color(0.7f, 0.76f, 0.85f));
            c.roundRect(200, y - 3, 500, 6, 3, Color(0.16f, 0.2f, 0.27f));
            c.circle(210 + balls[i].value * 480, y, 18, color);
            // The curve itself.
            for (uint k = 0; k < 20; k++)
            {
                float a = float(k) / 20, b = float(k + 1) / 20;
                c.line(730 + a * 60, y + 18 - ease(kinds[i], a) * 36, 730 + b * 60, y + 18 - ease(kinds[i], b) * 36, 2, color);
            }
        }
        c.pop();

        for (uint i = 0; i < 6; i++)
        {
            c.push();
            c.translate(cardX[i].value, cardY[i].value);
            c.rotate(cardTurn[i].value);
            c.roundRect(-62, -92, 124, 184, 14, Color(0, 0, 0, 0.35f));
            c.roundRect(-60, -90, 120, 180, 12, White);
            c.roundRect(-52, -82, 104, 164, 8, hsv(float(i) / 6 + 0.55f, 0.6f, 0.95f));
            c.circle(0, 0, 30, White);
            c.text(big, formatInt(i + 1), 0, -20, Color(0.1f, 0.1f, 0.2f), AlignCenter);
            c.pop();
        }
    }
}
