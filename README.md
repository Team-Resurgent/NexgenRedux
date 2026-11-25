<h1 align="center">Nexgen Redux</h1>

<p align="center"><b>A cross-platform 2D and 3D engine for games and dashboards, scripted in AngelScript: original Xbox, Windows, Linux and macOS</b></p>

<p align="center">
  <a href="https://github.com/Team-Resurgent/NexgenRedux/actions/workflows/engine.yml"><img src="https://github.com/Team-Resurgent/NexgenRedux/actions/workflows/engine.yml/badge.svg" alt="Engine"></a>
  <a href="https://team-resurgent.github.io/NexgenRedux-Docs/"><img src="https://img.shields.io/badge/docs-online-brightgreen.svg" alt="Documentation"></a>
  <a href="https://discord.gg/VcdSfajQGK"><img src="https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord" alt="Discord"></a>
</p>

<p align="center">
  <a href="https://ko-fi.com/J3J7L5UMN"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="ko-fi"></a>
  <a href="https://www.patreon.com/teamresurgent"><img src="https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white" alt="Patreon"></a>
</p>

<p align="center">
  <a href="https://github.com/Team-Resurgent/NexgenRedux/releases/latest"><img src="https://img.shields.io/badge/download-latest-brightgreen.svg?style=for-the-badge&logo=github" alt="Download"></a>
</p>

<p align="center">
  <img src="docs/screenshots/main.png" alt="The showcase's main menu" width="80%">
</p>

The same scripts run everywhere, 3D and all: every one of the 52 demos runs at 54 to 60 frames a second on a real 64 MB original Xbox. The engine is self-contained: it needs
a compiler and the platform's SDK, downloads nothing, and uses no SDL or GLFW.

## Screenshots

<p align="center">
  <img src="docs/screenshots/explore.png" alt="Island" width="49%">
  <img src="docs/screenshots/dashboard.png" alt="Dashboard" width="49%">
</p>
<p align="center">
  <img src="docs/screenshots/endless.png" alt="Endless land" width="32%">
  <img src="docs/screenshots/melt.png" alt="Ray marching" width="32%">
  <img src="docs/screenshots/tumble.png" alt="Tumble" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/cases.png" alt="Cover flow" width="32%">
  <img src="docs/screenshots/skim.png" alt="Skimmer" width="32%">
  <img src="docs/screenshots/shaders.png" alt="Shaders" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/forest.png" alt="Forest" width="32%">
  <img src="docs/screenshots/lights.png" alt="Night lights" width="32%">
  <img src="docs/screenshots/blades.png" alt="Blades" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/spire.png" alt="Spire" width="32%">
  <img src="docs/screenshots/rebound.png" alt="Rebound" width="32%">
  <img src="docs/screenshots/split.png" alt="Split screen" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/music.png" alt="Music" width="32%">
  <img src="docs/screenshots/runes.png" alt="Rune shift" width="32%">
  <img src="docs/screenshots/rally.png" alt="Comet rally" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/shooter.png" alt="Shooter" width="32%">
  <img src="docs/screenshots/platformer.png" alt="Platformer" width="32%">
  <img src="docs/screenshots/physics.png" alt="Physics" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/cracktro.png" alt="Cracktro" width="32%">
  <img src="docs/screenshots/parallax.png" alt="Parallax" width="32%">
  <img src="docs/screenshots/coroutines.png" alt="Coroutines" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/shapes.png" alt="Shapes" width="32%">
  <img src="docs/screenshots/blend.png" alt="Blending" width="32%">
  <img src="docs/screenshots/gauges.png" alt="Gauges" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/sounds.png" alt="Sounds" width="32%">
  <img src="docs/screenshots/loading.png" alt="Background loading" width="32%">
  <img src="docs/screenshots/settings.png" alt="Settings" width="32%">
</p>
<p align="center">
  <img src="docs/screenshots/xbox-explore.png" alt="Island on the original Xbox" width="32%">
  <img src="docs/screenshots/xbox-forest.png" alt="Forest on the original Xbox" width="32%">
  <img src="docs/screenshots/xbox-lights.png" alt="Night lights on the original Xbox" width="32%">
</p>
<p align="center"><sub>The last row is a real original Xbox</sub></p>

## Features

- **Scripts** — AngelScript 2.39.0. Each screen is one folder with one script; a mistake in it shows its file and line, and
  never takes the app down. Coroutines, tweens, and live reload while you edit.
- **2D drawing** — shapes, images, sprite sheets, nine-slice, tiling, scalable text, clipping and blending, batched, on any
  shape of screen.
- **Scenes** — entities with behaviours, prefabs, tile maps, particles, collision and 2D physics, described in plain text files.
- **3D** — glTF models with animation, lights, shadows, fog, sky and water; hills and endless land that streams as you go; rigid
  bodies and characters that walk; culling and levels of detail; ray-marched shapes; particles; split screen.
- **Shaders** — bloom, and effects of your own over the picture or on things: one file for every GPU that has shaders.
- **Networking** — rooms to play in on the local network, TCP, UDP, and HTTPS with certificates that are checked.
- **Console calls** — on the original Xbox: temperatures, fan, light, tray, drives, launching titles, and back to the dashboard.
- **Sound** — 32 voices with volume, pan, pitch and fades, or from a place in space; WAV, Ogg Vorbis and MP3; spectrum and waveform for visualisers.
- **Assets** — loaded in the background, shared, kept warm and unloaded only when a budget needs the room.
- **Input** — keyboard, mouse and four gamepads as actions that players can rebind.
- **Saving** — settings and scores, kept between runs.
- **Cook tool** — turns images into compressed textures and sounds into ADPCM for the console, under the same names.

## Platforms

| Platform | State | Graphics | Sound |
|---|---|---|---|
| Windows 10 and 11 | Working | Direct3D 11 | miniaudio |
| Original Xbox | Working on a console, and in xemu (64 and 128 MB) | Direct3D 8 | DirectSound |
| Linux | Working (X11) | OpenGL 3.3 | miniaudio |
| macOS 11 and later | Builds and passes its tests in CI (Intel and Apple silicon); not yet run on a Mac with a screen | OpenGL 3.3 | miniaudio |
| UWP: Xbox One, Series and Windows | Working on Windows; not yet tried on a console | Direct3D 11 | miniaudio |
| Xbox 360 | Shared code compiles; to come | | |

## Download

The [latest release](https://github.com/Team-Resurgent/NexgenRedux/releases/latest) has a package for each platform, built from
`main`, with all the demo scripts in it:

| Package | What to do |
|---|---|
| `Nexgen-windows-x64` | Unzip and run `nexgen.exe` |
| `Nexgen-linux-x64` | Unpack and run `./nexgen` |
| `Nexgen-macos-arm64`, `-macos-x64` | Unzip and open the app. It isn't notarised: the first time, right-click it and choose Open |
| `Nexgen-uwp-x64` | Xbox One, Xbox Series or Windows, in Developer Mode: install the `.cer` and then the package, or upload the package in the Xbox Device Portal |
| `Nexgen-xbox-og` | Copy the folder to the console, or run the `.iso` in xemu or from an ODE |

To change a demo, edit its `main.as` in `media/showcase` (inside the app on macOS, under `Contents/Resources`). Started with
`--dev`, the showcase reloads scripts and images as they are saved.

## A screen

```cpp
class Hello : Screen
{
    Font@ font = Assets::font("global:freesans.sfn", 48);
    Sound@ pop = Assets::sound("global:sound/pop.wav");
    Tween size(0, 1, 0.6f, EaseOvershoot);

    void update(float dt)
    {
        if (Input::pressed("accept")) pop.play();
    }

    void draw(Canvas@ c)
    {
        c.design(1280, 720);
        c.circle(640, 360, 200 * size.value, Blue);
        c.text(font, "Hello", 640, 330, White, AlignCenter);
    }
}
```

Put that in `media/showcase/hello/main.as`, beside a `screen.json` with a title, and it appears in the menu. Everything that
scripts can call, a guide and every demo's script are in the
[documentation](https://team-resurgent.github.io/NexgenRedux-Docs/).

## Building

See [engine/README.md](engine/README.md). In short, from `engine/`:

```powershell
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

```powershell
cmake --preset xboxog-release
cmake --build --preset xboxog-release
```

The Xbox build needs [RXDK](https://github.com/Team-Resurgent/RXDK-VSCode).

## What is where

| Folder | What it is |
|---|---|
| `engine/` | The engine |
| `docs/` | [The plan](docs/EnginePlan.md), [the guide](docs/Guide.md) and [the script reference](docs/ScriptReference.md) |
| `NexgenRedux-Shared`, `-PC`, `-OG`, `-360`, `-UWP` | The first engine, kept as it is |
| `NexgenRedux-Docs` | The [documentation site](https://team-resurgent.github.io/NexgenRedux-Docs/) (submodule) |
| `NexgenRedux-Media` | The demos: their scripts, pictures and sounds (submodule; needed to build) |

Clone with the submodules:

```bash
git clone --recurse-submodules https://github.com/Team-Resurgent/NexgenRedux.git
```

## Credits

Nexgen Redux is by Team Resurgent: EqUiNoX, HoRnEyDvL and RetroMura.

The engine includes unmodified copies of [AngelScript](https://www.angelcode.com/angelscript/),
[SSFN](https://gitlab.com/bztsrc/scalable-font2), [cgltf](https://github.com/jkuhlmann/cgltf), [stb](https://github.com/nothings/stb),
[miniaudio](https://miniaud.io), [dr_libs](https://github.com/mackron/dr_libs) and
[Mbed TLS](https://github.com/Mbed-TLS/mbedtls), under their own licences in
`engine/third_party`.
