// Breaks on purpose. The engine stops this screen, says where, and offers the way out.

class FaultyScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 30);
    array<string> faults = { "Divide by zero", "Run for ever", "Use something that isn't there", "Read past the end of a list" };
    uint chosen = 0;

    void update(float dt)
    {
        if (input::repeated("down")) chosen = (chosen + 1) % faults.length();
        if (input::repeated("up")) chosen = (chosen + faults.length() - 1) % faults.length();
        if (input::pressed("accept"))
            breakIt(chosen);
    }

    void breakIt(uint how)
    {
        int zero = 0;
        array<int> list;
        Texture@ nothing;
        if (how == 0)
            zero = 1 / zero;
        else if (how == 1)
        {
            while (true)
                zero++;
        }
        else if (how == 2)
            zero = int(nothing.width);
        else
            zero = list[10];
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Color(0.1f, 0.06f, 0.07f));
        c.text(font, "Choose a way to break this screen", 640, 120, White, AlignCenter);
        for (uint i = 0; i < faults.length(); i++)
        {
            float y = 220 + float(i) * 80;
            c.roundRect(390, y, 500, 62, 14, i == chosen ? Red : Color(0.2f, 0.12f, 0.14f));
            c.text(font, faults[i], 640, y + 13, White, AlignCenter);
        }
        c.text(font, "The app carries on whatever happens here", 640, 600, Gray, AlignCenter);
    }
}
