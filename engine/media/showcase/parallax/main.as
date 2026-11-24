// Layers that scroll at different speeds. The old version loaded a set of images per resolution;
// this one is drawn once in design units and fits any screen.

class ParallaxScreen : Screen
{
    Texture@ nebula = assets::texture("asset:nebula.png");
    Texture@ farStars = assets::texture("asset:stars1.png");
    Texture@ nearStars = assets::texture("asset:stars2.png");
    Texture@ ship = assets::texture("asset:ship.png");
    Font@ font = assets::font("global:freesans.sfn", 22);

    float travel = 0;
    float speed = 120;
    Vec2 position(300, 360);
    float bank = 0;

    void update(float dt)
    {
        Vec2 move = input::move();
        speed = clamp(speed + move.x * 260 * dt, 40, 700);
        position.y = clamp(position.y + move.y * 320 * dt, 80, 640);
        position.x = lerp(position.x, 220 + speed * 0.5f, 0.05f);
        bank = lerp(bank, move.y * 14, 0.1f);
        travel += speed * dt;
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.tile(nebula, 0, 0, 1280, 720, 900, 900, -travel * 0.08f, -60);
        c.tile(farStars, 0, 0, 1280, 720, 700, 700, -travel * 0.25f, 0);
        c.tile(nearStars, 0, 0, 1280, 720, 900, 900, -travel * 0.6f, 40);

        // Streaks close to the camera.
        for (int i = 0; i < 24; i++)
        {
            float depth = 1.2f + float(i % 5) * 0.5f;
            float x = 1400 - float((travel * depth + float(i) * 173)) % 1600;
            float y = float(i * 97 % 720);
            c.rect(x, y, speed * 0.12f * depth, 2, Color(0.7f, 0.85f, 1, 0.35f));
        }

        c.push();
        c.translate(position.x, position.y);
        c.rotate(90 + bank);
        c.setBlend(BlendAdd);
        float flame = 26 + speed * 0.06f + random(0, 10);
        c.triangle(Vec2(-14, 44), Vec2(14, 44), Vec2(0, 44 + flame), Color(0.4f, 0.7f, 1, 0.9f), Color(0.4f, 0.7f, 1, 0.9f), Color(0.1f, 0.2f, 1, 0));
        c.setBlend(BlendAlpha);
        c.image(ship, -55, -48, 110, 95);
        c.pop();

        c.roundRect(30, 650, 620, 44, 12, Color(0, 0, 0, 0.5f));
        c.text(font, "Left and right change speed, up and down steer.  Speed " + formatInt(int(speed)), 50, 660);
    }
}
