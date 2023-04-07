OrthoCameraNode@  orthoCamera = NodeManager::CreateOrthoCamera();
FogNode@ fog = NodeManager::CreateFog();
LightingNode@ lighting = NodeManager::CreateLighting();

TextNode@ textFPS = NodeManager::CreateText();
TextNode@ textMem = NodeManager::CreateText();

TextNode@ textP1Score = NodeManager::CreateText();

SpriteNode@ PlayerImg = NodeManager::CreateSprite();
SpriteNode@ BulletImg = NodeManager::CreateSprite();
SpriteNode@ EnemyBulletImg = NodeManager::CreateSprite();
SpriteNode@ EnemyImg = NodeManager::CreateSprite();
SpriteNode@ BossImg = NodeManager::CreateSprite();



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
    //DebugPrint(LogLevelInfo, "OnWindowMouseCursorEnter entered = " + entered);
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
uint64 previousTickTime;

uint frameCount;
float spriteRotate = 0;

//************************************************************************************************ 
// -- Image Width & Height
// --************************************************************************************************ 

int bulletWidth = 5;
int bulletHeight = 13;


// --------------------------------------------------------------------------------------------------

// Declare a global variable windowSize
SizeI windowSize;

// Define a function to initialize the windowSize variable
void InitWindowSize()
{
    // Call the GetWindowSize() function and assign the result to the windowSize variable
    windowSize = GetWindowSize();
}

//************************************************************************************************ 
// Table For Player
//************************************************************************************************

int playerWidth = 55;
int playerHeight = 43;

int P1Score = 0;

Vec2F PlayerPos = Vec2F();
float PlayerSpeed = 400;

Vec3F PlayerRotation = Vec3F(0,0,0);
float PlayerRotationSpeed = 250;

uint64 PlayerLastBulletFiredTime = 0;
uint64 PlayerBulletsLastUpdatedTime = 0;
float PlayerBulletSpeed = 0.0125;
array<int> PlayerBullets = {0};


//************************************************************************************************ 
// Table For Enemy
//************************************************************************************************

int enemyWidth = 55;
int enemyHeight = 43;

Vec2F EnemyPos = Vec2F();
float EnemySpeed = PlayerSpeed / 2;

uint64 EnemyLastBulletFiredTime = 0;

array<int> EnemiesSpawned = {0};

//************************************************************************************************ 
// Table For Boss Enemy
//************************************************************************************************

int bossWidth = 94;
int bossHeight = 76;

bool BossEnemySpawned = false;

Vec2F BossEnemyPos = Vec2F();
float BossEnemySpeed = PlayerSpeed / 2;

uint64 BossEnemyLastBulletFiredTime = 0;


//************************************************************************************************ 
// Main Function: Initialise all main assets
//************************************************************************************************
void Init()
{
    FontManager::LoadFont("skin:asset\\fonts\\freesans.sfn");

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

    fog.SetFog(FogOperationDisabled);
    fog.SetFogColor(Color3F(0, 0.6, 0));
    fog.SetFogStart(-1024);
    fog.SetFogEnd(1024);
    fog.SetFogDensity(0.001);
    NodeManager::AssignNode(fog, orthoCamera.GetID());
    
    lighting.SetLights(LightsOperationDisabled);
    lighting.SetAmbientLight(Color3F(0, 0, 0));
    lighting.SetLight1(LightOperationDisabled);
    lighting.SetLight1Position(Vec3F(0, 0, 0));
    lighting.SetLight1Distance(0);
    lighting.SetLight1Diffuse(Color4F(0, 0, 0, 0));
    NodeManager::AssignNode(lighting, fog.GetID());

    SpriteNode@ sprite1 = NodeManager::CreateSprite();
    sprite1.SetTexturePath("asset:images\\backgrounds\\bg.png");
    sprite1.SetUV(RectF(0, 0, 1, 1));
    sprite1.SetSize(SizeF(640, 480));
    NodeManager::AssignNode(sprite1, lighting.GetID());
    
    PlayerImg.SetTexturePath("asset:images\\game\\plane.png");
    PlayerImg.SetUV(RectF(0, 0, 1, 1));
    PlayerImg.SetAnchor(Vec3F(50, 50, 0));
    PlayerImg.SetSize(SizeF(playerWidth, playerHeight));
	PlayerImg.SetDepth(DepthOperationDisabled);
	//PlayerImg.SetVisible(false);
    NodeManager::AssignNode(PlayerImg, lighting.GetID());
	
	BulletImg.SetTexturePath("asset:images\\game\\playerbullet.png");
    BulletImg.SetUV(RectF(0, 0, 1, 1));
    BulletImg.SetAnchor(Vec3F(0, 0, 0));
    BulletImg.SetPosition(Vec3F(74, 74, 0));
    BulletImg.SetSize(SizeF(bulletWidth, bulletHeight));
	BulletImg.SetDepth(DepthOperationDisabled);
	BulletImg.SetVisible(false);
    NodeManager::AssignNode(BulletImg, lighting.GetID());
	
	EnemyBulletImg.SetTexturePath("asset:images\\game\\enemybullet.png");
    EnemyBulletImg.SetUV(RectF(0, 0, 1, 1));
    EnemyBulletImg.SetAnchor(Vec3F(64, 64, 0));
    EnemyBulletImg.SetPosition(Vec3F(74, 96, 0));
    EnemyBulletImg.SetSize(SizeF(bulletWidth, bulletHeight));
	EnemyBulletImg.SetDepth(DepthOperationDisabled);
	EnemyBulletImg.SetVisible(false);
    NodeManager::AssignNode(EnemyBulletImg, lighting.GetID());
	
	EnemyImg.SetTexturePath("asset:images\\game\\enemy.png");
    EnemyImg.SetUV(RectF(0, 0, 1, 1));
    EnemyImg.SetAnchor(Vec3F(64, 64, 0));
    EnemyImg.SetPosition(Vec3F(200, 106, 0));
    EnemyImg.SetSize(SizeF(enemyWidth, enemyHeight));
	EnemyImg.SetDepth(DepthOperationDisabled);
	EnemyImg.SetVisible(false);
    NodeManager::AssignNode(EnemyImg, lighting.GetID());
	
	BossImg.SetTexturePath("asset:images\\game\\boss.png");
    BossImg.SetUV(RectF(0, 0, 1, 1));
    BossImg.SetAnchor(Vec3F(64, 64, 0));
    BossImg.SetSize(SizeF(bossWidth, bossWidth));
    BossImg.SetDepth(DepthOperationDisabled);
	BossImg.SetVisible(false);
    NodeManager::AssignNode(BossImg, lighting.GetID());
	


    textFPS.SetFontName("FreeSans");
    textFPS.SetFontStyle(0);
    textFPS.SetFontSize(20);
    //TODO: SetFontStyle(FontStyleBold | FontStyleItalic | FontStyleUnderline);
    textFPS.SetPosition(Vec3F(50, 455, 20));
    NodeManager::AssignNode(textFPS, lighting.GetID());
    
    textMem.SetFontName("FreeSans");
    textMem.SetFontStyle(0);
    textMem.SetFontSize(20);
    //TODO: SetFontStyle(FontStyleBold | FontStyleItalic | FontStyleUnderline);
    textMem.SetPosition(Vec3F(75, 455, 20));
    NodeManager::AssignNode(textMem, lighting.GetID());
	
	textP1Score.SetFontName("FreeSans");
    textP1Score.SetFontStyle(0);
    textP1Score.SetFontSize(20);
    //TODO: SetFontStyle(FontStyleBold | FontStyleItalic | FontStyleUnderline);
    textP1Score.SetPosition(Vec3F(480, 455, 20));
    NodeManager::AssignNode(textP1Score, lighting.GetID());
    

    DebugPrint(LogLevelInfo, "Initializing...");

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
	
	InitWindowSize();

//--------------------------------------------------------------------------------------------------		
	
    //Initialise Tick for restricting render based updates to time based intervals
    previousTickTime = GetMillisecondsNow();
	
	PlayerPos.x = windowSize.width / 2 - playerWidth /2;
	PlayerPos.y = windowSize.height - playerHeight;
    PlayerImg.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
}

void Render(double dt)
{

//************************************************************************************************ 
// Fps & Mem & Score Counters
//************************************************************************************************ 
    currentTime = GetMillisecondsNow();
    double durationFPS = GetDurationSeconds(previousTickTime, currentTime);

    if (durationFPS > 0.1)
    {
        double fps = CalculateFramesPerSecond(frameCount, durationFPS);
        textFPS.SetText("fps = " + fps);
        //DebugPrint(LogLevelInfo, "fps = " + fps);
        frameCount = 0;
        previousTickTime = currentTime;
    }
    
    //textMem.SetText("free mem = " + (GetFreePhysicalMemory() / (1024 * 1024)) + "MB");
	
	textP1Score.SetText("P1 Score = " + P1Score); 

//-------------------------------------------------------------------------------------------------- 





//************************************************************************************************ 
// Airplane Movement + Firing Bullets
//************************************************************************************************ 

//Controller 1 Controller States
JoystickAxisStates joystickAxisStates = GetJoystickAxisStates(0);
JoystickButtonStates joystickButtonStates = GetJoystickButtonStates(0);

//Keyboard/Mouse Controls (1 Player only):
MouseButtonState LeftMouseClicked = GetMouseButtonPressed(0);

KeyButtonState SpaceBarPressed = GetKeyPressed(32);

KeyButtonState UpArrowPressed = GetKeyPressed(265);
KeyButtonState DownArrowPressed = GetKeyPressed(264);
KeyButtonState LeftArrowPressed = GetKeyPressed(263);
KeyButtonState RightArrowPressed = GetKeyPressed(262);

KeyButtonState WArrowPressed = GetKeyPressed(87);
KeyButtonState SArrowPressed = GetKeyPressed(83);
KeyButtonState AArrowPressed = GetKeyPressed(65);
KeyButtonState DArrowPressed = GetKeyPressed(68);

//-------------------------------------------------------------------------------------------------- 

//Move Player 1 Around screen

if (WArrowPressed == 1 || UpArrowPressed == 1 || joystickButtonStates.buttonDpadUp == JoystickButtonStatePressed || joystickAxisStates.axisLeftY < -0.2) {
    // Move up
    if ( PlayerPos.y < windowSize.height - bossHeight) {
        PlayerPos.y = float(PlayerPos.y + (PlayerSpeed * dt));
        PlayerImg.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
    }
    else {
        PlayerPos.y = windowSize.height - bossHeight;
    }
}
if (SArrowPressed == 1 || DownArrowPressed == 1 || joystickButtonStates.buttonDpadDown == JoystickButtonStatePressed || joystickAxisStates.axisLeftY > 0.2) {
    // Move down
    if (PlayerPos.y > 0) {
        PlayerPos.y = float(PlayerPos.y - (PlayerSpeed * dt));
        PlayerImg.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
    }
    else {
        PlayerPos.y = 0;
    }
}
if (AArrowPressed == 1 || LeftArrowPressed == 1 || joystickButtonStates.buttonDpadLeft == JoystickButtonStatePressed || joystickAxisStates.axisLeftX < -0.2) {
    // Move left
    if (PlayerPos.x > 0) {
        PlayerPos.x = float(PlayerPos.x - (PlayerSpeed * dt));
        PlayerImg.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
    }
    else {
        PlayerPos.x = 0;
    }
}
if (DArrowPressed == 1 || RightArrowPressed == 1 || joystickButtonStates.buttonDpadRight == JoystickButtonStatePressed || joystickAxisStates.axisLeftX > 0.2) {
    // Move right
    if (PlayerPos.x < windowSize.width - bossWidth) {
        PlayerPos.x = float(PlayerPos.x + (PlayerSpeed * dt));
        PlayerImg.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
    }
    else {
        PlayerPos.x = windowSize.width - bossWidth;
    }
}

//Rotate Player 1 Around Screen

if (joystickAxisStates.axisRightX < -0.2) {
    // Rotate left
    PlayerRotation = (Vec3F(PlayerRotation.x, PlayerRotation.y, PlayerRotation.z - PlayerRotationSpeed * dt));
    PlayerImg.SetRotation(PlayerRotation);
}
if (joystickAxisStates.axisRightX > 0.2) {
    // Rotate right
    PlayerRotation = (Vec3F(PlayerRotation.x, PlayerRotation.y, PlayerRotation.z + PlayerRotationSpeed * dt));
    PlayerImg.SetRotation(PlayerRotation);
}

//Fire Player 1's Primary Weapon

if (LeftMouseClicked == 1 || SpaceBarPressed == 1 || joystickButtonStates.buttonA == JoystickButtonStatePressed || joystickAxisStates.axisRightTrigger > 0.1)
{
	currentTime = GetMillisecondsNow();
    double durationPlayerBulletLastFiredToNow = GetDurationSeconds(PlayerLastBulletFiredTime, currentTime);
	if (durationPlayerBulletLastFiredToNow > 0.250)
	{	
		FirePlayerBullet();
		PlayerLastBulletFiredTime = currentTime;
	}
	
}


//************************************************************************************************ 
// Update Player1's Bullet locations on screen
//************************************************************************************************ 

	uint i = 0;
	currentTime = GetMillisecondsNow();
	double SecondsSinceLastBulletUpdate = GetDurationSeconds(PlayerBulletsLastUpdatedTime, currentTime);
	if (SecondsSinceLastBulletUpdate > PlayerBulletSpeed)
	{
		while(i < PlayerBullets.length())
		{		
			SpriteNode@ node = NodeManager::GetNode(PlayerBullets[i]);
		
			if (node !is null)
			{
				Vec3F playerBulletPositionData = node.GetPosition();

				//************************************************************************************************ 
				// Detect if Player Hits Enemies
				//************************************************************************************************

				uint EnemiesHitChecked = 0;
				bool bulletHitEnemy = false;
				while(!bulletHitEnemy && (EnemiesHitChecked < EnemiesSpawned.length()))
				{	
					SpriteNode@ enemyNode = NodeManager::GetNode(EnemiesSpawned[EnemiesHitChecked]);
	
					if (enemyNode !is null)
					{
						Vec3F enemyNodePositionData = enemyNode.GetPosition();
						if(CheckCollision(enemyNodePositionData.x, enemyNodePositionData.y, enemyWidth, enemyHeight, playerBulletPositionData.x,playerBulletPositionData.y, bulletWidth, bulletHeight))
						{
							NodeManager::DeleteNode(EnemiesSpawned[EnemiesHitChecked]);
							EnemiesSpawned.removeAt(EnemiesHitChecked);
							NodeManager::DeleteNode(PlayerBullets[i]);
							PlayerBullets.removeAt(i);
							P1Score++;
							bulletHitEnemy = true;
						}
					}
					EnemiesHitChecked++;
				}
				//------------------------------------------------------------------------------------------------

				//************************************************************************************************ 
				// Move Bullet if it wasn't hit
				//************************************************************************************************
				if(!bulletHitEnemy)
				{
					if (playerBulletPositionData.y > -30)
					{
						node.SetPosition(Vec3F(playerBulletPositionData.x, playerBulletPositionData.y - 2 , 0));
					}
					else
					{
						NodeManager::DeleteNode(PlayerBullets[i]);
					}
				}
			}
			i++;
		}
		
		PlayerBulletsLastUpdatedTime = GetMillisecondsNow();	

	}


//------------------------------------------------------------------------------------------------ 







//************************************************************************************************ 
// Spawn Enemies!
//************************************************************************************************

	if ((EnemiesSpawned.length() < 2 && P1Score < 5) || (EnemiesSpawned.length() < 3 && P1Score >= 5) || (EnemiesSpawned.length() < 5 && P1Score >= 10))
	{
		SpawnNewEnemy(false);
	}
 

//------------------------------------------------------------------------------------------------

    frameCount++;
	
}



//************************************************************************************************ 
// Create a Bullet for Player and Play Sound (Sound not supported by engine yet)
//************************************************************************************************ 
void FirePlayerBullet()
{	
	
	float bulletXPosition = PlayerPos.x + 25;
	float bulletYPosition = PlayerPos.y - 10;
	
	
    SpriteNode@ BulletImg2 = NodeManager::CreateSprite();
	BulletImg2.SetTexturePath("asset:images\\game\\playerbullet.png");
	BulletImg2.SetUV(RectF(0, 0, 1, 1));
	BulletImg2.SetAnchor(Vec3F(0, 0, 0));
	BulletImg2.SetPosition(Vec3F(bulletXPosition, bulletYPosition, 0));
	BulletImg2.SetSize(SizeF(bulletWidth, bulletHeight));
	BulletImg2.SetDepth(DepthOperationDisabled);
	//BulletImg2.SetVisible(true);	
	NodeManager::AssignNode(BulletImg2, lighting.GetID());	

	PlayerBullets.insertLast(BulletImg2.GetID());	
}



//************************************************************************************************ 
// Spawn in a new Enemy (Regular or Boss)
//************************************************************************************************ 
void SpawnNewEnemy(bool isBoss)
{	
    SpriteNode@ EnemySprite = NodeManager::CreateSprite();

	if (isBoss && !BossEnemySpawned)
	{
		EnemySprite.SetTexturePath("asset:images\\game\\boss.png");
		EnemySprite.SetSize(SizeF(bossWidth, bossHeight));		
	}
	else if (!isBoss && EnemiesSpawned.length() < 10)
	{
		EnemySprite.SetTexturePath("asset:images\\game\\enemy.png");
		EnemySprite.SetSize(SizeF(enemyWidth, enemyHeight));
	}
	
	EnemySprite.SetUV(RectF(0, 0, 1, 1));
	EnemySprite.SetAnchor(Vec3F(0, 0, 0));
	
	float RandEnemyXPosition = GetRandomIntInRange(30, windowSize.width - 30);	
	EnemySprite.SetPosition(Vec3F(RandEnemyXPosition, 15, 0));
	
	EnemySprite.SetDepth(DepthOperationDisabled);
	//BulletImg2.SetVisible(true);	
	NodeManager::AssignNode(EnemySprite, lighting.GetID());	
	
	if (!isBoss)
	{
		EnemiesSpawned.insertLast(EnemySprite.GetID());	
	}
}

//************************************************************************************************ 
// Spawn in a new Enemy (Regular or Boss)
//************************************************************************************************ 

// Collision detection taken function from http://love2d.org/wiki/BoundingBox.lua
// Returns true if two boxes overlap, false if they don't
// x1,y1 are the left-top coords of the first box, while w1,h1 are its width and height
// x2,y2,w2 & h2 are the same, but for the second box

bool CheckCollision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2)
{
    return x1 < x2 + w2 && x2 < x1 + w1 && y1 < y2 + h2 && y2 < y1 + h1;
}

//------------------------------------------------------------------------------------------------
