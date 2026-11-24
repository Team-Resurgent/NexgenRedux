# Guide

## Your first screen

Download the showcase for your machine, or build it, and look in its `media/showcase` folder. Each folder there is a screen.
Make a new one called `hello` with two files.

`screen.json` says how it appears in the menu:

```json
{
  "title": "Hello",
  "description": "My first screen",
  "category": "Games",
  "order": 200
}
```

`main.as` is the screen:

```cpp
class Hello : Screen
{
    Font@ font = assets::font("global:freesans.sfn", 48);
    float x = 640;

    void update(float dt)
    {
        x += input::move().x * 300 * dt;
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.circle(x, 360, 80, Blue);
        c.text(font, "Hello", x, 335, White, AlignCenter);
    }
}
```

Start the showcase with `--dev` and it reloads the screen each time you save. If there is a mistake, the screen shows the file
and the line; mend it, save, and it carries on.

## Screens

A screen is a class that implements `Screen`. The engine calls the methods it has:

| Method | When |
|---|---|
| `onEnter()` | Once, when everything the screen asked for has loaded |
| `update(dt)` | 60 times a second, whatever the display does |
| `draw(canvas)` | Once for each frame shown |
| `onSuspend()`, `onResume()` | When another screen opens on top, and closes again |
| `onExit()` | Once, before it closes |

Move in `update` and draw in `draw`. Screens form a stack: `screens::push("id")` opens one on top, `screens::pop()` goes back.
Each has its own script module, so nothing is shared by accident, and closing one gives back everything it had. Escape, Start,
or holding Back always opens the engine's menu: a script can't block it.

## Drawing

Call `design(1280, 720)` first and draw as if the screen were that size; it is fitted to the real one, with bars if the shape
differs. `designHeight(720)` fills any shape instead, and you lay out by `width`.

`push()` saves position, rotation, scale, alpha, blend, filter and clip, and `pop()` goes back. Drawing that uses one image and
one blend mode goes to the GPU together, so sort by image where you can.

## Assets

```cpp
Texture@ ship = assets::texture("asset:ship.png");   // this screen's asset folder
Sound@ hit = assets::sound("global:sound/hit.wav");  // shared by all screens
```

Ask for what a screen needs in its fields: `onEnter()` waits for those. Anything asked for later arrives in the background, and
draws as nothing until `ready`. Asking twice gives the same one. Letting go doesn't unload it: it stays warm, so going back to a
screen is instant, until the budget needs the room. A missing image draws as a checkerboard.

| What | Formats |
|---|---|
| Images | PNG, JPEG, TGA, BMP and GIF, up to 2048 pixels a side |
| Sounds | WAV, Ogg Vorbis and MP3 |
| Fonts | SSFN |

## Sound

```cpp
hit.play();                      // as an effect
hit.play(0.5f, -1, 1.2f);        // quieter, from the left, higher
Voice music = tune.playMusic(0.8f, 2);
music.stop(1.5f);                // fading out
```

A `Voice` is safe to use after the sound has ended. A screen's sounds pause while it is covered and stop when it closes.

## Input

Scripts ask for actions, not keys: `up`, `down`, `left`, `right`, `accept`, `cancel`, `fire`, `alternate` and `menu`. Players
can change every one except `menu`; the Settings demo shows how. `input::move()` gives a direction from keys, d-pad or stick.

## Coroutines and tweens

A coroutine is a function that can wait:

```cpp
void onEnter() { start(Coroutine(this.intro)); }

void intro()
{
    title.go(1, 0.5f, EaseOvershoot);
    wait(2);
    title.go(0, 0.3f);
    while (!title.done) nextUpdate();
    screens::replace("game");
}
```

A `Tween` is a number that moves by itself. There is nothing to update: read `value` when drawing. `go()` starts from wherever
it is, so changing course never jumps.

## Saving

```cpp
save::set("best", formatInt(score));
int best = int(parseInt(save::get("best", "0")));
```

## On the original Xbox

- 64 MB in all. Images and fonts may take 20 MB and sounds 6 MB by default.
- Build the disc with the cook tool, which makes images a quarter to an eighth of the size in memory and sounds a quarter,
  under the same names.
- There is no live reload: edit on a PC and then build the disc.
