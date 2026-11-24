// Text: one scalable font at several sizes, alignment, wrapping and effects letter by letter.

class TextScreen : Screen
{
    array<Font@> sizes;
    Font@ body = assets::font("global:freesans.sfn", 24);
    Font@ big = assets::font("global:freesans.sfn", 64);
    float t = 0;

    string story = "Glyphs are drawn once into an atlas, so changing a line of text costs nothing but a few quads. "
        "This paragraph is wrapped to the width of the box beside it, and the box changes width all the time. "
        "Accents work too: caf\xC3\xA9, na\xC3\xAFve, \xC3\x9C" "ber, \xC2\xA3" "5 and \xC2\xA9.";

    void onEnter()
    {
        array<uint> points = { 14, 20, 28, 40, 56 };
        for (uint i = 0; i < points.length(); i++)
            sizes.insertLast(assets::font("global:freesans.sfn", points[i]));
    }

    void update(float dt) { t += dt; }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.06f, 0.06f, 0.09f));

        float y = 24;
        for (uint i = 0; i < sizes.length(); i++)
        {
            c.text(sizes[i], "The quick brown fox", 40, y, hsv(0.5f + float(i) * 0.06f, 0.4f, 1));
            y += sizes[i].measure("X").y;
        }

        // Alignment around a line.
        c.line(960, 30, 960, 190, 2, Color(0.3f, 0.34f, 0.44f));
        c.text(body, "left of the line", 960, 40, White, AlignRight);
        c.text(body, "centred on it", 960, 90, Yellow, AlignCenter);
        c.text(body, "right of the line", 960, 140, Green, AlignLeft);

        // Wrapping.
        float width = 420 + 140 * sin(t * 0.7f);
        float height = c.paragraph(body, story, 60, 280, width, Color(0.85f, 0.88f, 0.95f));
        c.rectLine(50, 270, width + 20, height + 20, 2, Color(0.3f, 0.4f, 0.6f));

        // Letter by letter.
        wave(c, "WAVE", 700, 290);
        rainbow(c, "RAINBOW", 700, 390);
        shake(c, "SHAKE", 700, 490);
        typed(c, "Typed one letter at a time...", 700, 600);

        c.push();
        c.translate(1120, 560);
        c.rotate(t * 25);
        c.text(body, "rotated", 0, -14, Orange, AlignCenter);
        c.pop();
        c.push();
        c.translate(1120, 380);
        float s = 1 + 0.5f * sin(t * 2);
        c.scale(s, s);
        c.text(body, "scaled", 0, -14, Purple, AlignCenter);
        c.pop();
    }

    void wave(Canvas@ c, const string &in text, float x, float y)
    {
        for (uint i = 0; i < text.length(); i++)
        {
            string letter = text.substr(i, 1);
            x += c.text(big, letter, x, y + sin(t * 4 + float(i) * 0.7f) * 16, Blue);
        }
    }

    void rainbow(Canvas@ c, const string &in text, float x, float y)
    {
        for (uint i = 0; i < text.length(); i++)
            x += c.text(big, text.substr(i, 1), x, y, hsv(t * 0.3f + float(i) * 0.1f, 0.7f, 1));
    }

    void shake(Canvas@ c, const string &in text, float x, float y)
    {
        for (uint i = 0; i < text.length(); i++)
            x += c.text(big, text.substr(i, 1), x + random(-3, 3), y + random(-3, 3), Red);
    }

    void typed(Canvas@ c, const string &in text, float x, float y)
    {
        uint count = uint(t * 12) % (text.length() + 20);
        string shown = text.substr(0, count > text.length() ? text.length() : count);
        float w = c.text(body, shown, x, y, White);
        if (int(t * 3) % 2 == 0)
            c.rect(x + w + 3, y, 3, 26, White);
    }
}
