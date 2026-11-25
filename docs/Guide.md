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
        x += Input::move.x * 300 * dt;
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

## Three dimensions

The same entities can be in space: x right, y up, z towards you, in metres. `Space::spawn` puts one there, and behaviours, parents,
groups, `onHit` and `lifetime` work as they do on the plane.

```cpp
Model@ robot = Assets::model("global:model/robot.glb");
Entity@ hero;

void onEnter()
{
    Space::ground(0);
    Space::sky(Color(0.2f, 0.45f, 0.85f), Color(0.7f, 0.85f, 0.95f));
    @hero = Space::spawn(Vec3(0, 0, 0));
    hero.model(robot);
    hero.animate("idle");
}

void update(float dt)
{
    Space::camera(hero.place + Vec3(0, 3, 6), hero.place + Vec3(0, 1, 0));
}

void draw(Canvas@ c)
{
    c.designHeight(720);
    Space::draw(c);                        // all of the screen, or part of it
    c.text(font, "In front of it", 40, 40);
}
```

| To look like | Use |
|---|---|
| A glTF model | `model`, and `animate("walk")` to play one of its animations, changing smoothly from the last |
| A simple shape | `shape(SolidBox, Red)`: a box, ball, cylinder, cone, ring, capsule or sheet |
| A mesh of your own | `mesh`, with a `Mesh` built from boxes, balls and triangles |
| A soft light | `glow`. `light` is what lights other things: the four strongest reach each thing |

A model is a `.glb`, or a `.gltf` with everything inside it, with up to 64 bones. The engine makes two simpler versions as it loads,
and draws those when it is small on the screen. What can't be seen isn't drawn.

`Space::sun`, `ambient`, `sky`, `fog` and `shadows` set the scene. Real shadows need a GPU with shaders; the original Xbox draws soft
patches instead.

### A world to walk in

```cpp
Space::land(21, 192, 1.5f, 34, 0.45f, true);   // a seed, squares, metres to each, height, roughness, an island
Space::flatten(0, 0, 40, 3);                   // somewhere to build
Space::water(0, Color(0.16f, 0.42f, 0.55f, 0.8f));

hero.capsule(0.35f, 1.6f, 1);
hero.colliderAt = Vec3(0, 0.8f, 0);            // its place is at its feet
hero.walker = true;
```

A **walker** stays upright, slides along walls, climbs small steps and pushes what has mass. Set `motion` each update, keeping
its y so that it falls. Something with **mass** is a rigid body, as on the plane, with a `sphere`, `cube` or `capsule` collider; a
collider with `wall` stops both. `Space::raycast` says what is on a line, including the land.

### A world that streams

`Space::endlessLand` has no edge. Regions of 32 squares are made in the background as the camera comes near and go as it leaves. Ask
which have come, fill them, and let what is in them go with them:

```cpp
int x, z;
bool arrived;
while (Space::nextRegion(x, z, arrived))
    if (arrived)
        Space::load("asset:region_" + x + "_" + z + ".json", Vec3(), true);   // true: streamed
```

A region may come from a file, or be made up from its numbers so that it is the same each time. A file lists entities:

```json
{ "entities": [
  { "model": "global:model/oak.glb", "place": [4, 0, 2], "onLand": true, "angles": 90, "size": 1.2, "capsule": [0.3, 4], "wall": true },
  { "shape": "box", "color": "#b08048", "place": [3, 0.5, 1], "cube": [1, 1, 1], "mass": 5 },
  { "glow": 2, "light": [1, 0.7, 0.3], "range": 8, "place": [0, 3, 0] }
] }
```

| In such a file | What it does |
|---|---|
| `name`, `place`, `onLand`, `angles`, `size` | Where it is; `onLand` measures up from the ground |
| `model`, `shape`, `glow`, `skin`, `color`, `shine`, `glowing`, `twoSided`, `shadow` | What it looks like |
| `light`, `range` | Lights other things |
| `sphere`, `cube`, `capsule`, `colliderAt`, `group`, `hits`, `wall` | Its collider |
| `mass`, `friction`, `bounce`, `gravity`, `drag`, `walker` | How it moves |
| `behaviour` | A class in the script |

### Particles and sounds in space

Particles are described in a scene file, as on the plane, with lengths in metres. `spread` is in degrees: 90 is a cone, and 360 goes
every way. They face the camera, are lit by nothing, and hide behind things.

```cpp
Scene::load("asset:scene.json");                        // "particles": { "flame": {...}, "sparks": {...} }
fire.emit("flame", 60);                                 // steadily, upwards from it as it is turned
Space::burst("sparks", place, 40);                      // at once
fire.sound(crackle, 0.9f, 18);                          // heard from where it is, within 18 metres
boom.playAt(place, 1, 60);                              // once, from a place
```

What is heard fades with distance from the camera, and moves from one side to the other.

### Shapes that melt

`Space::field` adds one of up to 24 shapes that are drawn by marching along rays, so that they join softly, cut each other and
reflect. They mix with meshes. Clear them and add them again each update to move them. The original Xbox makes triangles of them.

## Shaders and bloom

```cpp
Effect@ tube = Assets::effect("global:effect/tube.fx");
Effect@ flag = Assets::effect("asset:flag.fx");

cloth.effect(flag);                // on a thing in space
flag.set(0, 1);                    // values[0].x

void draw(Canvas@ c)
{
    Space::draw(c);
    c.bloom(0.7f, 0.7f);           // light spills from what is brighter than 0.7
    c.effect(tube);                // over all that has been drawn so far
    c.text(font, "Not through it", 40, 40);
}
```

An effect is one file, for every GPU that has shaders: Direct3D 11 and OpenGL now. Where there are none, as on the original Xbox,
`bloom` and `effect` do nothing and give false, and things are drawn as they are, so a game need not ask. A mistake in one is in the
log with its line, and it is left out.

Over the picture:

```hlsl
float4 effect(float2 uv, float4 color)
{
    float grey = dot(color.rgb, float3(0.3, 0.6, 0.1));
    return float4(lerp(color.rgb, float3(grey, grey, grey), values[0].x), 1.0);
}
```

On a thing:

```hlsl
#define MOVES                                     // only if it has move()
float3 move(float3 place, float3 normal, float2 uv)
{
    return place + normal * sin(time * 3.0 + place.y * 4.0) * values[0].x;
}

float4 paint(float4 color, float2 uv, float3 place, float3 normal, inout float glow)
{
    glow = max(glow, frac(uv.y * 8.0 - time));    // 1 is bright whatever the light
    return color;                                 // before it is lit
}
```

| There is | Which is |
|---|---|
| `picture(uv)` | The picture so far, anywhere; `uv` is 0 to 1 from the top left |
| `size`, `time` | Of the picture in pixels, and seconds |
| `values[0]` to `values[3]` | Sixteen numbers, set with `Effect::set` |

It is written as HLSL is, and every number with a point in it. `float2` to `float4`, `lerp`, `frac`, `saturate` and `fmod` have their
OpenGL names put in for you; keep to what both have. Don't name anything `out`, `in`, `sample` or `texture`.

## Assets

```cpp
Texture@ ship = Assets::texture("asset:ship.png");   // this screen's asset folder
Sound@ hit = Assets::sound("global:sound/hit.wav");  // shared by all screens
```

A name can be an address too: `Assets::texture("https://...")` fetches a picture from the web.

Ask for what a screen needs in its fields: `onEnter()` waits for those. Anything asked for later arrives in the background, and
draws as nothing until `ready`. Asking twice gives the same one. Letting go doesn't unload it: it stays warm, so going back to a
screen is instant, until the budget needs the room. A missing image draws as a checkerboard.

| What | Formats |
|---|---|
| Images | PNG, JPEG, TGA, BMP and GIF, up to 2048 pixels a side |
| Sounds | WAV, Ogg Vorbis and MP3 |
| Fonts | SSFN |
| Models | glTF 2.0 (`.glb`) |

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
can change every one except `menu`; the Settings demo shows how. `Input::move` gives a direction from keys, d-pad or stick.

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

## Networking

Nothing here waits: ask, and look each update at what has come. Who sends what is up to you, so never trust what arrives.

### Playing together

A `Room` is a game on the local network. One player hosts; others find it by the game's name and join.

```cpp
Room@ room = Room();

room.host("my game", "Sam");                  // or
room.find("my game");                         // ...and then, when room.found > 0,
room.join("my game", 0, "Alex");

Message@ m = Message();
m.writeInt(1);
m.writeVec2(position);
room.send(m, false);                          // false: quickly; true: for sure, and in order

for (Message@ m = room.receive(); m !is null; @m = room.receive())
    if (m.readInt() == 1)
        where[m.from] = m.readVec2();         // m.whole says whether it was all there
```

`room.state` is `RoomIn` once you are in, and `RoomLost` if the host goes. `arrived()` and `departed()` say who has come and gone. Use
quick messages for what is out of date in a moment, such as where things are, and sure ones for what happened.

### The web

```cpp
Request@ request = Net::get("https://example.com/scores");

if (request.done && !request.failed)
    show(request.status, request.text);
```

Certificates are checked, against those in `media/global/certificates.pem`: a server that can't be trusted is refused, and `error`
says why. An answer may be up to 4 MB. `Net::post` sends something.

### Anything else

`Connection` is a stream of text over TCP, to a server or from those who connect to you; `Datagrams` sends short texts over UDP.

## What only an Xbox can do

`Xbox::available` is true on an original Xbox. Its temperatures, fan, light, tray, drives and files are there, as are ways out:

```cpp
if (Xbox::available)
{
    string cpu = formatInt(Xbox::cpuTemperature) + " C";
    array<string>@ games = Xbox::folders("E:\\Games");
    Xbox::launch("E:\\Games\\Thing\\default.xbe");   // or toDashboard(), toFirmware(), restart(), powerOff()
}
```

`Xbox::busRead` and `busWrite` reach anything on the SMBus but the console's settings, where a mistake can't be undone. Elsewhere
readings are -1 or empty and the rest do nothing.

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
- Three dimensions use the fixed pipeline: lights are worked out at corners, so give a floor plenty of squares. Ray-marched shapes
  become triangles. There are no shaders, so no bloom or effects.
- There is no live reload: edit on a PC and then build the disc.
