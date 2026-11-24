// Getting the comet past your rival scores three; every star it passes through scores one for whoever
// hit it last. A moving bat bends its path, and holding fire charges a smash. First to fifteen.

class Star
{
    Vec2 at;
    float drift;
    float born = 0;
}

class RallyScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 24);
    Font@ big = assets::font("global:freesans.sfn", 60);
    Sound@ hit = assets::sound("global:sound/hit.wav");
    Sound@ wall = assets::sound("global:sound/wall.wav");
    Sound@ coin = assets::sound("global:sound/coin.wav");
    Sound@ boom = assets::sound("global:sound/boom.wav");
    Sound@ whoosh = assets::sound("global:sound/whoosh.wav");
    Sound@ win = assets::sound("global:sound/win.wav");
    Sound@ lose = assets::sound("global:sound/lose.wav");

    Vec2 ball, speed;
    float spin = 0;
    float me = 360, rival = 360, meSpeed = 0, rivalSpeed = 0;
    float charge = 0;
    int mine = 0, theirs = 0;
    bool lastMine = true;
    bool playing = false, over = false;
    int rallyLength = 0, bestRally = 0;
    float shake = 0;
    string banner;
    Tween bannerIn;
    array<Vec2> trail(24);
    uint at = 0;
    array<Star@> stars;
    float aim = 0;

    void onEnter()
    {
        bestRally = int(parseInt(save::get("rally.best", "0")));
        start(Coroutine(this.serve));
        start(Coroutine(this.sky));
    }

    void sky()
    {
        while (true)
        {
            wait(2.2f);
            if (playing && stars.length() < 4)
            {
                Star s;
                s.at = Vec2(random(420, 860), random(140, 620));
                s.drift = random(-40, 40);
                stars.insertLast(s);
            }
        }
    }

    void say(const string &in text)
    {
        banner = text;
        bannerIn = Tween(0, 1, 0.4f, EaseOvershoot);
    }

    void serve()
    {
        playing = false;
        ball = Vec2(640, 360);
        for (uint i = 0; i < trail.length(); i++) trail[i] = ball;
        spin = 0;
        rallyLength = 0;
        for (int i = 3; i > 0; i--)
        {
            say(formatInt(i));
            wall.play(0.6f, 0, i == 1 ? 1.5f : 1.0f);
            wait(0.5f);
        }
        banner = "";
        float side = lastMine ? 1.0f : -1.0f;
        speed = Vec2(side * 430, random(-200, 200));
        playing = true;
    }

    void finish()
    {
        over = true;
        stars.resize(0);
        say(mine > theirs ? "You win!" : "Rival wins");
        (mine > theirs ? win : lose).play(0.9f);
        wait(1);
        while (!input::pressed("accept")) nextUpdate();
        mine = theirs = 0;
        over = false;
        start(Coroutine(this.serve));
    }

    void point(bool forMe, int worth)
    {
        if (forMe) mine += worth;
        else theirs += worth;
    }

    void update(float dt)
    {
        float move = input::move().y;
        meSpeed = lerp(meSpeed, move * 640, 0.25f);
        me = clamp(me + meSpeed * dt, 120, 600);
        charge = input::held("fire") ? clamp(charge + dt * 1.2f, 0, 1) : clamp(charge - dt * 2, 0, 1);

        // The rival watches the comet, not perfectly, and drifts back to the middle.
        aim = lerp(aim, speed.x > 0 ? ball.y + speed.y * 0.10f : 360.0f, 0.06f);
        float want = clamp((aim - rival) * 5, -430, 430);
        rivalSpeed = lerp(rivalSpeed, want, 0.2f);
        rival = clamp(rival + rivalSpeed * dt, 120, 600);

        shake = clamp(shake - dt * 3, 0, 1);
        for (uint i = 0; i < stars.length(); i++)
        {
            stars[i].born += dt;
            stars[i].at.y += stars[i].drift * dt;
            if (stars[i].at.y < 110 || stars[i].at.y > 640) stars[i].drift = -stars[i].drift;
        }
        if (!playing || over) return;

        trail[at] = ball;
        at = (at + 1) % trail.length();

        speed.y += spin * dt;
        spin *= 0.985f;
        ball = ball + speed * dt;
        float pan = (ball.x - 640) / 640;

        if ((ball.y < 96 && speed.y < 0) || (ball.y > 644 && speed.y > 0))
        {
            speed.y = -speed.y;
            spin = -spin * 0.5f;
            wall.play(0.5f, pan);
        }
        if (speed.x < 0 && ball.x < 96 && ball.x > 60 && near(ball.y, me)) strike(true, pan);
        if (speed.x > 0 && ball.x > 1184 && ball.x < 1220 && near(ball.y, rival)) strike(false, pan);

        for (uint i = 0; i < stars.length();)
        {
            Vec2 d = stars[i].at - ball;
            if (d.x * d.x + d.y * d.y < 34 * 34)
            {
                point(lastMine, 1);
                coin.play(0.5f, pan);
                stars.removeAt(i);
                checkOver();
            }
            else
                i++;
        }

        if (ball.x < -20 || ball.x > 1300)
        {
            bool forMe = ball.x > 0;
            point(forMe, 3);
            lastMine = forMe;
            shake = 1;
            boom.play(0.8f, pan);
            playing = false;
            if (!checkOver()) start(Coroutine(this.serve));
        }
    }

    bool checkOver()
    {
        if (over || (mine < 15 && theirs < 15)) return over;
        playing = false;
        start(Coroutine(this.finish));
        over = true;
        return true;
    }

    bool near(float a, float b) { return a > b - 68 && a < b + 68; }

    void strike(bool byMe, float pan)
    {
        float bat = byMe ? me : rival;
        float batSpeed = byMe ? meSpeed : rivalSpeed;
        float power = byMe ? charge : 0.0f;
        float pace = clamp(sqrt(speed.x * speed.x + speed.y * speed.y) + 22, 430, 900) + power * 320;
        float angle = (ball.y - bat) / 68 * 0.9f;
        speed = Vec2(cos(angle) * pace * (byMe ? 1.0f : -1.0f), sin(angle) * pace);
        spin = -batSpeed * 1.4f;
        lastMine = byMe;
        rallyLength++;
        if (rallyLength > bestRally)
        {
            bestRally = rallyLength;
            save::set("rally.best", formatInt(bestRally));
        }
        hit.play(0.7f, pan, 0.9f + pace / 1800);
        if (power > 0.5f) whoosh.play(0.8f, pan);
        if (byMe) charge = 0;
        shake = 0.15f + power * 0.4f;
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.gradient(0, 0, 1280, 720, Color(0.05f, 0.03f, 0.13f), Color(0.02f, 0.05f, 0.1f));
        c.push();
        c.translate(random(-1, 1) * shake * 12, random(-1, 1) * shake * 12);
        c.rectLine(20, 80, 1240, 580, 3, Color(0.25f, 0.3f, 0.5f));
        for (int i = 0; i < 15; i++) c.rect(638, 92 + float(i) * 38, 4, 20, Color(0.2f, 0.25f, 0.42f));

        for (uint i = 0; i < stars.length(); i++)
        {
            Star@ s = stars[i];
            float size = clamp(s.born * 3, 0, 1) * (14 + sin(app::time() * 4 + float(i)) * 2);
            c.push();
            c.translate(s.at.x, s.at.y);
            c.rotate(app::time() * 60);
            c.circle(0, 0, size * 1.6f, Color(1, 0.9f, 0.4f, 0.15f));
            for (int k = 0; k < 4; k++)
            {
                c.rotate(90);
                c.triangle(Vec2(-size * 0.35f, 0), Vec2(size * 0.35f, 0), Vec2(0, -size), Yellow, Yellow, White);
            }
            c.pop();
        }

        c.setBlend(BlendAdd);
        for (uint i = 0; i < trail.length(); i++)
        {
            float p = float(i) / float(trail.length());
            Vec2 t = trail[(at + i) % trail.length()];
            c.circle(t.x, t.y, 4 + p * 12, fade(lastMine ? Color(0.3f, 0.7f, 1) : Color(1, 0.45f, 0.35f), p * 0.35f));
        }
        c.setBlend(BlendAlpha);
        if (!over) c.circle(ball.x, ball.y, 14, White);

        c.roundRect(64, me - 60, 22, 120, 11, Color(0.35f, 0.7f, 1));
        if (charge > 0) c.roundRect(58, me - 60 * charge, 6, 120 * charge, 3, Yellow);
        c.roundRect(1194, rival - 60, 22, 120, 11, Color(1, 0.45f, 0.35f));
        c.pop();

        c.text(big, formatInt(mine), 560, 4, Color(0.5f, 0.8f, 1), AlignCenter);
        c.text(big, formatInt(theirs), 720, 4, Color(1, 0.6f, 0.5f), AlignCenter);
        c.text(font, "Rally " + formatInt(rallyLength) + "   best " + formatInt(bestRally), 40, 30, Gray);
        c.text(font, over ? "Accept to play again" : "Up and down, hold fire to smash", 1240, 30, over ? Yellow : Gray, AlignRight);
        if (banner != "")
        {
            c.push();
            c.translate(640, 250);
            c.scale(bannerIn.value, bannerIn.value);
            c.text(big, banner, 0, -40, White, AlignCenter);
            c.pop();
        }
    }
}
