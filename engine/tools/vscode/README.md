# Nexgen Redux for VS Code

Debugs Nexgen Redux scripts as they run, on a PC or on a console over the network:

- breakpoints, and stepping over, into and out of functions
- the call stack, with each frame's locals, the object's own members and the module's globals
- objects opened up to what they hold: script classes, arrays, and the engine's own types through their properties
- hovering over a name, or typing `player.position.x` or `tiles[3]` in the Debug Console, shows its value
- a script error stops where it happened, before anything is unwound
- the engine's log in the Debug Console

It also colours AngelScript.

## Installing

Each release has it as `Nexgen-vscode.vsix`: `code --install-extension Nexgen-vscode.vsix`. Or make it from here:

```
python engine/tools/vscode/package.py
code --install-extension engine/tools/vscode/nexgen-redux-1.0.0.vsix
```

## Running

Add a launch configuration from **Run > Add Configuration... > Nexgen Redux**.

- **Run and debug** starts the engine with `--debug 4711 --debug-wait`, so that it waits until the breakpoints are
  set. Point `program` at the `nexgen.exe` you built. Add `--dev` in `args` to reload scripts as they are saved too.
- **Attach** connects to an engine already started with `--debug [PORT]`, which listens on this machine only; add
  `--debug-remote` to debug it from another. On a console, which has no command line, put a folder called `debug`
  beside the title (or have a script call `App::debugger()`), then attach to the console's address. (Not yet on the Xbox 360, which has no network layer.)

Files are named by the engine as they are in its media folder, such as `showcase/blocks/main.as`. The extension
looks for that folder in the workspace (`media`, or `NexgenRedux-Media/media`); set `media` if it is elsewhere.

While a script is stopped the engine's window stays open but doesn't move. Closing the debugger lets it carry on.
