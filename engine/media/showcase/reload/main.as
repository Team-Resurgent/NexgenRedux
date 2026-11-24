// Run the showcase with --dev, then change anything in this file and save it. The screen restarts by
// itself. A mistake shows as an error, and mending it carries on.

const string Message = "Change me!";
const Color Back = Color(0.1f, 0.16f, 0.2f);
const uint Petals = 9;

class ReloadScreen : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 26);
    Font@ big = assets::font("global:freesans.sfn", 60);
    Tween arrive;

    void onEnter() { arrive = Tween(0, 1, 0.7f, EaseOvershoot); }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.rect(0, 0, 1280, 720, Back);
        c.push();
        c.translate(640, 270);
        c.scale(arrive.value, arrive.value);
        c.rotate(app::time() * 20);
        for (uint i = 0; i < Petals; i++)
        {
            c.rotate(360.0f / float(Petals));
            c.circle(0, -120, 46, fade(hsv(float(i) / float(Petals), 0.6f, 1), 0.85f));
        }
        c.circle(0, 0, 60, White);
        c.pop();
        c.text(big, Message, 640, 440, White, AlignCenter);
        c.text(font, "media/showcase/reload/main.as", 640, 530, Color(0.75f, 0.85f, 0.9f), AlignCenter);
        c.text(font, app::development() ? "Watching for changes. Reloaded " + formatInt(app::reloads()) + " times." : "Start the showcase with --dev to watch for changes.", 640, 580, Yellow, AlignCenter);
        c.text(font, "Images reload too, where they are, without restarting anything.", 640, 620, Color(0.75f, 0.85f, 0.9f), AlignCenter);
    }
}
