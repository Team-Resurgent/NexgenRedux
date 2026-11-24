// How much can it draw? Every sprite shares one image, so however many there are it is one draw.

class StressScreen : Screen
{
    Texture@ sprite = assets::texture("asset:FishShip.png");
    Font@ font = assets::font("global:freesans.sfn", 24);
    array<float> x, y, vx, vy;
    uint count = 0;
    float t = 0;

    void onEnter() { add(200); }

    void add(uint more)
    {
        for (uint i = 0; i < more && count < 2000; i++)
        {
            x.insertLast(random(0, 1280));
            y.insertLast(random(0, 720));
            vx.insertLast(random(-160, 160));
            vy.insertLast(random(-160, 160));
            count++;
        }
    }

    void remove(uint less)
    {
        for (uint i = 0; i < less && count > 0; i++)
        {
            x.removeLast();
            y.removeLast();
            vx.removeLast();
            vy.removeLast();
            count--;
        }
    }

    void update(float dt)
    {
        t += dt;
        if (input::repeated("up")) add(100);
        if (input::repeated("down")) remove(100);
        for (uint i = 0; i < count; i++)
        {
            x[i] += vx[i] * dt;
            y[i] += vy[i] * dt;
            if (x[i] < 0 || x[i] > 1280) vx[i] = -vx[i];
            if (y[i] < 0 || y[i] > 720) vy[i] = -vy[i];
        }
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.05f, 0.07f, 0.12f));
        for (uint i = 0; i < count; i++)
            c.image(sprite, x[i] - 22, y[i] - 20, 44, 40);

        c.roundRect(30, 24, 420, 112, 12, Color(0, 0, 0, 0.7f));
        c.text(font, formatInt(count) + " sprites in " + formatInt(app::draws()) + " draws", 50, 36);
        c.text(font, formatInt(int(app::fps() + 0.5f)) + " frames a second", 50, 66, app::fps() > 55 ? Green : Orange);
        c.text(font, "Up and down to change", 50, 96, Gray);
    }
}
