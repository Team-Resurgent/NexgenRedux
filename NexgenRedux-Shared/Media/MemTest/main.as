OrthoCameraNode@ orthoCamera = NodeManager::CreateOrthoCamera();

void OnWindowIconify(uint iconified)
{
    DebugPrint(LogLevelInfo, "OnWindowIconify iconified = " + iconified);
}

void OnWindowMaximize(uint maximized)
{
    DebugPrint(LogLevelInfo, "OnWindowMaximize maximized = " + maximized);
}

void OnWindowSize(uint width, uint height)
{
    DebugPrint(LogLevelInfo, "OnWindowSize width = " + width + ", height = " + height);
}

void OnWindowFocus(uint focused)
{
    DebugPrint(LogLevelInfo, "OnWindowFocus focused = " + focused);
}

void OnWindowKeyboardKey(uint key, uint scancode, uint action, uint modifier)
{
    DebugPrint(LogLevelInfo, "OnWindowKeyboardKey key = " + key + ", scancode = " + scancode + ", action = " + action + ", modifier = " + modifier);
}

void OnWindowKeyboardCharacter(uint codepoint)
{
    DebugPrint(LogLevelInfo, "OnWindowKeyboardCharacter codepoint = " + codepoint);
}

void OnWindowMouseCursorPosition(double xPos, double yPos)
{
    // commented out to reduce noisey logging
    //DebugPrint(LogLevelInfo, "OnWindowMouseCursorPosition xPos = " + xPos + ", yPos = " + yPos);
}

void OnWindowMouseCursorEnter(uint entered)
{
    DebugPrint(LogLevelInfo, "OnWindowMouseCursorEnter entered = " + entered);
}

void OnWindowMouseButton(uint button, uint action, uint modifier)
{
    DebugPrint(LogLevelInfo, "OnWindowMouseButton button = " + button + ", action = " + action + ", modifier = " + modifier);
}

void OnWindowMouseScroll(double xOffset, double yOffset)
{
    DebugPrint(LogLevelInfo, "OnWindowMouseScroll xOffset = " + xOffset + ", yOffset = " + yOffset);
}

void OnWindowDrop(array<string> paths)
{
    for (uint i = 0; i < paths.length(); i++)
    {
        DebugPrint(LogLevelInfo, "OnWindowDrop paths" + i + " = " + paths[i]);
    }
}

void OnJoystickConnect(uint joystickID, uint connected)
{
    DebugPrint(LogLevelInfo, "OnJoystickConnect joystickID = " + joystickID + ", connected = " + connected);
}

uint64 currentTime;
uint64 previousTime;
uint frameCount;
float spriteRotate = 0;

void Init()
{
    uint sceneID = SceneManager::CreateScene(true);
    SceneManager::SetCurrentScene(sceneID);

    orthoCamera.SetClearColor(Color4F(0, 0, 0, 1));
    orthoCamera.SetEye(Vec3F(0, 0, 50));
    orthoCamera.SetTarget(Vec3F(0, 0, 0));
    orthoCamera.SetUp(Vec3F(0, 1, 0));
    orthoCamera.SetLeft(0);
    orthoCamera.SetRight(640);
    orthoCamera.SetBottom(0);
    orthoCamera.SetTop(480);
    orthoCamera.SetZNear(-1024);
    orthoCamera.SetZFar(1024);

    SceneManager::AssignNode(orthoCamera, sceneID);

    SpriteNode@ sprite1 = NodeManager::CreateSprite();
    sprite1.SetTexturePath("skin:background.png");
    sprite1.SetUV(RectF(0, 0, 1, 1));
    sprite1.SetSize(SizeF(320, 240));
    sprite1.SetPosition(Vec3F(0, 0, 0));
    NodeManager::AssignNode(sprite1, orthoCamera.GetID());

    SpriteNode@ sprite2 = NodeManager::CreateSprite();
    sprite2.SetTexturePath("skin:background.png");
    sprite2.SetUV(RectF(0, 0, 1, 1));
    sprite2.SetSize(SizeF(320, 240));
    sprite2.SetPosition(Vec3F(320, 240, 0));
    NodeManager::AssignNode(sprite2, orthoCamera.GetID());
    
    DebugPrint(LogLevelInfo, "Getting default video modes for each monitor...");
    uint monitorCount = GetAvailableMonitorCount();
    DebugPrint(LogLevelInfo, "monitorCount = " + monitorCount);
    for (uint i = 0; i < monitorCount; i++)
    {
        MonitorVideoMode monitorVideoMode = GetMonitorVideoMode(i);
        DebugPrint(LogLevelInfo, "monitorIndex = " + monitorVideoMode.monitorIndex);
        DebugPrint(LogLevelInfo, "width = " + monitorVideoMode.width);
        DebugPrint(LogLevelInfo, "height = " + monitorVideoMode.height);
        DebugPrint(LogLevelInfo, "redBits = " + monitorVideoMode.redBits);
        DebugPrint(LogLevelInfo, "greenBits = " + monitorVideoMode.greenBits);
        DebugPrint(LogLevelInfo, "blueBits = " + monitorVideoMode.blueBits);
        DebugPrint(LogLevelInfo, "refreshRate = " + monitorVideoMode.refreshRate);
    }

    // Create window example
    WindowCreateWithSize(640, 480, "Nexgen Redux Demo");

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

    SetCursorMode(CursorModeNormal);

    // initialize default for fps
    previousTime = GetMillisecondsNow();
}

void Update(double dt)
{
    currentTime = GetMillisecondsNow();
    double durationFPS = GetDurationSeconds(previousTime, currentTime);
    if (durationFPS > 2.0)
    {
        double fps = CalculateFramesPerSecond(frameCount, durationFPS);
        DebugPrint(LogLevelInfo, "fps = " + fps);
        frameCount = 0;
        previousTime = currentTime;
    }

    frameCount++;
}
