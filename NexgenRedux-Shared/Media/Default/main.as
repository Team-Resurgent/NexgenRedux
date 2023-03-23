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
    DebugPrint(0, "OnWindowMouseCursorPosition windowHandle = " + windowHandle + ", xPos = " + xPos + ", yPos = " + yPos);
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

uint windowID = 0;

uint64 currentTime;
uint64 previousTime;
uint frameCount;

void Init()
{


    uint sceneID = SceneManager::CreateScene(true);
    SceneManager::SetCurrentScene(sceneID);

    NodeSprite nodeSprite1 = NodeManager::CreateSprite();
    nodeSprite1.SetTexturePath("hello1");

    NodeSprite nodeSprite2 = NodeManager::CreateSprite();
    nodeSprite2.SetTexturePath("hello2");

    NodeSprite nodeSprite3 = NodeManager::CreateSprite();
    nodeSprite3.SetTexturePath("hello2");

    SceneManager::AssignNode(nodeSprite1.ToNode(), sceneID);
    NodeManager::AssignNode(nodeSprite2.ToNode(), nodeSprite1.GetID());
    NodeManager::AssignNodeAt(nodeSprite3.ToNode(), nodeSprite1.GetID(), nodeSprite2.GetID());

    NodeManager::DeleteNode(nodeSprite2.GetID());
    NodeManager::PurgeNodes();


    // uint nodeID1 = nodeManager.AddSceneNode(nodeSprite1, sceneID);

    //RectF newy(rectx);

    //nodeSprite.SetUV(rect);

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

    bool joystickIsPresent = JoystickIsPresent(0);
    DebugPrint(1, "JoystickIsPresent = " + joystickIsPresent);
    bool joystickIsGamepad = JoystickIsGamepad(0);
    DebugPrint(1, "JoystickIsGamepad = " + joystickIsGamepad);
    uint joystickHatCount = GetJoystickHatCount(0);
    DebugPrint(1, "JoystickHatCount = " + joystickHatCount);

    SetClipboardString("hello world");
    // string clipoard = GetClipboardString();
    // DebugPrint(1, "clipoard = " + clipoard);

    DebugPrint(1, "Window id = " + windowID);
    // SetWindowBackground(windowID, 0xff00ff);

    SizeI windowSize = GetWindowSize(windowID);
    DebugPrint(1, "windowSizeHeight = " + windowSize.width + ", windowSizeHeight = " + windowSize.height);

    Time timeNow = GetTimeNow();
    DebugPrint(1, "month = " + timeNow.month);
    DebugPrint(1, "day = " + timeNow.day);
    DebugPrint(1, "year = " + timeNow.year);
    DebugPrint(1, "hour = " + timeNow.hour);
    DebugPrint(1, "minute = " + timeNow.minute);
    DebugPrint(1, "second = " + timeNow.second);

    uint64 millisecondsNowStart = GetMillisecondsNow();
    SleepMilliseconds(1000);
    uint64 millisecondsNowEnd = GetMillisecondsNow();
    double durationSeconds = GetDurationSeconds(millisecondsNowStart, millisecondsNowEnd);
    DebugPrint(1, "durationSeconds = " + durationSeconds);

    SeedRandomWithValue(123); // use to get same sequence random values each application run
	SeedRandom(); // use to get random values each application run
	double randomDouble = GetRandomDouble();
    DebugPrint(1, "randomDouble = " + randomDouble);
	int randomRange = GetRandomIntInRange(-100, 100);
    DebugPrint(1, "randomRange = " + randomRange);

    // initialize default for fps
    previousTime = GetMillisecondsNow();

    //NOTE: since we created a window the program will wait for window to be closed
}

void Render(uint windowID, double dt)
{
    //DebugPrint(1, "Frame Render on window " + windowID + " dt = " + dt);

    currentTime = GetMillisecondsNow();
    double durationFPS = GetDurationSeconds(previousTime, currentTime);
    if (durationFPS > 2.0)
    {
        double fps = CalculateFramesPerSecond(frameCount, durationFPS);
        DebugPrint(1, "fps = " + fps);
        frameCount = 0;
        previousTime = currentTime;
    }

    JoystickButtonStates joystickButtonStates = GetJoystickButtonStates(0);
	DebugPrintIf(joystickButtonStates.buttonA == 1, 1, "buttonA = " + joystickButtonStates.buttonA);
	DebugPrintIf(joystickButtonStates.buttonB == 1, 1, "buttonB = " + joystickButtonStates.buttonB);
	DebugPrintIf(joystickButtonStates.buttonX == 1, 1, "buttonX = " + joystickButtonStates.buttonX);
	DebugPrintIf(joystickButtonStates.buttonY == 1, 1, "buttonY = " + joystickButtonStates.buttonY);
	DebugPrintIf(joystickButtonStates.buttonLeftBumperOrWhite == 1, 1, "buttonLeftBumperOrWhite = " + joystickButtonStates.buttonLeftBumperOrWhite);
	DebugPrintIf(joystickButtonStates.buttonRightBumperOrBlack == 1, 1, "buttonRightBumperOrBlack = " + joystickButtonStates.buttonRightBumperOrBlack);
	DebugPrintIf(joystickButtonStates.buttonBack == 1, 1, "buttonBack = " + joystickButtonStates.buttonBack);
	DebugPrintIf(joystickButtonStates.buttonStart == 1, 1, "buttonStart = " + joystickButtonStates.buttonStart);
	DebugPrintIf(joystickButtonStates.buttonGuide == 1, 1, "buttonGuide = " + joystickButtonStates.buttonGuide);
	DebugPrintIf(joystickButtonStates.buttonLeftThumb == 1, 1, "buttonLeftThumb = " + joystickButtonStates.buttonLeftThumb);
	DebugPrintIf(joystickButtonStates.buttonRightThumb == 1, 1, "buttonRightThumb = " + joystickButtonStates.buttonRightThumb);
	DebugPrintIf(joystickButtonStates.buttonDpadUp == 1, 1, "buttonDpadUp = " + joystickButtonStates.buttonDpadUp);
	DebugPrintIf(joystickButtonStates.buttonDpadRight == 1, 1, "buttonDpadRight = " + joystickButtonStates.buttonDpadRight);
	DebugPrintIf(joystickButtonStates.buttonDpadDown == 1, 1, "buttonDpadDown = " + joystickButtonStates.buttonDpadDown);
	DebugPrintIf(joystickButtonStates.buttonDpadLeft == 1, 1, "buttonDpadLeft = " + joystickButtonStates.buttonDpadLeft);

    JoystickAxisStates joystickAxisStates = GetJoystickAxisStates(0);
	DebugPrintIf(abs(joystickAxisStates.axisLeftX) > 0.35, 1, "axisLeftX = " + joystickAxisStates.axisLeftX);
	DebugPrintIf(abs(joystickAxisStates.axisLeftY) > 0.35, 1, "axisLeftY = " + joystickAxisStates.axisLeftY);
	DebugPrintIf(abs(joystickAxisStates.axisRightX) > 0.35, 1, "axisRightX = " + joystickAxisStates.axisRightX);
	DebugPrintIf(abs(joystickAxisStates.axisRightY) > 0.35, 1, "axisRightY = " + joystickAxisStates.axisRightY);
	DebugPrintIf(joystickAxisStates.axisLeftTrigger > -0.65, 1, "axisLeftTrigger = " + joystickAxisStates.axisLeftTrigger);
	DebugPrintIf(joystickAxisStates.axisRightTrigger > -0.65, 1, "axisRightTrigger = " + joystickAxisStates.axisRightTrigger);

    uint joystickHatDirection = GetJoystickHatDirection(0, 0);
    DebugPrintIf(joystickHatDirection > 0, 1, "joystickHatDirection = " + joystickHatDirection);

    // get space key state
    uint keyPressed = GetKeyPressed(windowID, 0x20);
    DebugPrintIf(keyPressed > 0, 1, "keyPressed = " + keyPressed);

    uint mouseButtonPressed = GetMouseButtonPressed(windowID, 0);
    DebugPrintIf(mouseButtonPressed > 0, 1, "mouseButtonPressed = " + mouseButtonPressed);

    //Vec2D mousePosition = GetMouseCursorPosition(windowID);
    //DebugPrint(1, "mousePositionX = " + mousePosition.x + ", mousePositionY = " + mousePosition.y);

    // set mouse position 10 pixels from top corner
    //SetMouseCursorPosition(windowID, 10, 10);

    frameCount++;
}
