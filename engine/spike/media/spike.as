float angle = 0;
float time = 0;

void init()
{
    print("hello from AngelScript");
    app::setClear(0.1f, 0.15f, 0.25f);
}

void update(float dt)
{
    time += dt;
    angle += dt;
    Vec2 offset = Vec2(sin(time), cos(time)) * 0.1f;
    app::setTriangle(offset, angle, 1.0f + 0.1f * sin(time * 2));
}

// Run once at start-up to check that a bad script can't take the app down.
void endless()
{
    while (true)
    {
    }
}

void divideByZero()
{
    int zero = 0;
    int broken = 1 / zero;
    print("not reached " + broken);
}
