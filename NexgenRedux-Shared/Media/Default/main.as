OrthoCamera@ orthoCamera = NodeManager::CreateOrthoCamera();
Sprite@ sprite2 = NodeManager::CreateSprite();

void OnWindowIconify(uint iconified)
{
    DebugPrint(0, "OnWindowIconify iconified = " + iconified);
}

void OnWindowMaximize(uint maximized)
{
    DebugPrint(0, "OnWindowMaximize maximized = " + maximized);
}

void OnWindowSize(uint width, uint height)
{
    DebugPrint(0, "OnWindowSize width = " + width + ", height = " + height);
}

void OnWindowFocus(uint focused)
{
    DebugPrint(0, "OnWindowFocus focused = " + focused);
}

void OnWindowKeyboardKey(uint key, uint scancode, uint action, uint modifier)
{
    DebugPrint(0, "OnWindowKeyboardKey key = " + key + ", scancode = " + scancode + ", action = " + action + ", modifier = " + modifier);
}

void OnWindowKeyboardCharacter(uint codepoint)
{
    DebugPrint(0, "OnWindowKeyboardCharacter codepoint = " + codepoint);
}

void OnWindowMouseCursorPosition(double xPos, double yPos)
{
    // commented out to reduce noisey logging
    //DebugPrint(0, "OnWindowMouseCursorPosition xPos = " + xPos + ", yPos = " + yPos);
}

void OnWindowMouseCursorEnter(uint entered)
{
    DebugPrint(0, "OnWindowMouseCursorEnter entered = " + entered);
}

void OnWindowMouseButton(uint button, uint action, uint modifier)
{
    DebugPrint(0, "OnWindowMouseButton button = " + button + ", action = " + action + ", modifier = " + modifier);
}

void OnWindowMouseScroll(double xOffset, double yOffset)
{
    DebugPrint(0, "OnWindowMouseScroll xOffset = " + xOffset + ", yOffset = " + yOffset);
}

void OnWindowDrop(array<string> paths)
{
    for (uint i = 0; i < paths.length(); i++)
    {
        DebugPrint(0, "OnWindowDrop paths" + i + " = " + paths[i]);
    }
}

void OnJoystickConnect(uint joystickID, uint connected)
{
    DebugPrint(0, "OnJoystickConnect joystickID = " + joystickID + ", connected = " + connected);
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

    Fog@ fog = NodeManager::CreateFog();
    fog.SetFog(FogOperationLinear);
    fog.SetFogColor(Color3F(0, 0.6, 0));
    fog.SetFogStart(-1024);
    fog.SetFogEnd(1024);
    fog.SetFogDensity(0.01);
    NodeManager::AssignNode(fog, orthoCamera.GetID());
    
    Lighting@ lighting = NodeManager::CreateLighting();
    lighting.SetLights(LightsOperationDisabled);
    lighting.SetAmbientLight(Color3F(0, 0, 0));
    lighting.SetLight1(LightOperationDisabled);
    lighting.SetLight1Position(Vec3F(0, 0, 0));
    lighting.SetLight1Distance(0);
    lighting.SetLight1Diffuse(Color4F(0, 0, 0, 0));
    NodeManager::AssignNode(lighting, fog.GetID());

    Sprite@ sprite1 = NodeManager::CreateSprite();
    sprite1.SetTexturePath("skin:background.png");
    sprite1.SetUV(RectF(0, 0, 1, 1));
    sprite1.SetSize(SizeF(640, 480));
    NodeManager::AssignNode(sprite1, lighting.GetID());
    
    sprite2.SetTexturePath("skin:sprite.png");
    sprite2.SetUV(RectF(0, 0, 1, 1));
    sprite2.SetAnchor(Vec3F(64, 64, 0));
    sprite2.SetTranslation(Vec3F(74, 74, 10));
    sprite2.SetSize(SizeF(128, 128));
    sprite2.SetTint(Color4F(0.5, 0.9, 0.6, 1.0));
    sprite2.SetBlend(BlendOperationAdd);
    sprite2.SetBlendFactorSrc(BlendFactorSrcAlpha);
    sprite2.SetBlendFactorDst(BlendFactorOneMinusSrcAlpha);
    sprite2.SetDrawMode(DrawModeOperationTriangles);
    sprite2.SetDepth(DepthOperationLess);
    NodeManager::AssignNode(sprite2, lighting.GetID());



    // Sprite@ sprite2 = NodeManager::CreateSprite();
    // sprite2.SetTexturePath("hello2");

    // Sprite@ sprite3 = NodeManager::CreateSprite();
    // sprite3.SetTexturePath("hello2");

    // SceneManager::AssignNode(sprite1, sceneID);
    // sprite1.SetTexturePath("helloxxx");
    // NodeManager::AssignNode(sprite2, sprite1.GetID());
    // NodeManager::AssignNodeAt(sprite3, sprite1.GetID(), sprite2.GetID());

    // Sprite@ sprite1copy = NodeManager::GetNode(sprite1.GetID());
    // sprite1copy.SetTexturePath("helloxxx-copy");


    // NodeManager::DeleteNode(sprite2.GetID());
    // NodeManager::PurgeNodes();
    // NodeManager::CheckForOrphans();

    // uint nodeID1 = nodeManager.AddSceneNode(sprite1, sceneID);

    //RectF newy(rectx);

    //sprite.SetUV(rect);

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
    WindowCreateWithSize(640, 480, "Nexgen Redux Demo");

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
    //             WindowCreateWithVideoMode(monitorVideoModes[j]);
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

    SetCursorMode(CursorModeNormal);

    bool joystickIsPresent = JoystickIsPresent(0);
    DebugPrint(1, "JoystickIsPresent = " + joystickIsPresent);
    bool joystickIsGamepad = JoystickIsGamepad(0);
    DebugPrint(1, "JoystickIsGamepad = " + joystickIsGamepad);
    uint joystickHatCount = GetJoystickHatCount(0);
    DebugPrint(1, "JoystickHatCount = " + joystickHatCount);

    SetClipboardString("hello world");
    // string clipoard = GetClipboardString();
    // DebugPrint(1, "clipoard = " + clipoard);

    SizeI windowSize = GetWindowSize();
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

void Render(double dt)
{
    sprite2.SetRotation(Vec3F(0, 0, spriteRotate));
    spriteRotate += float(dt) * 90;

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
    uint keyPressed = GetKeyPressed(0x20);
    DebugPrintIf(keyPressed > 0, 1, "keyPressed = " + keyPressed);

    uint mouseButtonPressed = GetMouseButtonPressed(0);
    if (mouseButtonPressed == 1)
    {
        Vec2D mousePos = GetMouseCursorPosition();
        if (sprite2.HitTest(mousePos.x, mousePos.y, orthoCamera) == true)
        {
            DebugPrint(1, "sprite clicked");
        }
    }

    frameCount++;
}
