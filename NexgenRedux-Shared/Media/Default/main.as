void Init()
{
    DebugPrint(1, "Initializing...");

    // create window otherwise program will just exit
    uint windowID = WindowCreate(640, 480, "my window title");

    DebugPrint(1, "Window id = " + windowID);
    // SetWindowBackground(windowID, 0xff00ff);
}

void Render(uint windowID, float dt)
{
    DebugPrint(1, "Frame Render on window " + windowID + " dt = " + dt);
}
