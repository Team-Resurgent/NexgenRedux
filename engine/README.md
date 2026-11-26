# Nexgen engine

The new engine described in [docs/EnginePlan.md](../docs/EnginePlan.md). Scripts are described in
[docs/ScriptReference.md](../docs/ScriptReference.md).

## Build

From this folder:

| Target | Commands |
|---|---|
| Windows | `cmake --preset windows`, `cmake --build --preset windows`, `ctest --preset windows` |
| Linux | `cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release`, `cmake --build build/linux`, `ctest --test-dir build/linux` |
| macOS | The same as Linux. Tested in CI only, without a screen |
| Original Xbox | `cmake --preset xboxog-release`, `cmake --build --preset xboxog-release`, then run `build/xboxog-release/XISO/showcase.iso` |
| Xbox 360 | `cmake --preset xbox360 -DRXDK360_CC=<RXDK-360 source tree>/build/llvm/bin/clang.exe`, `cmake --build --preset xbox360`, then run `build/xbox360/nexgen/default.xex` in Xenia |
| UWP: Xbox One, Series and Windows | `cmake --preset uwp`, `cmake --build --preset uwp`; the package is in `build/uwp/AppPackages` |

Needs CMake 3.21 and, for the Xbox targets, Ninja on the path (it comes with Visual Studio).

- **Linux:** the X11 and OpenGL headers (`libx11-dev libgl-dev`).
- **Original Xbox:** RXDK 1.3.13 or later; `tools/stage-rxdk.ps1` installs it on a build machine. To ship compressed textures
  and sounds, build `cook` for Windows first and add `-DNX_COOK=<path to cook.exe>`. On a debug kernel or devkit the log goes
  to the debug output; in xemu add `-device lpc47m157 -serial stdio`.
- **UWP:** Visual Studio with the Universal Windows Platform tools. Windows installs only signed packages, so sign it, or unpack it
  with `makeappx unpack` and run `Add-AppxPackage -Register AppxManifest.xml` in Developer Mode. Its log is `nexgen.log` in the
  app's LocalState folder.
- **Xbox 360:** RXDK-360 from November 2026 or later, built from its source, and Visual Studio 2022 or later with its Xbox 360
  platform: the libraries are compiled with its clang, and the title is linked by its MSBuild toolset into `default.xex`, with
  the media beside it. Rebuild its `libcpp.a` (`tools/build_libcpp.py`) whenever its LLVM is: an old one leaves `std::stringstream`
  empty. The log is in Xenia's `xenia.log`, or a dev kit's debug output.

`.github/workflows/engine.yml` builds, tests and packages Windows, Linux, macOS, UWP and the original Xbox, and
publishes the packages from `main` in the `latest` release.

## Run

The showcase is in the [NexgenRedux-Media](../NexgenRedux-Media) submodule (`git submodule update --init`). `nexgen` opens its
main menu. Each folder in `media/showcase` is a screen: a `screen.json` and a `main.as`. Add a folder and
it appears in the menu.

| Option | |
|---|---|
| `--saves FOLDER` | Keep saves there, with `--demo` too. On the original Xbox, an empty folder `media/trial` is where they go |
| `--dev` | Reload scripts and images when their files change |
| `--screen ID` | Start with a screen open |
| `--demo SECONDS` | Show every screen, or the one given with `--screen`, for a while, playing it, then stop |
| `--bench --frames 400` | Don't wait for the display, and say how long frames took (Direct3D 11) |
| `--tour N` | Open every screen N times, unattended, and check that nothing leaks |
| `--hidden --frames 100 --capture out.bmp` | Save a picture without showing a window or making a sound |
| `--docs FILE` | Write the script reference |
| `--budget MB` | How much memory images and fonts may take |

For an OG Xbox self-test, configure with `-DNX_TOUR=ON`: that disc shows each screen for a moment, then opens them all rapidly
twice more to check that nothing leaks, and stops. `spike` is the phase 0 test of script calls and memory.

Saves are in `%LOCALAPPDATA%\Nexgen` (Windows), `~/.local/share/nexgen` (Linux), `~/Library/Application Support/Nexgen`
(macOS), the app's LocalState (UWP) and the title's own folder (Xbox).

## Record a video

On Windows, with ffmpeg. The app hands over each frame and its sound, so none is missed, and nothing is shown or heard meanwhile.

```powershell
python tools/record/record.py build/windows/showcase/Release/nexgen.exe demos.mp4 8
```

## Cook

`cook --platform xboxog|desktop <media folder> <new folder>` makes a copy of a media folder in which images are BC1 or BC3
textures, sounds are ADPCM and models of every kind are `.glb`, under the same names. Small images and long music stay as they are. Only what has changed is
done again. `cook house.fbx` makes one model, and `cook --help` has the rest.

`tools/cook-ui` is a window for it, made with Avalonia on .NET 10: `dotnet run --project tools/cook-ui`, with `NEXGEN_COOK` set to the
cook tool that was built. Both are a download of their own, for Windows, Linux and macOS on x64 and arm64.

## Layout

| Folder | What | May use |
|---|---|---|
| `base/` | Memory, handles, maths, logging, JSON, threads | nothing |
| `gpu/` | GPU interface; `d3d11` (Windows, UWP), `d3d8` (original Xbox), `gl` (Linux, macOS), `null` (tests) | base |
| `platform/` | Window, input devices, time, files; `win32`, `uwp`, `xboxog`, `linux`, `macos`, and what some share in `windows` and `posix` | base |
| `input/` | Actions, from any device, and their bindings | base |
| `audio/` | Decoding, mixing and output | base |
| `assets/` | Cache of images, fonts, sounds and models; meshes being made | gpu, audio |
| `canvas/` | 2D drawing | assets |
| `scene/` | Entities, behaviours, collision, physics, particles, prefabs and maps; in space, with land, walkers and shapes that melt | canvas, script |
| `ui/` | Widgets: layout, focus, themes, lists, dialogs, typing, pages from files | canvas, script, input |
| `save/` | What players keep | platform |
| `script/` | AngelScript host, coroutines, bindings and the reference writer | base |
| `bindings/`, `screens/` | What scripts can call; the screen stack and the engine's menu | all above |
| `app/` | The program that runs the showcase | all |
| `tools/` | `cook`, and `cook-ui`, a window for it; `record`; `make_media.py`, `make_models.py` and `make_pieces.py`, which make the showcase's sounds, music and models; `make_docs.py`, which makes the web site; `stage-rxdk.ps1` | all |
| `third_party/` | AngelScript, SSFN, stb, cgltf, ufbx, miniaudio, dr_libs and Mbed TLS, unmodified | |
| `spike/`, `tests/` | Phase 0 app and tests | all |

Platform `#if`s belong only in `platform/`, `gpu/`, `base/thread_*`, `audio/output_*` and `net/`.

The engine is self-contained: it needs a compiler and the platform's SDK, and nothing is downloaded. The
small libraries in `third_party` are unmodified copies. The `self_contained` test enforces this.
