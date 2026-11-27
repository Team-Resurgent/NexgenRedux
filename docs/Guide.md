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
and the line; mend it, save, and it carries on. A console has no command line: there, put a folder called `dev` beside the title
(in its `media`), and copy changed scripts and pictures to it over the network, as with an FTP client or the debug monitor. A script
can also turn it on, or off, itself with `App::development`.

### Debugging

The VS Code extension in `engine/tools/vscode` (`Nexgen-vscode.vsix` in each release) stops a script where you ask it to: set a
breakpoint, press F5, and step over, into and out of functions, with the call stack, each frame's locals, the screen's members
and the module's globals. Objects open up to what they hold, the engine's own types through their properties; hovering over a
name, or typing `player.position.x` in the Debug Console, shows its value; a script error stops where it happened; and the log is
in the Debug Console. Its profiler, on the debug toolbar, shows which functions and lines took the scripts' time. *Run and debug* starts the showcase with `--debug 4711 --debug-wait`; *Attach* connects to one already
started with `--debug`, which listens on that machine only unless `--debug-remote` is given. On a console, a folder called `debug`
beside the title lets a computer on the network attach to it.

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

`rich` draws a paragraph with some of it marked: `[color=orange]`, `[b]`, `[u]` and `[s]`, each ended with `[/color]`, `[/b]`,
`[/u]` or `[/s]`, and `[[` for a bracket itself. Colours are names (white, black, red, green, blue, yellow, orange, pink, purple,
cyan, gray) or `#rrggbb`. It wraps as `paragraph` does, and `richHeight` says how tall it will be.

### Pictures to draw into

`Assets::picture(width, height)` makes a blank picture of its own. Between `c.beginPicture(picture)` and `c.endPicture()`,
everything drawn goes into it rather than onto the screen, 3D included; then it is drawn like any other picture:

```cpp
Texture@ map = Assets::picture(320, 320);

void draw(Canvas@ c)
{
    if (c.beginPicture(map, Color(0, 0, 0, 1)))     // cleared to a colour first, and its own pixels are the design
    {
        Space::camera(Vec3(0, 20, 0.01f), Vec3(0, 0, 0));
        Space::draw(c);                              // the world from above
        c.endPicture();
    }
    Space::camera(eye, look);
    Space::draw(c);
    c.image(map, c.width - 340, 20, 320, 320);       // a minimap
}
```

A minimap, a screen in a room, a mirror, or something drawn once and shown many times. One is drawn into at a time; effects,
bloom and grading are for the screen. Up to 2048 pixels a side, and 1024 on the original Xbox; the Pictures demo has one.

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

`Scene::path(from, to, way)` finds a way round the solid tiles: the corners of it, as straight as it can be, ending where it was
asked to go. It goes diagonally too, but never across a corner, unless it is told to keep to straight steps. `Scene::blocked`
says whether a solid tile is at a place.

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
| A model | `model`, and `animate("walk")` to play one of its animations, changing smoothly from the last |
| A simple shape | `shape(SolidBox, Red)`: a box, ball, cylinder, cone, ring, capsule or sheet |
| A mesh of your own | `mesh`, with a `Mesh` built from boxes, balls and triangles |
| A soft light | `glow`. `light` is what lights other things: the four strongest reach each thing |

A model is a glTF (`.glb` or `.gltf`), FBX, OBJ, STL or PLY file, with up to 128 bones and 65536 vertices, and the files it names in its
own folder. Anything but a `.glb` is turned into one as it loads; the cook tool does it beforehand, and has to for an FBX on the
original Xbox. FBX and glTF bring bones and animations; OBJ brings materials and pictures; STL and PLY are shapes. The engine makes two simpler versions as it loads,
and draws those when it is small on the screen. What can't be seen isn't drawn.

`Space::sun`, `ambient`, `sky`, `fog` and `shadows` set the scene. Shadows are the shape of what casts them, on the original Xbox too,
where the land casts none.

### Holding things, and joints

`attach(parent)` makes one thing move with another. `attach(parent, "hand.r")` makes it move with one of the parent's bones instead,
through every animation: a sword in a hand, a hat on a head, a light held up. `bones` and `bone(i)` list a model's.

Bodies with mass can be joined, to each other or, with `null`, to the world, at places in the world:

```cpp
Space::join(link, above, top);                                   // pinned, free to turn: chains
Space::hinge(door, null, Vec3(0, 1, 0), Vec3(0, 1, 0));          // turning about an axis: doors, lids, wheels
Space::rope(seat, null, Vec3(5, 1.3f, 0), Vec3(5, 6, 0));        // no further apart than now: it pulls, never pushes
Space::spring(weight, null, Vec3(3, 3, 0), Vec3(3, 6, 0), 40, 1, 2);  // stiffness, damping and length
```

A joint goes when either thing does, or with `Space::unjoin`. The Tumble demo's *Swings* has one of each.

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
| `exact` | Every triangle of its model is its collider |
| `moves`, `seconds`, `pathTime` | It goes that far and back again, for ever, and takes walkers along |
| `sound`, `reach`, `volume` | Something to hear from where it is, over and over |
| `behaviour` | A class in the script |

`Space::make` makes one entity from a `Json` of the same kind. The showcase's **Scene editor** makes such files: fly about, add
things, move, turn, size and colour them, shape and paint the land, lay roads, join caves together, set the sky and the fog, then walk,
drive or fly through the place, and save it; there can be several. It is a screen like any other, in
`showcase/editor`, and can be taken for your own tools. Models of your own appear at the end of its list: drop them on the window,
or put them in the folder `own` beside the saves, or in `media/own` on the original Xbox. Names there are small letters,
digits, dots and dashes.

### Using a scene in a game

A screen can include files of script that are shared: `#include "global:scene.as"` brings in `Level`, which makes a scene just as
the editor does, land, sky, roads and all:

```cpp
#include "global:scene.as"

Level level;
void onEnter() { level.load("asset:village.json"); level.set("health", 100); }
void update(float dt)
{
    string out = level.update(dt, player);          // "file|arrival" when the player went through a way out
    if (level.isOn("gate") && score > 10)
        level.show("treasure", 0.8f, "rise");       // hidden until now: it grows, rises or drops in
}
void draw(Canvas@ c) { Space::draw(c); level.drawHud(c); }
```

Anything with a name can be found (`level.find`), shown, hidden and switched on and off. **Triggers** are boxes that do something
when someone goes into them, or a shot lands in them: show or hide a thing, switch something on or off, play a sound, or only
tell the screen, through `level.onTouch`. **Ways out** lead to another scene, to an **arrival** of a name there. The HUD shows
bars and text by name, set with `level.set`. On endless land, what is in each region comes and goes with it.

The **Lost key** showcase is a whole game made this way: two scene files, a village at dusk and an island at night, say what is
where, which trigger opens the stone ring and what the HUD shows. Its script only walks, follows, keeps the score, and remembers
what was taken so it stays gone when you go back.

### Land by hand

Land that has an end can be changed, while a game runs too, and things on it follow:

```cpp
Space::raise(x, z, 6, 2);                            // a hill six metres round; less than 0 digs
Space::smooth(x, z, 6, 0.5f);
Space::level(x, z, 6, height, 1);                    // somewhere to build
Space::paint(x, z, 4, Color(0.55f, 0.42f, 0.28f), 1);  // a path; a clear colour takes it away
string kept = Space::landEdits;                      // all of it, as text for a Json
Space::restoreLand(kept);                            // on land of as many squares
```

`Space::hole(x, z, 3)` opens the land, so that there can be something under it: the cave pieces in `global:model` (`cave-mouth`,
`cave-tunnel`, `cave-bend`, `cave-slope`, `cave-room` and `cave-end`) meet on a grid of 4 metres. A model of your own whose name begins with one of those joins them in the editor, so an entrance can look
like anything. Road pieces (`road-straight`, `road-bend`, `road-t`, `road-cross` and `road-end`) meet on a grid of 8 metres. The sun doesn't reach what has a material whose name begins with `indoors`. `Space::edges = true` puts walls that
can't be seen at the sides of the land.

`Space::lines = true` draws only the edges of triangles, to see how things are made.

### Those who move by themselves

`#include "global:ai.as"` brings in `Agent` and `Crowd`. An agent is a walker with a role: a **guard** stands where it was
put and chases what it sees, a **patrol** walks a route of places, a **wanderer** walks about near home, and there are
**chase**, **flee**, **follow** and **still**. It sees with a range and a field of view, and only what no wall is in front of;
it hears noises the game makes; it searches where it last saw you, and goes back to what it was doing. Its `state`
("walking", "chasing", "searching" and so on) is there for animations and sounds, and `onEvent` tells the game when it
spots, loses, catches or hears something.

```cpp
Crowd crowd;
Agent@ guard = crowd.add(robot, "patrol");
guard.route = { Vec3(-10, 0, 0), Vec3(10, 0, 0), Vec3(10, 0, 12) };
@guard.target = player;
...each update: crowd.update(dt);         // crowd.noise(place) when the player runs; crowd.draw(c) shows what they see
```

Ways round what is in the way come from the engine: `Space::navigate(middle, size)` works out where things can walk, in
squares over a part of the world, and `Space::path(from, to, way)` gives the shortest way between two places, made as
straight as it can be. In a scene file, a thing with a `"role"` moves by itself as the level's crowd, and a patrol's
`"route"` is the waypoints of that name; the editor has both. The **Guards** showcase is a game of getting past them.

### Water

`Space::water(height, color)` fills everything below a height: clear in the shallows and dark where it is deep, the sky in
it at a low angle, and foam where it meets the land. `Space::waterStyle` sets how it moves: `calm`, `sea`, `choppy` (crests
that lean and break white), `storm`, or `cartoon` (flat light and a hard line of foam). Or set it yourself with
`waterWaves(height, length, speed, choppy)`, `waterColors(shallow, deep)` and `waterFoam(shore, crests)`. The waves are moved
on the CPU, so they look and cost the same everywhere; a scene file says `"seaStyle"` in its sky, and the editor has it on
the Sky tab. The **Water** showcase goes round an island in each style.

### Mist, smoke and fire

`Space::mist(middle, size, color, thickness, swirl, grain)` puts a box of mist in the world that can be walked into: billows
of noise in three dimensions, drifting with the wind and soft at the box's edges. The colour's alpha is how much it hides;
swirl is how fast it moves and grain how many metres its billows are. Low and wide it is a bank of fog over a valley; tall
and grey over a fire it is smoke; small, orange and swift it is the fire's glow. It gives an id, for `moveMist` and
`removeMist`. It is drawn as slices that face the camera, coloured from a texture with three dimensions, which every
platform has, the original Xbox included; there, where there is less to fill the screen with, with fewer slices. The
**Mist** showcase is a valley at dawn.

### Blocks

`Space::voxels(corner, across, high, deep, size)` makes a world of blocks, up to 1024 across, 256 high and 1024 deep, each
`size` metres. It is empty until it is filled: `setVoxel(x, y, z, kind)` one at a time, `fillVoxels` a box, `voxelBall` a
ball (0 digs a hole), or `voxelHills(seed, height, roughness, sea, caves)` hills from a number, with grass and earth over
stone, sand at the sea, snow on the tops and hollows under them. Kind 0 is nothing; 1 to 255 are kinds, coloured on top and
on their sides by `voxelColor` (twelve to begin with: grass, earth, stone, sand, wood, leaves, brick, snow, planks, dark
stone, gold, ice). `voxelRay(from, to, cell, face, hit)` finds the block a line meets and the face it goes into, so cell
plus face is where a new block goes; `voxelCell` and `voxelPlace` turn places into blocks and back.

The blocks are kept in chunks of 16 each way, and only where there is something. Each is made into the faces that can be
seen, side by side faces of the same kind joined into one, and corners darker where blocks crowd them: a new world is made
a few chunks a frame, nearest first, and a block that is dug or built is seen at once. They are part of the land: walkers
stand on them and bump into them, things fall onto them, and `raycast` meets them as group 0. The **Blocks** showcase is
hills to dig and build in, in the first person; at 60 frames a second on the original Xbox too.

### Wind

Plants lean with the wind: give a thing a `sway`, how far its top moves for each metre of height (about 0.02 for a tree, 0.15 for
`global:model/grass.glb`), or `"sway"` in a scene file. `Space::wind = Vec3(1, 0, 0.4f)` says which way it blows and how hard, and
gusts roll across a wood as a wave. The base stays where it is, it is only how things are drawn, and it costs the same on every
platform. The editor gives trees, bushes and grass their sway when they are added.

### Grass, impostors and sprites

`Space::grass(Assets::model("global:model/grass.glb"), 3, 30)` grows tufts over the land near the camera, so many to a
square metre and out to a distance: only where the land is green, flat enough and dry, coloured as the land is there, and
bending in the wind. It is made a few squares at a time as you move, and again where the land is changed. A scene file says
`"grass": 2.5`, and the editor has it on the Land tab. On the original Xbox, where vertices can't be moved, it stands still.

Things with `impostor = true` are drawn, further away than `Space::impostors` metres, as a picture of themselves: made from
the model when it is first needed, from four sides, and drawn with all the others that share it at once. A wood of
two thousand trees costs about half as many triangles, and looks the same.

A sprite is a picture that stands upright and turns to face the camera, as the characters of old maze games did:

```cpp
Entity@ wisp = Space::spawn(place);
wisp.sprite(Assets::texture("global:picture/sprite-wisp.png"), 4, 2);  // four sides across, two frames down
wisp.size = Vec3(1.3f, 1.3f, 1.3f);
wisp.spriteFrame = uint(App::time * 5) % 2;
```

Across the sheet is how it looks from each side, the first from in front and then going round to its left; which one is
shown follows from where the camera is and which way it faces. In a scene file it is `"sprite"`, with `"views"` and
`"frames"`. The **Wisps** showcase is a maze of them, and the wisps in **Guards** run from you.

### Files that players bring

`Assets::list("own:")` has the names of the player's own files, and `Assets::dropped()` those that were just dropped on the window. They
are loaded like any others: `Assets::model("own:chair.obj")`.

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

## Interfaces

Menus and forms are widgets. Add them to `Ui::root`, say what to do when they are used, and draw the page:

```cpp
void onEnter()
{
    Widget@ page = Ui::root;
    page.padding = 40;
    page.label("Settings", "title");
    Widget@ row = page.row();
    row.label("Volume").grow = 1;                       // takes what is left over
    row.slider(0.7f, 0, 1, WidgetEvent(this.onVolume));
    page.toggle("Subtitles", true, WidgetEvent(this.onSubtitles));
    page.button("Done", WidgetEvent(this.onDone));
}

void onVolume(Widget@ w) { Audio::masterVolume = w.value; }

void draw(Canvas@ c)
{
    c.designHeight(720);
    Ui::draw(c);
}
```

Nothing has a position. Columns, rows, grids and stacks share out their room by `grow`, `gap`, `padding`, `align` and `justify`, and
text that wraps is as tall as it needs. The d-pad, arrows and mouse all move the same focus, scrolling to what they reach, and the
shoulder buttons turn tabs and pages of a list. What the interface does with a press isn't seen by `Input::pressed` too.

| There are | Made with |
|---|---|
| Containers | `column`, `row`, `grid`, `stack`, `scroll`, `panel`, `tabs` |
| Things to use | `button`, `toggle`, `slider`, `choice`, `field`, `list` |
| Things to see | `label`, `paragraph`, `image`, `progress`, `line`, `space`, `custom` |

A `list` may have any number of rows: it makes those that can be seen and uses them again. A `custom` widget is drawn by you, and may
be a view of `Space`. With a gamepad, a `field` brings up a keyboard on the screen.

`Ui::dialog` asks something, `Ui::toast` says something, and `Ui::popup` is a page in front of the page.

### Pages, themes and languages from files

```json
{ "column": [
  { "label": "@title", "style": "title" },
  { "row": [ { "label": "@name", "grow": 1 }, { "field": "", "id": "name", "change": "onName" } ] },
  { "button": "@send", "click": "onSend" }
], "padding": 40, "gap": 16 }
```

`Ui::load("asset:page.json")` adds that; `click`, `change`, `focus`, `done`, `draw` and `makeRow` name methods of the screen, and
`Ui::find("name")` finds a widget by its id. Text that begins with `@` is looked up in what `Ui::words("asset:english.json")` read,
which is one name and text after another.

A theme changes colours, or whole styles in each of their states:

```json
{
  "colors": { "accent": "#ff2fb0", "panel": "#120026" },
  "styles": {
    "button": { "base": "control", "radius": 4, "focused": { "back": "$accent", "scale": 1.05 }, "pressed": { "scale": 0.97 } },
    "big": { "base": "plain", "size": 60 }
  }
}
```

A style has `back`, `text`, `line`, `track`, `lineWidth`, `radius`, `scale` and `shadow`, all of which move smoothly between
`hovered`, `checked`, `focused`, `pressed` and `disabled`; and `font`, `size`, `image` and `slice`, `padding`, `gap`, `minWidth`,
`minHeight`, `align` and `seconds`.

## Shaders, bloom and grading

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

An effect is one file, for every GPU that has shaders: Direct3D 11 and OpenGL now. The original Xbox has bloom but no such shaders;
there, a line `// xbox: lava` in the file names something close that it can do: `lava`, `dissolve`, `hologram` or `flag` on a thing, and
`tube`, `pixels`, `ripple`, `glitch`, `grade` or `ink` over the picture. Without one, `effect` gives false and things are drawn as they are. A mistake in one is in the
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

### Grading

```cpp
Look@ look = Look("dusk");         // or Look(), and settings of your own
look.saturation = 0.8f;
look.shadows = Color(0.3f, 0.4f, 1, 0.4f);   // bluer where it is dark; alpha is how much
look.load("asset:film.cube");      // a table from a grading program, first, if you like

void draw(Canvas@ c)
{
    Space::draw(c);
    c.bloom(0.3f, 0.8f);
    c.grade(look);                 // strength 0 to 1 is a second number
    c.text(font, "Not graded", 40, 40);
}
```

A `Look` is what each colour of the picture becomes. It has settings: `exposure` in stops, `warmth` and `tint`, `contrast`,
`saturation`, `fade` (blacks lifted, as old film), `gamma`, and colours for the `shadows` and the `highlights`. `preset` sets them all
to one ready-made: `none`, `warm`, `cold`, `faded`, `noir`, `sepia`, `dusk`, `vivid`, `bleach` or `night`. `load` reads a .cube 3D
table, as Resolve, Photoshop and other grading programs write them, and colours go through it before the settings; `apply` gives what
it makes of one colour, without the GPU.

Whatever it is, it is worked out once into a table of 32 by 32 by 32 colours, a texture with three dimensions, and again only when
something about it changes. `grade` then looks each pixel up in it, in one pass over the screen: with a shader on Direct3D 11 and
OpenGL, and on the original Xbox with its own texture stages, which can look a colour up in a table, so it costs little there too.
Within a clip, only that part is graded. The **Looks** showcase shows them all, before and after, at 60 frames a second on the
original Xbox.

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
| Models | glTF 2.0 (`.glb`, `.gltf`), FBX, OBJ, STL and PLY. Pictures in them up to 4096 pixels a side, made smaller if need be |

## Sound

```cpp
hit.play();                      // as an effect
hit.play(0.5f, -1, 1.2f);        // quieter, from the left, higher
Voice music = tune.playMusic(0.8f, 2);
music.stop(1.5f);                // fading out
```

A `Voice` is safe to use after the sound has ended. A screen's sounds pause while it is covered and stop when it closes.

Everything on a bus can be heard through effects, each gliding to its new setting over the seconds given:

```cpp
Audio::lowPass(BusEffects, 600, 0.4f);     // muffled, as under water or through a wall; 20000 is clear again
Audio::highPass(BusEffects, 1000);         // thin, as a small radio
Audio::reverb(BusEffects, 0.7f, 0.9f);     // how much of the room, and how big it is
Audio::echo(BusEffects, 0.5f, 0.4f, 0.3f); // how loud, how late, and how much echoes again
Audio::clearEffects();
```

A screen starts with none, and they go when it closes. The Sounds demo puts its pads in a room, a hall, a canyon, under water and
on the radio.

## Input

Scripts ask for actions, not keys: `up`, `down`, `left`, `right`, `accept`, `cancel`, `fire`, `alternate` and `menu`. Players
can change every one except `menu`; the Settings demo shows how. `Input::move` gives a direction from keys, d-pad or stick.

On the original Xbox, a USB keyboard and mouse work too, through an adapter in a controller port or its bottom slot: keys,
typing and the mouse are read as they are on a computer, and the engine draws the pointer, since nothing else there does.
Where there is only a controller, the stick can move a pointer instead, and A click with it: the player chooses that in
Settings (`Input::stickPointer`, kept with the other settings), and a screen says it has a use for one with
`Input::wantsPointer = true`, as the scene editor does while editing. The stick and A then do nothing else, and a mouse
that moves takes over.

Pads rumble: `Input::rumble(0.6f, 0.2f)` shakes every pad for a fifth of a second, and `Input::rumbleMotors(heavy, light, seconds,
pad)` sets the slow, heavy motor and the quick, light one each. It stops when the engine's menu opens and when the screen changes,
and players can turn it off in Settings (`Input::rumbleOn`, kept with the other settings).

## Paths and noise

A `Path` is a smooth way through points, passing through every one, for things to follow at a steady speed:

```cpp
Path@ track = Path({ Vec3(0, 0, 0), Vec3(10, 0, 4), Vec3(20, 2, 0), Vec3(10, 0, -6) }, true);   // a loop
float along = 0;

void update(float dt)
{
    along += 6 * dt;                                    // metres a second, however far apart the points are
    car.place = track.at(along);
    car.angles = Vec3(0, atan2(-track.direction(along).z, track.direction(along).x) * 57.3f, 0);
}
```

On the plane, give it `Vec2`s with `add` and ask `at2` and `direction2`. `nearest` says how far along it a place is closest.

`noise(x, y, z)` is smooth randomness, -1 to 1, with hills and dips about a unit apart and the same at the same place every time:
scale the place for bigger or smaller ones, and move through `z` for noise that changes over time. `fractalNoise` adds finer
layers, for land, clouds and fire; `noiseSeed` picks another pattern.

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

A `Room` is a game on the local network, or on the internet with a meeting place. One player hosts; others find it by the game's name and join.

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

### Playing over the internet

Players on different networks meet at a **meeting place**: a machine every one of them can reach, such as a small server
somewhere, running the engine with `--meeting 27300` (no window, nothing else; it says now and then how many rooms and players
it has). Give it to a room before hosting, finding or joining:

```cpp
room.meet("games.example.com");               // or "1.2.3.4:27300"
room.host("my game", "Sam");                  // listed there, as well as on this network
```

A room that is hosted is listed there; `find` asks it as well as the local network, and `foundFar(i)` says which were found
there. Joining one of those, the meeting place introduces the two players, so that each one's router lets the other's packets
in, and they play directly; if a router won't have it, the meeting place passes their packets on, between those two only and
only so much a second, and `relayed(player)` says so. The player can choose a meeting place once, in the settings, as
`Net::meetingPlace`, kept between runs; every room meets there unless it is told otherwise. A room whose host has let its port
(27310) through the router can also be joined by address, as `room.join("my game", "games.example.com:27310", "Alex")`, with
no meeting place at all.

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

Files can go to and from the console over the network. Nothing listens until it is asked to, and nothing can be changed unless
that is asked for too:

```cpp
Xbox::startFtp("xbox", password, false);             // drives E, F and G; true lets it write
```

Only machines on the same network are answered. FTP hides nothing from them, so start it when it is wanted and then `Xbox::stopFtp()`.

## Saving

```cpp
Save::set("best", formatInt(score));
int best = int(parseInt(Save::get("best", "0")));
```

## Files and JSON

`Json` reads and makes JSON. What isn't there is null, and can be asked things all the same:

```cpp
Json j;
string error;
if (j.parse(Assets::read("asset:level.json"), error))
{
    float x = float(j["player"]["place"][0].number());
    Vec3 at = j["door"].vec3();                      // [x, y, z]
}
j.set("score", 40);                                  // text, flags, places, colours and other Json too
Assets::write("asset:level.json", j.write());
```

`Assets::write` writes only a screen's own `asset:` files that end in `.json`. With `--dev` it is the file in the media; otherwise
it is kept with the saves, on consoles too, and `Assets::read` and `Space::load` read that one from then on. `Assets::forget` goes
back to the one that came with the screen.

## On the original Xbox

- 64 MB in all. Images and fonts may take 20 MB and sounds 6 MB by default.
- A scene may have 4096 entities and 1500 particles at once; elsewhere 16384 and 8000.
- Build the disc with the cook tool, which makes images a quarter to an eighth of the size in memory and sounds a quarter,
  under the same names.
- Three dimensions use the fixed pipeline: lights are worked out at corners, so give a floor plenty of squares. Ray-marched shapes
  become triangles. Effects are stand-ins.
- Live reload works with a `dev` folder beside the title: copy changed scripts to the console over the network and the screen
  reloads. The debugger works over the network too: put a `debug` folder there, or call `App::debugger()`, and attach to the
  console's address.
