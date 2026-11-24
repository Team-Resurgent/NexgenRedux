// Script objects are freed as soon as nothing refers to them; loops of them by a collector that does a
// little every frame. The graph should wander, never climb.

class Spark
{
    Vec2 at, speed;
    float life;
    Spark@ other;      // half of these point at each other, so counting alone can't free them
    string note = "a string of its own";
}

class MemoryScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    Font@ big = assets::font("global:freesans.sfn", 44);
    array<Spark@> sparks;
    array<float> history(240);
    uint at = 0;
    float clock = 0;
    uint made = 0;
    uint perSecond = 600;

    void update(float dt)
    {
        if (input::repeated("right") && perSecond < 3000) perSecond += 200;
        if (input::repeated("left") && perSecond > 200) perSecond -= 200;
        uint make = perSecond / 60;
        for (uint i = 0; i < make; i++)
        {
            Spark a;
            a.at = Vec2(640, 300);
            float angle = random(0, 6.283f), speed = random(60, 320);
            a.speed = Vec2(cos(angle) * speed, sin(angle) * speed);
            a.life = random(0.4f, 1.4f);
            if (i % 2 == 0)
            {
                Spark b;
                @a.other = b;
                @b.other = a;
            }
            sparks.insertLast(a);
            made++;
        }
        for (uint i = 0; i < sparks.length();)
        {
            Spark@ s = sparks[i];
            s.life -= dt;
            s.at = s.at + s.speed * dt;
            if (s.life <= 0)
            {
                @sparks[i] = sparks[sparks.length() - 1];
                sparks.removeLast();
            }
            else
                i++;
        }
        clock += dt;
        if (clock > 0.05f)
        {
            clock = 0;
            history[at] = float(app::scriptKilobytes());
            at = (at + 1) % history.length();
        }
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.04f, 0.04f, 0.07f));
        c.setBlend(BlendAdd);
        for (uint i = 0; i < sparks.length(); i++)
        {
            Spark@ s = sparks[i];
            c.rect(s.at.x - 3, s.at.y - 3, 6, 6, fade(hsv(s.life * 0.12f, 0.9f, 1), clamp(s.life, 0, 1)));
        }
        c.setBlend(BlendAlpha);

        c.text(big, formatInt(made), 60, 40, White);
        c.text(font, "objects made so far", 62, 100, Gray);
        c.text(big, formatInt(app::scriptKilobytes()) + " KB", 1220, 40, Green, AlignRight);
        c.text(font, "script memory now", 1220, 100, Gray, AlignRight);

        float most = 1;
        for (uint i = 0; i < history.length(); i++)
            if (history[i] > most) most = history[i];
        c.roundRect(50, 520, 1180, 140, 12, Color(0.09f, 0.1f, 0.15f));
        for (uint i = 0; i < history.length(); i++)
        {
            float h = history[(at + i) % history.length()] / most * 120;
            c.rect(60 + float(i) * 4.85f, 655 - h, 4, h, Color(0.3f, 0.8f, 0.5f));
        }
        c.text(font, formatInt(perSecond) + " a second. Left and right change it.", 640, 676, Yellow, AlignCenter);
    }
}
