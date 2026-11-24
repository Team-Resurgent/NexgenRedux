// Blend modes, and a fire built from additive particles.

class Particle
{
    float x, y, vx, vy, life, size;
}

class BlendScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 22);
    array<Particle> sparks(260);   // reused for ever, so nothing is allocated while it burns
    float t = 0;

    void onEnter()
    {
        for (uint i = 0; i < sparks.length(); i++)
        {
            light(sparks[i]);
            sparks[i].life = random();
        }
    }

    void light(Particle@ p)
    {
        p.x = random(-40, 40);
        p.y = random(-6, 6);
        p.vx = random(-18, 18);
        p.vy = random(-190, -90);
        p.life = 1;
        p.size = random(10, 26);
    }

    void update(float dt)
    {
        t += dt;
        for (uint i = 0; i < sparks.length(); i++)
        {
            Particle@ p = sparks[i];
            p.life -= dt * 0.75f;
            p.x += (p.vx + sin(t * 3 + p.y * 0.03f) * 30) * dt;
            p.y += p.vy * dt;
            if (p.life <= 0)
                light(p);
        }
    }

    void lights(Canvas@ c, float x, float y, Blend mode, const string &in label)
    {
        c.push();
        c.setBlend(mode);
        for (int i = 0; i < 3; i++)
        {
            float a = t + float(i) * 2.0944f;
            c.circle(x + cos(a) * 46, y + sin(a) * 46, 70, fade(hsv(float(i) / 3, 1, 0.9f), mode == BlendNone ? 1.0f : 0.75f));
        }
        c.pop();
        c.text(font, label, x, y + 140, White, AlignCenter);
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.05f, 0.05f, 0.1f), Color(0.1f, 0.05f, 0.08f));
        for (int i = 0; i < 16; i++)
            c.rect(float(i) * 80, 0, 40, 720, Color(1, 1, 1, 0.02f));

        lights(c, 170, 190, BlendNone, "BlendNone");
        lights(c, 470, 190, BlendAlpha, "BlendAlpha");
        lights(c, 770, 190, BlendAdd, "BlendAdd");

        // Fire.
        c.push();
        c.translate(1080, 560);
        c.setBlend(BlendAdd);
        for (uint i = 0; i < sparks.length(); i++)
        {
            Particle@ p = sparks[i];
            float s = p.size * (0.4f + p.life * 0.6f);
            c.circle(p.x, p.y, s, Color(1, 0.25f + 0.5f * p.life * p.life, 0.1f * p.life, 0.16f * p.life));
        }
        c.pop();
        c.roundRect(1000, 566, 160, 26, 8, Color(0.25f, 0.17f, 0.12f));
        c.text(font, "260 additive particles", 1080, 610, White, AlignCenter);

        // Fading.
        for (int i = 0; i < 10; i++)
        {
            c.push();
            c.setAlpha(float(i + 1) / 10);
            c.roundRect(70 + float(i) * 78, 440, 66, 120, 10, Blue);
            c.pop();
            c.text(font, formatInt((i + 1) * 10) + "%", 103 + float(i) * 78, 570, Gray, AlignCenter);
        }
        c.text(font, "setAlpha", 460, 620, White, AlignCenter);
    }
}
