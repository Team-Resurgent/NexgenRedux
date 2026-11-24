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
    Font@ font = Assets::font("global:freesans.sfn", 48);
    float x = 640;

    void update(float dt)
    {
        x += Input::move().x * 300 * dt;
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

Move in `update` and draw in `draw`. Screens form a stack: `Screens::push("id")` opens one on top, `Screens::pop()` goes back.
Each has its own script module, so nothing is shared by accident, and closing one gives back everything it had. Escape, Start,
or holding Back always opens the engine's menu: a script can't block it.

## Drawing

Call `design(1280, 720)` first and draw as if the screen were that size; it is fitted to the real one, with bars if the shape
differs. `designHeight(720)` fills any shape instead, and you lay out by `width`.

`push()` saves position, rotation, scale, alpha, blend, filter and clip, and `pop()` goes back. Drawing that uses one image and
one blend mode goes to the GPU together, so sort by image where you can.

## Scenes

`Canvas` draws whatever you tell it to each frame. For things that move, collide and come and go, let the engine keep them:

```cpp
Entity@ rock = Scene::spawn(Vec2(640, 100));
rock.image(texture);
rock.circle(30, 1);              // a collider in group 1
rock.velocity = Vec2(0, 80);

void draw(Canvas@ c)
{
    c.design(1280, 720);
    Scene::draw(c);
}
```

A handle is always safe: once an entity has been destroyed its `alive` is false, and nothing done to it has any effect. One can be
attached to another and moves with it. A behaviour is a script on an entity:

```cpp
class Rock : Behaviour
{
    int health = 3;
    void update(float dt) { entity.rotation += 40 * dt; }
    void onHit(Entity@ other)
    {
        other.destroy();
        if (--health == 0) entity.destroy();
    }
}

rock.add(Rock());
```

`onHit` is called when its collider begins to touch one in a group that it `hits`.

### Scene files

What things are like is better kept in a file. `Scene::load("asset:scene.json")` reads one:

```json
{
  "groups": ["player", "rock", "shot"],
  "particles": {
    "dust": { "life": [0.3, 0.8], "speed": [20, 120], "size": [20, 4], "color": "#c0a080", "blend": "alpha" }
  },
  "prefabs": {
    "rock": { "image": "asset:rock.png", "circle": 30, "group": "rock", "hits": ["shot"], "spin": 40, "behaviour": "Rock" },
    "boulder": { "base": "rock", "scale": 3 }
  },
  "entities": [ { "prefab": "boulder", "position": [300, 200] } ]
}
```

and then `Scene::spawn("rock", position)` makes one. A name that means nothing is an error with its line. Files are reloaded with
`--dev`, like scripts.

| In a prefab | What it does |
|---|---|
| `name`, `position`, `rotation`, `scale`, `layer`, `hidden` | Where it is. A position is from its parent |
| `image`, `size`, `sheet`, `frame`, `flip` | Looks like an image; a sheet is `[columns, rows]` |
| `rect`, `disc`, `text` with `font` and `fontSize` | Looks like a shape or some text |
| `color`, `blend` | `"#rrggbb"`, `"#rrggbbaa"` or `[r, g, b, a]`; `alpha`, `add` or `none` |
| `play`, `loop` | Frames: `[first, last, a second]` |
| `velocity`, `spin`, `drag`, `gravity`, `bounce`, `solid` | How it moves |
| `mass`, `friction` | Makes it a rigid body |
| `circle`, `box`, `group`, `hits`, `wall` | Its collider |
| `lifetime` | Seconds until it is destroyed |
| `emitter`, `rate`, `burst` | Particles: steadily, and at once |
| `behaviour` | A class in the script, or two |
| `base`, `children` | Start from another prefab; parts that move with it |

### Maps

A map is rows of letters, and a legend says what each is: a tile's number, or a prefab to spawn there.

```json
"map": {
  "tile": 32, "solid": [1], "platforms": [2],
  "colors": { "1": "#5b3a24", "2": "#a8763e" },
  "legend": { "#": 1, "=": 2, "o": "coin", "H": "hero" },
  "rows": [
    "      o   ",
    "     ===  ",
    " H        ",
    "##########"
  ]
}
```

With `"image"` and `"sheet"` it is drawn from a picture instead of colours.

### Physics

There are two kinds. A **solid** body is stopped by walls and tiles and never turns: use it for characters, with `onGround`,
`onWall` and `onCeiling`. A body with **mass** is rigid: it pushes, stacks, bounces and turns. `Scene::join` pins two together, for
chains, hinges and wheels.

## Assets

```cpp
Texture@ ship = Assets::texture("asset:ship.png");   // this screen's asset folder
Sound@ hit = Assets::sound("global:sound/hit.wav");  // shared by all screens
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
can change every one except `menu`; the Settings demo shows how. `Input::move()` gives a direction from keys, d-pad or stick.

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
    Screens::replace("game");
}
```

A `Tween` is a number that moves by itself. There is nothing to update: read `value` when drawing. `go()` starts from wherever
it is, so changing course never jumps.

## Saving

```cpp
Save::set("best", formatInt(score));
int best = int(parseInt(Save::get("best", "0")));
```

## On the original Xbox

- 64 MB in all. Images and fonts may take 20 MB and sounds 6 MB by default.
- A scene may have 4096 entities and 1500 particles at once; elsewhere 16384 and 8000.
- Build the disc with the cook tool, which makes images a quarter to an eighth of the size in memory and sounds a quarter,
  under the same names.
- There is no live reload: edit on a PC and then build the disc.
