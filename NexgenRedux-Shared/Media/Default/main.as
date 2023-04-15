#include "util.as"

OrthoCameraNode@ orthoCamera = NodeManager::CreateOrthoCamera();
SpriteNode@ sprite2 = NodeManager::CreateSprite();
TextNode@ textFPS = NodeManager::CreateText();
TextNode@ textMem = NodeManager::CreateText();

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
    PrintHello();

    FontManager::LoadFont("skin:freesans.sfn");

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

    FogNode@ fog = NodeManager::CreateFog();
    fog.SetFog(FogOperationDisabled);
    fog.SetFogColor(Color3F(0, 0.6, 0));
    fog.SetFogStart(-1024);
    fog.SetFogEnd(1024);
    fog.SetFogDensity(0.001);
    NodeManager::AssignNode(fog, orthoCamera.GetID());
    
    LightingNode@ lighting = NodeManager::CreateLighting();
    lighting.SetLights(LightsOperationDisabled);
    lighting.SetAmbientLight(Color3F(0, 0, 0));
    lighting.SetLight1(LightOperationDisabled);
    lighting.SetLight1Position(Vec3F(0, 0, 0));
    lighting.SetLight1Distance(0);
    lighting.SetLight1Diffuse(Color4F(0, 0, 0, 0));
    NodeManager::AssignNode(lighting, fog.GetID());

    SpriteNode@ sprite1 = NodeManager::CreateSprite();
    sprite1.SetTexturePath("skin:background.png");
    sprite1.SetUV(RectF(0, 0, 1, 1));
    sprite1.SetSize(SizeF(640, 480));
    NodeManager::AssignNode(sprite1, lighting.GetID());
    
    sprite2.SetTexturePath("skin:sprite.png");
    sprite2.SetUV(RectF(0, 0, 1, 1));
    sprite2.SetAnchor(Vec3F(64, 64, 0));
    sprite2.SetPosition(Vec3F(74, 74, 10));
    sprite2.SetSize(SizeF(128, 128));
    sprite2.SetTint(Color4F(1, 1, 1, 1));
    sprite2.SetBlend(BlendOperationAdd);
    sprite2.SetBlendFactorSrc(BlendFactorSrcAlpha);
    sprite2.SetBlendFactorDst(BlendFactorOneMinusSrcAlpha);
    sprite2.SetDrawMode(DrawModeOperationTriangles);
    NodeManager::AssignNode(sprite2, lighting.GetID());

    textFPS.SetFontName("FreeSans");
    textFPS.SetFontStyle(0);
    textFPS.SetFontSize(60);
    //TODO: SetFontStyle(FontStyleBold | FontStyleItalic | FontStyleUnderline);
    textFPS.SetPosition(Vec3F(75, 360, 20));
    NodeManager::AssignNode(textFPS, lighting.GetID());
    
    textMem.SetFontName("FreeSans");
    textMem.SetFontStyle(0);
    textMem.SetFontSize(30);
    //TODO: SetFontStyle(FontStyleBold | FontStyleItalic | FontStyleUnderline);
    textMem.SetPosition(Vec3F(75, 300, 30));
    NodeManager::AssignNode(textMem, lighting.GetID());
    

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

    // uint nodeID1 = nodeManager.AddSceneNode(sprite1, sceneID);

    //RectF newy(rectx);

    //sprite.SetUV(rect);

    DebugPrint(LogLevelInfo, "Initializing...");

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
    DebugPrint(LogLevelInfo, "JoystickIsPresent = " + joystickIsPresent);
    bool joystickIsGamepad = JoystickIsGamepad(0);
    DebugPrint(LogLevelInfo, "JoystickIsGamepad = " + joystickIsGamepad);
    uint joystickHatCount = GetJoystickHatCount(0);
    DebugPrint(LogLevelInfo, "JoystickHatCount = " + joystickHatCount);

    SetClipboardString("hello world");
    // string clipoard = GetClipboardString();
    // DebugPrint(LogLevelInfo, "clipoard = " + clipoard);

    SizeI windowSize = GetWindowSize();
    DebugPrint(LogLevelInfo, "windowSizeHeight = " + windowSize.width + ", windowSizeHeight = " + windowSize.height);

    Time timeNow = GetTimeNow();
    DebugPrint(LogLevelInfo, "month = " + timeNow.month);
    DebugPrint(LogLevelInfo, "day = " + timeNow.day);
    DebugPrint(LogLevelInfo, "year = " + timeNow.year);
    DebugPrint(LogLevelInfo, "hour = " + timeNow.hour);
    DebugPrint(LogLevelInfo, "minute = " + timeNow.minute);
    DebugPrint(LogLevelInfo, "second = " + timeNow.second);

    uint64 millisecondsNowStart = GetMillisecondsNow();
    SleepMilliseconds(1000);
    uint64 millisecondsNowEnd = GetMillisecondsNow();
    double durationSeconds = GetDurationSeconds(millisecondsNowStart, millisecondsNowEnd);
    DebugPrint(LogLevelInfo, "durationSeconds = " + durationSeconds);

    SeedRandomWithValue(123); // use to get same sequence random values each application run
	SeedRandom(); // use to get random values each application run
	double randomDouble = GetRandomDouble();
    DebugPrint(LogLevelInfo, "randomDouble = " + randomDouble);
	int randomRange = GetRandomIntInRange(-100, 100);
    DebugPrint(LogLevelInfo, "randomRange = " + randomRange);

    // initialize default for fps
    previousTime = GetMillisecondsNow();

    //NOTE: since we created a window the program will wait for window to be closed
}

void Update(double dt)
{
    sprite2.SetRotation(Vec3F(0, 0, spriteRotate));
    spriteRotate += float(dt) * 90;

    currentTime = GetMillisecondsNow();
    double durationFPS = GetDurationSeconds(previousTime, currentTime);
    if (durationFPS > 2.0)
    {
        double fps = CalculateFramesPerSecond(frameCount, durationFPS);
        textFPS.SetText("fps = " + fps);
        DebugPrint(LogLevelInfo, "fps = " + fps);
        frameCount = 0;
        previousTime = currentTime;
    }
    
    textMem.SetText("free mem = " + (GetFreePhysicalMemory() / (1024 * 1024)) + "MB");

    JoystickButtonStates joystickButtonStates = GetJoystickButtonStates(0);
	DebugPrintIf(joystickButtonStates.buttonA == JoystickButtonStatePressed, LogLevelInfo, "buttonA = " + joystickButtonStates.buttonA);
	DebugPrintIf(joystickButtonStates.buttonB == JoystickButtonStatePressed, LogLevelInfo, "buttonB = " + joystickButtonStates.buttonB);
	DebugPrintIf(joystickButtonStates.buttonX == JoystickButtonStatePressed, LogLevelInfo, "buttonX = " + joystickButtonStates.buttonX);
	DebugPrintIf(joystickButtonStates.buttonY == JoystickButtonStatePressed, LogLevelInfo, "buttonY = " + joystickButtonStates.buttonY);
	DebugPrintIf(joystickButtonStates.buttonLeftBumperOrWhite == JoystickButtonStatePressed, LogLevelInfo, "buttonLeftBumperOrWhite = " + joystickButtonStates.buttonLeftBumperOrWhite);
	DebugPrintIf(joystickButtonStates.buttonRightBumperOrBlack == JoystickButtonStatePressed, LogLevelInfo, "buttonRightBumperOrBlack = " + joystickButtonStates.buttonRightBumperOrBlack);
	DebugPrintIf(joystickButtonStates.buttonBack == JoystickButtonStatePressed, LogLevelInfo, "buttonBack = " + joystickButtonStates.buttonBack);
	DebugPrintIf(joystickButtonStates.buttonStart == JoystickButtonStatePressed, LogLevelInfo, "buttonStart = " + joystickButtonStates.buttonStart);
	DebugPrintIf(joystickButtonStates.buttonGuide == JoystickButtonStatePressed, LogLevelInfo, "buttonGuide = " + joystickButtonStates.buttonGuide);
	DebugPrintIf(joystickButtonStates.buttonLeftThumb == JoystickButtonStatePressed, LogLevelInfo, "buttonLeftThumb = " + joystickButtonStates.buttonLeftThumb);
	DebugPrintIf(joystickButtonStates.buttonRightThumb == JoystickButtonStatePressed, LogLevelInfo, "buttonRightThumb = " + joystickButtonStates.buttonRightThumb);
	DebugPrintIf(joystickButtonStates.buttonDpadUp == JoystickButtonStatePressed, LogLevelInfo, "buttonDpadUp = " + joystickButtonStates.buttonDpadUp);
	DebugPrintIf(joystickButtonStates.buttonDpadRight == JoystickButtonStatePressed, LogLevelInfo, "buttonDpadRight = " + joystickButtonStates.buttonDpadRight);
	DebugPrintIf(joystickButtonStates.buttonDpadDown == JoystickButtonStatePressed, LogLevelInfo, "buttonDpadDown = " + joystickButtonStates.buttonDpadDown);
	DebugPrintIf(joystickButtonStates.buttonDpadLeft == JoystickButtonStatePressed, LogLevelInfo, "buttonDpadLeft = " + joystickButtonStates.buttonDpadLeft);

    JoystickAxisStates joystickAxisStates = GetJoystickAxisStates(0);
	DebugPrintIf(abs(joystickAxisStates.axisLeftX) > 0.35, LogLevelInfo, "axisLeftX = " + joystickAxisStates.axisLeftX);
	DebugPrintIf(abs(joystickAxisStates.axisLeftY) > 0.35, LogLevelInfo, "axisLeftY = " + joystickAxisStates.axisLeftY);
	DebugPrintIf(abs(joystickAxisStates.axisRightX) > 0.35, LogLevelInfo, "axisRightX = " + joystickAxisStates.axisRightX);
	DebugPrintIf(abs(joystickAxisStates.axisRightY) > 0.35, LogLevelInfo, "axisRightY = " + joystickAxisStates.axisRightY);
	DebugPrintIf(joystickAxisStates.axisLeftTrigger > -0.65, LogLevelInfo, "axisLeftTrigger = " + joystickAxisStates.axisLeftTrigger);
	DebugPrintIf(joystickAxisStates.axisRightTrigger > -0.65, LogLevelInfo, "axisRightTrigger = " + joystickAxisStates.axisRightTrigger);

    uint joystickHatDirection = GetJoystickHatDirection(0, 0);
    DebugPrintIf(joystickHatDirection > 0, LogLevelInfo, "joystickHatDirection = " + joystickHatDirection);

    // get space key state
    KeyButtonState keyPressed = GetKeyPressed(0x20);
    DebugPrintIf(keyPressed == KeyButtonStatePressed, LogLevelInfo, "keyPressed = " + keyPressed);

    MouseButtonState mouseButtonPressed = GetMouseButtonPressed(0);
    if (mouseButtonPressed == MouseButtonStatePressed)
    {
        Vec2D mousePos = GetMouseCursorPosition();
        if (sprite2.HitTest(mousePos.x, mousePos.y, orthoCamera) == true)
        {
            DebugPrint(LogLevelInfo, "sprite clicked");
        }
    }

    frameCount++;
}
