// A little play. Each character is a coroutine, and the lines are just written in order.

class Actor
{
    string name;
    Tween x;
    Tween jump;
    Color color;
    string line;
    uint shown = 0;

    Actor(const string &in name_, float at, const Color &in color_)
    {
        name = name_;
        x = Tween(at);
        color = color_;
    }
}

class CoroutinesScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 26);
    Font@ small = assets::font("global:freesans.sfn", 20);
    Sound@ talk = assets::sound("global:sound/move.wav");
    Sound@ hop = assets::sound("global:sound/pop.wav");
    Sound@ bell = assets::sound("global:sound/bell.wav");
    Actor@ red = Actor("Ember", -100, Color(0.95f, 0.4f, 0.3f));
    Actor@ blue = Actor("Tide", 1380, Color(0.35f, 0.6f, 1));
    Tween sun(0);
    uint lamp = 0;
    int scene = 0;

    void onEnter()
    {
        start(Coroutine(this.play));
        start(Coroutine(this.traffic));
        start(Coroutine(this.blink));
    }

    float eyes = 1;

    void blink()
    {
        while (true)
        {
            wait(random(1.5f, 4));
            eyes = 0.1f;
            wait(0.12f);
            eyes = 1;
        }
    }

    void traffic()
    {
        while (true)
        {
            lamp = 0;
            wait(3);
            lamp = 1;
            wait(1);
            lamp = 2;
            wait(3);
            lamp = 1;
            wait(1);
        }
    }

    void say(Actor@ who, const string &in text)
    {
        who.line = text;
        for (who.shown = 0; who.shown < text.length(); who.shown++)
        {
            if (who.shown % 3 == 0) talk.play(0.4f, who is red ? -0.4f : 0.4f, who is red ? 0.9f : 1.2f);
            wait(0.028f);
        }
        wait(1.3f);
        who.line = "";
    }

    void walk(Actor@ who, float to)
    {
        who.x.go(to, 1.6f, EaseSmooth);
        while (!who.x.done) nextUpdate();
    }

    void leap(Actor@ who)
    {
        hop.play(0.7f);
        who.jump = Tween(1, 0, 0.6f, EaseLinear);
        while (!who.jump.done) nextUpdate();
    }

    void play()
    {
        while (true)
        {
            scene++;
            sun.go(1, 4, EaseSmooth);
            walk(red, 420);
            say(red, "I was told there would be a game engine.");
            walk(blue, 860);
            say(blue, "You're standing in it.");
            leap(red);
            say(red, "It remembers where we were?");
            say(blue, "Every line of this is one function. It just waits.");
            start(Coroutine(this.ring));
            say(red, "And that bell?");
            say(blue, "Another one, at the same time.");
            leap(blue);
            leap(red);
            sun.go(0, 4, EaseSmooth);
            say(blue, "Same again?");
            walk(blue, 1380);
            walk(red, -100);
            wait(0.5f);
        }
    }

    void ring()
    {
        for (int i = 0; i < 3; i++)
        {
            bell.play(0.5f, 0, 1 + float(i) * 0.25f);
            wait(0.5f);
        }
    }

    void actor(Canvas@ c, Actor@ a)
    {
        float j = a.jump.value;
        float up = 4 * j * (1 - j) * 120;
        float step = a.x.done ? 0 : abs(sin(app::time() * 12)) * 8;
        float x = a.x.value, y = 560 - up - step;
        c.push();
        c.translate(x, 562);
        c.scale(1, 0.25f);
        c.circle(0, 0, 50 - up * 0.15f, Color(0, 0, 0, 0.25f));
        c.pop();
        c.roundRect(x - 44, y - 110, 88, 110, 38, a.color);
        c.circle(x - 16, y - 70, 12, White);
        c.circle(x + 16, y - 70, 12, White);
        float look = a is red ? 4.0f : -4.0f;
        c.circle(x - 16 + look, y - 70, 6 * eyes, Black);
        c.circle(x + 16 + look, y - 70, 6 * eyes, Black);
        c.text(small, a.name, x, 580, White, AlignCenter);
        if (a.line != "")
        {
            string text = a.line.substr(0, a.shown);
            float w = font.measure(a.line).x + 50;
            float left = clamp(x - w / 2, 30, 1250 - w);
            c.roundRect(left, y - 210, w, 60, 18, White);
            c.triangle(Vec2(x - 12, y - 152), Vec2(x + 12, y - 152), Vec2(x, y - 126), White, White, White);
            c.text(font, text, left + 25, y - 196, Color(0.1f, 0.1f, 0.2f));
        }
    }

    float abs(float v) { return v < 0 ? -v : v; }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        float s = sun.value;
        c.gradient(0, 0, 1280, 560, Color(0.05f + 0.3f * s, 0.07f + 0.48f * s, 0.2f + 0.65f * s), Color(0.15f + 0.7f * s, 0.1f + 0.6f * s, 0.3f + 0.35f * s));
        c.circle(200, 560 - s * 420, 60, Color(1, 0.9f, 0.5f));
        c.circle(1060, 140 + s * 520, 44, Color(0.9f, 0.92f, 1));
        c.rect(0, 540, 1280, 180, Color(0.12f + 0.15f * s, 0.2f + 0.3f * s, 0.14f + 0.1f * s));

        c.roundRect(1150, 380, 60, 160, 14, Color(0.12f, 0.12f, 0.15f));
        c.circle(1180, 410, 18, lamp == 0 ? Red : Color(0.3f, 0.1f, 0.1f));
        c.circle(1180, 460, 18, lamp == 1 ? Orange : Color(0.3f, 0.2f, 0.05f));
        c.circle(1180, 510, 18, lamp == 2 ? Green : Color(0.08f, 0.25f, 0.1f));

        actor(c, red);
        actor(c, blue);
        c.text(small, formatInt(coroutines()) + " coroutines running, time round " + formatInt(scene), 30, 676, Color(0.85f, 0.9f, 0.95f));
    }
}
