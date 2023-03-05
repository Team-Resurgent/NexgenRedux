void OnWindowIconify(uint windowHandle, uint iconified)
{
    DebugPrint(0, "OnWindowIconify windowHandle = " + windowHandle + ", iconified = " + iconified);
}

void OnWindowMaximize(uint windowHandle, uint maximized)
{
    DebugPrint(0, "OnWindowMaximize windowHandle = " + windowHandle + ", maximized = " + maximized);
}

void OnWindowSize(uint windowHandle, uint width, uint height)
{
    DebugPrint(0, "OnWindowSize windowHandle = " + windowHandle + ", width = " + width + ", height = " + height);
}

void OnWindowFocus(uint windowHandle, uint focused)
{
    DebugPrint(0, "OnWindowFocus windowHandle = " + windowHandle + ", focused = " + focused);
}

void OnWindowKeyboardKey(uint windowHandle, uint key, uint scancode, uint action, uint modifier)
{
    DebugPrint(0, "OnWindowKeyboardKey windowHandle = " + windowHandle + ", key = " + key + ", scancode = " + scancode + ", action = " + action + ", modifier = " + modifier);
}

void OnWindowKeyboardCharacter(uint windowHandle, uint codepoint)
{
    DebugPrint(0, "OnWindowKeyboardCharacter windowHandle = " + windowHandle + ", codepoint = " + codepoint);
}

void OnWindowMouseCursorPosition(uint windowHandle, double xPos, double yPos)
{
    // commented out to reduce noisey logging
    // DebugPrint(0, "OnWindowMouseCursorPosition windowHandle = " + windowHandle + ", xPos = " + xPos + ", yPos = " + yPos);
}

void OnWindowMouseCursorEnter(uint windowHandle, uint entered)
{
    DebugPrint(0, "OnWindowMouseCursorEnter windowHandle = " + windowHandle + ", entered = " + entered);
}

void OnWindowMouseButton(uint windowHandle, uint button, uint action, uint modifier)
{
    DebugPrint(0, "OnWindowMouseButton windowHandle = " + windowHandle + ", button = " + button + ", action = " + action + ", modifier = " + modifier);
}

void OnWindowMouseScroll(uint windowHandle, double xOffset, double yOffset)
{
    DebugPrint(0, "OnWindowMouseScroll windowHandle = " + windowHandle + ", xOffset = " + xOffset + ", yOffset = " + yOffset);
}

void OnWindowDrop(uint windowHandle, array<string> paths)
{
    for (uint i = 0; i < paths.length(); i++)
    {
        DebugPrint(0, "OnWindowDrop windowHandle = " + windowHandle + ", paths" + i + " = " + paths[i]);
    }
}

void OnJoystickConnect(uint joystickID, uint connected)
{
    DebugPrint(0, "OnJoystickConnect joystickID = " + joystickID + ", connected = " + connected);
}

void Init()
{
    DebugPrint(0, "Initializing...");

    DebugPrint(0, "Getting default video modes for each monitor...");
    uint monitorCount = GetAvailableMonitorCount();
    DebugPrint(0, "monitorCount = " + monitorCount);
    for (uint i = 0; i < monitorCount; i++)
    {
        MonitorVideoMode monitorVideoMode = GetMonitorVideoMode(i);
        DebugPrint(0, "monitorIndex = " + monitorVideoMode.monitorIndex);
        DebugPrint(0, "width = " + monitorVideoMode.width);
        DebugPrint(0, "height = " + monitorVideoMode.height);
        DebugPrint(0, "redBits = " + monitorVideoMode.redBits);
        DebugPrint(0, "greenBits = " + monitorVideoMode.greenBits);
        DebugPrint(0, "blueBits = " + monitorVideoMode.blueBits);
        DebugPrint(0, "refreshRate = " + monitorVideoMode.refreshRate);
    }

    uint windowID = 0;

    // Create window example
    windowID = WindowCreateWithSize(640, 480, "Nexgen Redux Demo");

    // Create full screen of desired size
    // DebugPrint(0, "Getting video modes for each monitor...");
    // for (uint i = 0; i < monitorCount; i++)
    // {
    //     array<MonitorVideoMode> monitorVideoModes = GetMonitorVideoModes(i);
    //     for (uint j = 0; j < monitorVideoModes.length(); j++)
    //     {
    //         DebugPrint(0, "monitorIndex = " + monitorVideoModes[j].monitorIndex);
    //         DebugPrint(0, "width = " + monitorVideoModes[j].width);
    //         DebugPrint(0, "height = " + monitorVideoModes[j].height);
    //         DebugPrint(0, "redBits = " + monitorVideoModes[j].redBits);
    //         DebugPrint(0, "greenBits = " + monitorVideoModes[j].greenBits);
    //         DebugPrint(0, "blueBits = " + monitorVideoModes[j].blueBits);
    //         DebugPrint(0, "refreshRate = " + monitorVideoModes[j].refreshRate);

    //         if (monitorVideoModes[j].width == 1920 && monitorVideoModes[j].height == 1080)
    //         {
    //             windowID = WindowCreateWithVideoMode(monitorVideoModes[j]);
    //             break;
    //         }
    //     }
    // }

    SetWindowIconifyCallback(OnWindowIconify);
    SetWindowMaximizeCallback(OnWindowMaximize);
    SetWindowSizeCallback(OnWindowSize);
    SetWindowFocusCallback(OnWindowFocus);
    SetWindowKeyboardKeyCallback(OnWindowKeyboardKey);
    SetWindowKeyboardCharacterCallback(OnWindowKeyboardCharacter);
    SetWindowMouseCursorPositionCallback(OnWindowMouseCursorPosition);
    SetWindowMouseCursorEnterCallback(OnWindowMouseCursorEnter);
    SetWindowMouseButtonCallback(OnWindowMouseButton);
    SetWindowMouseScrollCallback(OnWindowMouseScroll);
    SetWindowDropCallback(OnWindowDrop);

    SetJoystickConnectCallback(OnJoystickConnect);

    const uint CursorModeNormal = 0;
    const uint CursorModeHidden = 1;
    const uint CursorModeDisabled = 2;
    const uint CursorModeCaptured = 3;
    SetCursorMode(windowID, CursorModeHidden);

    DebugPrint(1, "Window id = " + windowID);
    // SetWindowBackground(windowID, 0xff00ff);

    //NOTE: since we created a window the program will wait for window to be closed
}

void Render(uint windowID, float dt)
{
    //DebugPrint(1, "Frame Render on window " + windowID + " dt = " + dt);
}
