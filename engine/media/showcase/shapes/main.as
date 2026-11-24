// Every shape the canvas can draw.

class ShapesScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 22);
    float t = 0;

    void update(float dt) { t += dt; }

    void cell(Canvas@ c, uint index, const string &in label)
    {
        float x = float(index % 4) * 310 + 30;
        float y = float(index / 4) * 225 + 30;
        c.roundRect(x, y, 290, 205, 14, Color(0.11f, 0.13f, 0.2f));
        c.text(font, label, x + 145, y + 170, Color(0.7f, 0.76f, 0.86f), AlignCenter);
        c.push();
        c.translate(x + 145, y + 88);
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.05f, 0.06f, 0.1f));

        cell(c, 0, "rect");
        c.rect(-70, -45, 80, 60, Blue);
        c.rect(-10, -15, 80, 60, fade(Orange, 0.8f));
        c.pop();

        cell(c, 1, "rectLine");
        for (int i = 0; i < 5; i++)
            c.rectLine(-90 + i * 12, -60 + i * 10, 180 - i * 24, 120 - i * 20, 3, hsv(float(i) * 0.12f + t * 0.1f, 0.7f, 1));
        c.pop();

        cell(c, 2, "roundRect");
        c.roundRect(-90, -50, 180, 100, 20 + 20 * sin(t * 2), Purple);
        c.pop();

        cell(c, 3, "gradient");
        c.gradient(-90, -60, 180, 120, hsv(t * 0.1f, 0.8f, 1), hsv(t * 0.1f + 0.4f, 0.8f, 0.6f));
        c.pop();

        cell(c, 4, "line");
        for (int i = 0; i < 16; i++)
        {
            float a = float(i) * 3.14159f / 8 + t * 0.4f;
            c.line(cos(a) * 18, sin(a) * 18, cos(a) * 70, sin(a) * 70, 2 + float(i % 4), hsv(float(i) / 16, 0.6f, 1));
        }
        c.pop();

        cell(c, 5, "circle");
        for (int i = 6; i > 0; i--)
            c.circle(sin(t + i) * 8, cos(t * 1.2f + i) * 8, float(i) * 11, hsv(float(i) * 0.06f + 0.5f, 0.7f, 0.4f + 0.1f * float(6 - i)));
        c.pop();

        cell(c, 6, "ring and arc");
        c.ring(0, 0, 70, 6, Color(0.2f, 0.24f, 0.34f));
        c.arc(0, 0, 70, 6, t * 90, t * 90 + 120 + 60 * sin(t), Green);
        c.arc(0, 0, 46, 14, -90, -90 + 360 * smooth(0.5f + 0.5f * sin(t)), Yellow);
        c.pop();

        cell(c, 7, "slices");
        float from = -90;
        array<float> parts = { 0.35f, 0.25f, 0.2f, 0.12f, 0.08f };
        for (uint i = 0; i < parts.length(); i++)
        {
            float to = from + parts[i] * 360;
            float middle = (from + to) * 0.5f * 3.14159f / 180;
            c.arc(cos(middle) * 5, sin(middle) * 5, 68, 68, from, to, hsv(float(i) * 0.15f, 0.65f, 0.95f));
            from = to;
        }
        c.pop();

        cell(c, 8, "triangle");
        c.rotate(t * 30);
        c.triangle(Vec2(0, -70), Vec2(62, 40), Vec2(-62, 40), Red, Green, Blue);
        c.pop();

        cell(c, 9, "transforms");
        for (int i = 0; i < 8; i++)
        {
            c.rotate(45);
            c.push();
            c.translate(0, -48);
            c.rotate(t * 120);
            c.scale(1 + 0.4f * sin(t * 3 + i), 1);
            c.rect(-12, -12, 24, 24, hsv(float(i) / 8, 0.7f, 1));
            c.pop();
        }
        c.pop();

        cell(c, 10, "curves from lines");
        for (int i = 0; i < 60; i++)
        {
            float x0 = -95 + float(i) * 190 / 60;
            float x1 = -95 + float(i + 1) * 190 / 60;
            c.line(x0, sin(x0 * 0.05f + t * 3) * 45, x1, sin(x1 * 0.05f + t * 3) * 45, 4, Orange);
            c.line(x0, cos(x0 * 0.07f - t * 2) * 30, x1, cos(x1 * 0.07f - t * 2) * 30, 2, Blue);
        }
        c.pop();

        cell(c, 11, "star");
        for (int i = 0; i < 5; i++)
        {
            float a = float(i) * 1.25664f - 1.5708f + t * 0.5f;
            Vec2 tip(cos(a) * 72, sin(a) * 72);
            Vec2 l(cos(a - 0.62832f) * 30, sin(a - 0.62832f) * 30);
            Vec2 r(cos(a + 0.62832f) * 30, sin(a + 0.62832f) * 30);
            c.triangle(tip, l, Vec2(0, 0), Yellow, Orange, Orange);
            c.triangle(tip, r, Vec2(0, 0), Yellow, Orange, Orange);
        }
        c.pop();
    }
}
