// Sky Patrol: a complete little game. Drones drift in on curves, some fire back, and clearing a wave
// brings a faster one. All the art is drawn from shapes.

class Drone
{
    float x, y, phase, swing, speed, reload;
    int health;
    int kind;
    float hurt;
    bool alive = false;
}

class Shot
{
    float x, y, vx, vy;
    bool mine;
    bool alive = false;
}

class Spark
{
    float x, y, vx, vy, life;
    Color color;
}

class PatrolScreen : Screen
{
    Texture@ stars = assets::texture("asset:stars.png");
    Font@ font = assets::font("global:freesans.sfn", 28);
    Font@ big = assets::font("global:freesans.sfn", 70);

    // Fixed pools: nothing is made or thrown away while playing.
    array<Drone> drones(24);
    array<Shot> shots(96);
    array<Spark> sparks(200);
    uint nextSpark = 0;

    int state = 0;            // 0 title, 1 playing, 2 over
    float t = 0, shake = 0, banner = 0;
    int score = 0, best = 0, lives = 3, wave = 0, waiting = 0;
    float spawnIn = 0, reload = 0, safe = 0;
    float px = 640, py = 620, tilt = 0;

    void start()
    {
        state = 1;
        score = 0;
        lives = 3;
        wave = 0;
        px = 640;
        py = 620;
        safe = 2;
        for (uint i = 0; i < drones.length(); i++) drones[i].alive = false;
        for (uint i = 0; i < shots.length(); i++) shots[i].alive = false;
        nextWave();
    }

    void nextWave()
    {
        wave++;
        waiting = 5 + wave * 2;
        spawnIn = 1.2f;
        banner = 2;
    }

    void burst(float x, float y, const Color &in color, int count)
    {
        for (int i = 0; i < count; i++)
        {
            Spark@ s = sparks[nextSpark];
            nextSpark = (nextSpark + 1) % sparks.length();
            float a = random(0, 6.28318f);
            float v = random(40, 300);
            s.x = x;
            s.y = y;
            s.vx = cos(a) * v;
            s.vy = sin(a) * v;
            s.life = random(0.4f, 1);
            s.color = color;
        }
    }

    void shoot(float x, float y, float vx, float vy, bool mine)
    {
        for (uint i = 0; i < shots.length(); i++)
        {
            Shot@ s = shots[i];
            if (!s.alive)
            {
                s.x = x;
                s.y = y;
                s.vx = vx;
                s.vy = vy;
                s.mine = mine;
                s.alive = true;
                return;
            }
        }
    }

    void spawn()
    {
        for (uint i = 0; i < drones.length(); i++)
        {
            Drone@ d = drones[i];
            if (!d.alive)
            {
                d.kind = wave > 1 && random() < 0.3f ? 1 : 0;
                d.x = random(160, 1120);
                d.y = -50;
                d.phase = random(0, 6);
                d.swing = random(40, 150);
                d.speed = random(70, 110) + float(wave) * 8;
                d.health = d.kind == 1 ? 3 : 1;
                d.reload = random(1, 2.5f);
                d.hurt = 0;
                d.alive = true;
                waiting--;
                return;
            }
        }
    }

    bool hit(float ax, float ay, float bx, float by, float reach)
    {
        float dx = ax - bx, dy = ay - by;
        return dx * dx + dy * dy < reach * reach;
    }

    void update(float dt)
    {
        t += dt;
        shake = clamp(shake - dt * 2, 0, 1);
        banner -= dt;
        for (uint i = 0; i < sparks.length(); i++)
        {
            Spark@ s = sparks[i];
            s.life -= dt;
            s.x += s.vx * dt;
            s.y += s.vy * dt;
            s.vx *= 1 - 2 * dt;
            s.vy *= 1 - 2 * dt;
        }
        if (state != 1)
        {
            if (input::pressed("accept"))
                start();
            return;
        }
        player(dt);
        fleet(dt);
        bullets(dt);
    }

    void player(float dt)
    {
        Vec2 move = input::move();
        px = clamp(px + move.x * 420 * dt, 50, 1230);
        py = clamp(py + move.y * 420 * dt, 120, 680);
        tilt = lerp(tilt, move.x, 0.15f);
        safe -= dt;
        reload -= dt;
        if (input::held("fire") && reload <= 0)
        {
            reload = 0.14f;
            shoot(px - 18, py - 20, 0, -760, true);
            shoot(px + 18, py - 20, 0, -760, true);
        }
    }

    void fleet(float dt)
    {
        spawnIn -= dt;
        if (waiting > 0 && spawnIn <= 0)
        {
            spawn();
            spawnIn = clamp(0.9f - float(wave) * 0.05f, 0.25f, 1);
        }
        bool any = false;
        for (uint i = 0; i < drones.length(); i++)
        {
            Drone@ d = drones[i];
            if (!d.alive)
                continue;
            any = true;
            d.phase += dt;
            d.hurt = clamp(d.hurt - dt * 5, 0, 1);
            d.y += d.speed * dt;
            d.x = clamp(d.x + cos(d.phase * 1.6f) * d.swing * dt, 30, 1250);
            d.reload -= dt;
            if (d.kind == 1 && d.reload <= 0 && d.y < 480)
            {
                d.reload = 1.8f;
                float dx = px - d.x, dy = py - d.y;
                float l = sqrt(dx * dx + dy * dy) + 1;
                shoot(d.x, d.y + 20, dx / l * 260, dy / l * 260, false);
            }
            if (d.y > 780)
                d.alive = false;
            else if (safe <= 0 && hit(d.x, d.y, px, py, 46))
            {
                d.alive = false;
                burst(d.x, d.y, Orange, 24);
                loseLife();
            }
        }
        if (!any && waiting == 0)
            nextWave();
    }

    void bullets(float dt)
    {
        for (uint i = 0; i < shots.length(); i++)
        {
            Shot@ s = shots[i];
            if (!s.alive)
                continue;
            s.x += s.vx * dt;
            s.y += s.vy * dt;
            if (s.y < -20 || s.y > 740 || s.x < -20 || s.x > 1300)
            {
                s.alive = false;
                continue;
            }
            if (!s.mine)
            {
                if (safe <= 0 && hit(s.x, s.y, px, py, 24))
                {
                    s.alive = false;
                    loseLife();
                }
                continue;
            }
            for (uint j = 0; j < drones.length(); j++)
            {
                Drone@ d = drones[j];
                if (d.alive && hit(s.x, s.y, d.x, d.y, d.kind == 1 ? 34 : 26))
                {
                    s.alive = false;
                    d.health--;
                    d.hurt = 1;
                    if (d.health <= 0)
                    {
                        d.alive = false;
                        score += d.kind == 1 ? 50 : 10;
                        burst(d.x, d.y, d.kind == 1 ? Purple : Green, 18);
                    }
                    break;
                }
            }
        }
    }

    void loseLife()
    {
        lives--;
        safe = 2;
        shake = 1;
        burst(px, py, Yellow, 40);
        if (lives <= 0)
        {
            state = 2;
            best = score > best ? score : best;
        }
    }

    void drawShip(Canvas@ c, float x, float y)
    {
        c.push();
        c.translate(x, y);
        c.scale(1 - abs(tilt) * 0.2f, 1);
        c.setBlend(BlendAdd);
        float f = random(18, 34);
        c.triangle(Vec2(-9, 22), Vec2(9, 22), Vec2(0, 22 + f), Orange, Orange, fade(Red, 0));
        c.setBlend(BlendAlpha);
        Color hull(0.75f, 0.82f, 0.92f);
        Color dark(0.35f, 0.45f, 0.62f);
        c.triangle(Vec2(0, -34), Vec2(40, 24), Vec2(0, 12), hull, dark, hull);
        c.triangle(Vec2(0, -34), Vec2(-40, 24), Vec2(0, 12), hull, dark, hull);
        c.triangle(Vec2(0, -34), Vec2(11, 22), Vec2(-11, 22), White, hull, hull);
        c.circle(0, -2, 6, Blue);
        c.pop();
    }

    float abs(float v) { return v < 0 ? -v : v; }

    void drawDrone(Canvas@ c, const Drone@ d)
    {
        c.push();
        c.translate(d.x, d.y);
        c.rotate(sin(d.phase * 1.6f) * 14);
        Color body = d.kind == 1 ? Purple : Green;
        if (d.hurt > 0)
            body = White;
        float s = d.kind == 1 ? 1.3f : 1.0f;
        c.scale(s, s);
        c.ring(-22, 0, 12, 4, fade(body, 0.7f));
        c.ring(22, 0, 12, 4, fade(body, 0.7f));
        c.rect(-22, -3, 44, 6, fade(body, 0.7f));
        c.circle(0, 0, 14, body);
        c.circle(0, 4, 6, Color(0.1f, 0.1f, 0.15f));
        c.circle(sin(d.phase * 3) * 2, 5, 3, Red);
        c.pop();
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.02f, 0.03f, 0.1f), Color(0.08f, 0.05f, 0.16f));
        c.tile(stars, 0, 0, 1280, 720, 640, 640, 0, t * 60);
        c.tile(stars, 0, 0, 1280, 720, 1000, 1000, 100, t * 140, Color(1, 1, 1, 0.6f));

        c.push();
        c.translate(random(-12, 12) * shake, random(-12, 12) * shake);
        for (uint i = 0; i < drones.length(); i++)
        {
            if (drones[i].alive)
                drawDrone(c, drones[i]);
        }
        if (state == 1 && (safe <= 0 || int(t * 12) % 2 == 0))
            drawShip(c, px, py);

        c.setBlend(BlendAdd);
        for (uint i = 0; i < shots.length(); i++)
        {
            Shot@ s = shots[i];
            if (!s.alive)
                continue;
            if (s.mine)
                c.roundRect(s.x - 3, s.y - 14, 6, 28, 3, Color(0.4f, 0.8f, 1));
            else
                c.circle(s.x, s.y, 8, Color(1, 0.4f, 0.5f));
        }
        for (uint i = 0; i < sparks.length(); i++)
        {
            Spark@ s = sparks[i];
            if (s.life > 0)
                c.circle(s.x, s.y, 2 + s.life * 4, fade(s.color, s.life));
        }
        c.pop();

        c.text(font, "Score " + formatInt(score), 30, 20);
        c.text(font, "Best " + formatInt(best), 640, 20, Gray, AlignCenter);
        for (int i = 0; i < lives; i++)
            c.triangle(Vec2(1236 - i * 34, 22), Vec2(1248 - i * 34, 48), Vec2(1224 - i * 34, 48), White, Blue, Blue);

        string accept = input::usingPad() ? "A" : "Enter";
        if (state == 1 && banner > 0)
            c.text(big, "Wave " + formatInt(wave), 640, 250, fade(White, clamp(banner, 0, 1)), AlignCenter);
        if (state == 0)
        {
            c.text(big, "SKY PATROL", 640, 220, hsv(t * 0.1f, 0.4f, 1), AlignCenter);
            c.text(font, "Arrows or stick to move, hold " + (input::usingPad() ? "A" : "Space") + " to fire", 640, 340, Color(0.8f, 0.85f, 0.95f), AlignCenter);
            c.text(font, accept + " to start", 640, 420, fade(Yellow, 0.6f + 0.4f * sin(t * 4)), AlignCenter);
            drawShip(c, 640, 560 + sin(t * 2) * 10);
        }
        if (state == 2)
        {
            c.rect(0, 0, 1280, 720, Color(0, 0, 0, 0.5f));
            c.text(big, "Game over", 640, 250, Red, AlignCenter);
            c.text(font, "You reached wave " + formatInt(wave) + " with " + formatInt(score) + " points", 640, 350, White, AlignCenter);
            c.text(font, accept + " to play again", 640, 420, fade(Yellow, 0.6f + 0.4f * sin(t * 4)), AlignCenter);
        }
    }
}
