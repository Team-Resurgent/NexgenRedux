// Sprite animation: five ripple frames around a ship, over tiled water.

class WavesScreen : Screen
{
    Texture@ water = assets::texture("asset:water.png");
    Texture@ ship = assets::texture("asset:ship.png");
    array<Texture@> ripples;
    Font@ font = assets::font("global:freesans.sfn", 22);

    Vec2 position(640, 400);
    Vec2 velocity;
    float heading = 0;
    float frame = 0;
    float t = 0;

    void onEnter()
    {
        for (int i = 0; i < 5; i++)
            ripples.insertLast(assets::texture("asset:ripple" + formatInt(i) + ".png"));
    }

    void update(float dt)
    {
        t += dt;
        velocity = velocity + input::move() * (600 * dt);
        velocity = velocity * (1 - 1.8f * dt);
        position = position + velocity * dt;
        position.x = clamp(position.x, 80, 1200);
        position.y = clamp(position.y, 160, 600);
        heading = lerp(heading, velocity.x * 0.1f, 0.08f);
        // The faster it goes, the faster the water moves.
        frame += dt * (6 + sqrt(velocity.dot(velocity)) * 0.03f);
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.tile(water, 0, 0, 1280, 720, 320, 320, sin(t * 0.4f) * 30, t * 40);
        c.setBlend(BlendAdd);
        c.tile(water, 0, 0, 1280, 720, 460, 460, -t * 24, t * 16, Color(1, 1, 1, 0.25f));
        c.setBlend(BlendAlpha);

        c.push();
        c.translate(position.x, position.y);
        c.rotate(heading);
        // Two frames at once, one fading into the next, look smoother than one.
        uint now = uint(frame) % ripples.length();
        uint next = (now + 1) % ripples.length();
        float part = frame - float(int(frame));
        c.image(ripples[now], -80, -208, 160, 415, fade(White, 1 - part));
        c.image(ripples[next], -80, -208, 160, 415, fade(White, part));
        c.image(ship, -46, -138 + sin(t * 2) * 2, 92, 276);
        c.pop();

        c.roundRect(30, 650, 520, 44, 12, Color(0, 0, 0, 0.5f));
        c.text(font, "Steer with the arrows, d-pad or left stick", 50, 660);
    }
}
