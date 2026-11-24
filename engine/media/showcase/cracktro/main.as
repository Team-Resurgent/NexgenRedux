// The Nexgen cracktro, rebuilt: scroller, raster bars, starfield, sprite sheet and line mesh.

class CracktroScreen : Screen
{
    Texture@ sheet = assets::texture("asset:sprite-sheet.png");
    Font@ font = assets::font("global:freesans.sfn", 44);
    Font@ logo = assets::font("global:freesans.sfn", 110);

    string message = "HELLO WORLD ... THIS SCROLLER IS ONE STRING, DRAWN A LETTER AT A TIME WITH EVERY LETTER ON ITS OWN SINE WAVE ... "
        "GREETINGS TO EVERYONE WHO STILL WRITES CODE FOR OLD MACHINES ...      ";
    array<float> widths;
    float total = 0;
    float t = 0;

    array<float> sx, sy, sz;

    void onEnter()
    {
        // Measured once: the scroller only needs to know how wide each letter is.
        for (uint i = 0; i < message.length(); i++)
        {
            widths.insertLast(font.measure(message.substr(i, 1)).x);
            total += widths[i];
        }
        for (int i = 0; i < 180; i++)
        {
            sx.insertLast(random(-1, 1));
            sy.insertLast(random(-1, 1));
            sz.insertLast(random(0.05f, 1));
        }
    }

    void update(float dt)
    {
        t += dt;
        for (uint i = 0; i < sz.length(); i++)
        {
            sz[i] -= dt * 0.35f;
            if (sz[i] < 0.05f)
                sz[i] = 1;
        }
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.01f, 0.01f, 0.03f));

        for (uint i = 0; i < sz.length(); i++)
        {
            float px = 640 + sx[i] / sz[i] * 300;
            float py = 360 + sy[i] / sz[i] * 300;
            float s = (1 - sz[i]) * 4 + 0.5f;
            c.rect(px, py, s, s, Color(0.8f, 0.85f, 1, 1 - sz[i]));
        }

        // Raster bars.
        c.setBlend(BlendAdd);
        for (int i = 0; i < 9; i++)
        {
            float y = 330 + sin(t * 1.4f + float(i) * 0.35f) * 190;
            Color bar = hsv(float(i) / 9 + t * 0.05f, 0.8f, 0.8f);
            c.gradient(0, y, 1280, 16, fade(bar, 0), bar);
            c.gradient(0, y + 16, 1280, 16, bar, fade(bar, 0));
        }
        c.setBlend(BlendAlpha);

        float lx = 640;
        c.text(logo, "NEXGEN", lx + 5, 46 + 5, Color(0, 0, 0, 0.6f), AlignCenter);
        c.text(logo, "NEXGEN", lx, 46, hsv(t * 0.1f, 0.3f, 1), AlignCenter);

        // The sheet is 16 by 16 frames.
        uint frame = uint(t * 20) % 255;
        for (int i = 0; i < 5; i++)
        {
            float a = t * 0.8f + float(i) * 1.2566f;
            c.push();
            c.translate(640 + cos(a) * 330, 370 + sin(a) * 110);
            float s = 1.6f + sin(a) * 0.5f;
            c.scale(s, s);
            c.rotate(sin(t * 2 + float(i)) * 12);
            c.setFilter(FilterNearest);
            c.sprite(sheet, 16, 16, (frame + uint(i) * 40) % 255, -32, -32, 64, 64);
            c.pop();
        }

        // A ring of lines, as in the first version.
        for (int i = 0; i < 36; i++)
        {
            float a0 = float(i) * 0.17453f + t;
            float a1 = float(i + 1) * 0.17453f + t;
            float r = 250 + sin(t * 3 + float(i) * 0.5f) * 10;
            c.line(640 + cos(a0) * r, 370 + sin(a0) * r * 0.5f, 640 + cos(a1) * r, 370 + sin(a1) * r * 0.5f, 2, hsv(float(i) / 36, 0.6f, 1));
        }

        // Scroller: start at the first letter that can be seen.
        float x = 1280 - (t * 220) % (total + 1280);
        for (uint i = 0; i < message.length() && x < 1290; i++)
        {
            if (x + widths[i] > -10)
            {
                float y = 610 + sin(x * 0.008f + t * 3) * 40;
                c.text(font, message.substr(i, 1), x, y, hsv(x * 0.0006f + t * 0.1f, 0.5f, 1));
            }
            x += widths[i];
        }
    }
}
