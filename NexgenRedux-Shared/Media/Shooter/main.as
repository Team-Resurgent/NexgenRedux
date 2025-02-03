OrthoCameraNode@  orthoCamera = NodeManager::CreateOrthoCamera();
FogNode@ fog = NodeManager::CreateFog();
LightingNode@ lighting = NodeManager::CreateLighting();

SpriteNode@ Parallax1a = NodeManager::CreateSprite();
SpriteNode@ Parallax1b = NodeManager::CreateSprite();
SpriteNode@ Parallax2a = NodeManager::CreateSprite();
SpriteNode@ Parallax2b = NodeManager::CreateSprite();
SpriteNode@ Parallax3a = NodeManager::CreateSprite();
SpriteNode@ Parallax3b = NodeManager::CreateSprite();

TextNode@ textFPS = NodeManager::CreateText();
TextNode@ textMem = NodeManager::CreateText();

TextNode@ textP1Score = NodeManager::CreateText();

uint64 currentTime;
uint64 previousTickTime;

uint frameCount;

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

//************************************************************************************************ 
// -- Image Width & Height
// --************************************************************************************************ 

// Declare a global variable windowSize
SizeI windowSize;

// Define a function to initialize the windowSize variable
void InitWindowSize()
{
    // Call the GetWindowSize() function and assign the result to the windowSize variable
    windowSize = GetWindowSize();
}

//************************************************************************************************ 
// Initial Loading Image Fader
//************************************************************************************************

float Transparency = 0;
uint TransparencyLastUpdatedTime = 0;
bool FadeInProgress = false;
bool FadedIn = false;
bool FadedOut = false;

SpriteNode@ LoadingImg = NodeManager::CreateSprite();

void InitializeLoadingImage()
{
	LoadingImg.SetTexturePath("asset:images\\TR_Banner_Background.jpg");
    LoadingImg.SetUV(RectF(0, 0, 1, 1));
    LoadingImg.SetSize(SizeF(640, 480));
	LoadingImg.SetTint(Color4F(1, 1, 1, Transparency));
    NodeManager::AssignNode(LoadingImg, lighting.GetID());
}
	
void FadeImage()
{
	// currentTime = GetMillisecondsNow();
	// double SecondsSinceLastTransparencyUpdate = GetDurationSeconds(TransparencyLastUpdatedTime, currentTime);
	// if (SecondsSinceLastTransparencyUpdate > 0.0175) //Update Speed
	// {
		// if(Transparency <= 1 && !FadedIn)
		// {
			// Transparency += 0.01;
			// LoadingImg.SetTint(Color4F(1, 1, 1, Transparency));
			// if (Transparency >= 1)
			// {
				// FadedIn = true;
			// }

		// }
		// else if (Transparency >= 0 && !FadedOut)
		// {
			// Transparency -= 0.01;
			// LoadingImg.SetTint(Color4F(1, 1, 1, Transparency));
			// if (Transparency <= 0)
			// {
				// FadedOut = true;
				// FadeInProgress = false;
				// NodeManager::DeleteNode(LoadingImg.GetID());
			// }
		// }
		
		// TransparencyLastUpdatedTime = GetMillisecondsNow();
		//DebugPrint(LogLevelInfo, "Transparency = " + Transparency);
	// }
}

//************************************************************************************************ 
// Class for Bullet
//************************************************************************************************

uint64 LastBulletFiredTime = 0;
uint64 BulletsLastUpdatedTime = 0;

class Bullet
{
	int NodeID = 0;

	float Width = 5;
	float Height = 10;
	
	float Speed = 5;

    void UpdatePlayerBulletPosition()
    {
		//************************************************************************************************ 
		// Detect if Player Hits Enemies
		//************************************************************************************************
		
		SpriteNode@ bulletNode = NodeManager::GetNode(NodeID);
		
		if (bulletNode is null)
		{
			return;
		}
		Vec3F playerBulletPositionData = bulletNode.GetPosition();

		uint EnemiesHitChecked = 0;
		bool bulletHitEnemy = false;
		while(!bulletHitEnemy && (EnemiesHitChecked < EnemiesSpawned.length()))
		{	
			Enemy@ currentEnemy = EnemiesSpawned[EnemiesHitChecked];
			SpriteNode@ enemyNode = NodeManager::GetNode(currentEnemy.Sprite.GetID());
	
			if (enemyNode !is null && currentEnemy !is null)
			{
				Vec3F enemyNodePositionData = enemyNode.GetPosition();
				if(CheckCollision(enemyNodePositionData.x, enemyNodePositionData.y, currentEnemy.Width, currentEnemy.Height, playerBulletPositionData.x,playerBulletPositionData.y, Width, Height))
				{
					NodeManager::DeleteNode(currentEnemy.Sprite.GetID());
					int EnemyBulletsRemoved = 0;
					while(EnemyBulletsRemoved < int (currentEnemy.Bullets.length()))
					{
						NodeManager::DeleteNode(currentEnemy.Bullets[EnemyBulletsRemoved].NodeID);
						EnemyBulletsRemoved++;
					}
					EnemiesSpawned.removeAt(EnemiesHitChecked);
					NodeManager::DeleteNode(NodeID);
					Player1.Bullets.removeAt(Player1.Bullets.find(this));
					Player1.Score++;
					bulletHitEnemy = true;
				}
			}
			EnemiesHitChecked++;
		}
				
		//Check if Player Hit Boss Enemy if they exist:
		if (BossEnemySpawned && !bulletHitEnemy)
		{
			SpriteNode@ enemyNode = NodeManager::GetNode(BossEnemyNodeID);
			
			if (enemyNode !is null)
			{
				Vec3F enemyNodePositionData = enemyNode.GetPosition();
				if(CheckCollision(enemyNodePositionData.x, enemyNodePositionData.y, bossWidth, bossHeight, playerBulletPositionData.x,playerBulletPositionData.y, Width, Height))
				{					
					if (bossHealth > 1)
					{
						bulletHitEnemy = true;
						bossHealth = bossHealth - 1;
					}
					else
					{
						Player1.Score = Player1.Score + 10;
						bulletHitEnemy = true;
						BossEnemySpawned = false;
						NodeManager::DeleteNode(BossEnemyNodeID);
						BossEnemyNodeID = 0;	
					}
					NodeManager::DeleteNode(NodeID);
					Player1.Bullets.removeAt(Player1.Bullets.find(this));
				}
			}
		}
				
		//------------------------------------------------------------------------------------------------

		//************************************************************************************************ 
		// Move Bullet if it wasn't hit
		//************************************************************************************************
		if(!bulletHitEnemy)
		{
			if (playerBulletPositionData.x > -30 && playerBulletPositionData.y > -30)
			{
				Vec3F bulletRotation = bulletNode.GetRotation();
				float bulletRadians = (bulletRotation.z + 270.0f) * PI() / 180.0f; // convert to radians
				            
				Vec3F bulletVelocity = Vec3F(Speed * cos(bulletRadians), -Speed * sin(bulletRadians), 0); // calculate X and Y components separately
                
				bulletNode.SetPosition(bulletNode.GetPosition() + bulletVelocity);
			}
			else
			{
				NodeManager::DeleteNode(NodeID);
			}
		}        
    }

	void UpdateEnemyBulletPosition(int EnemyID)
    {
		//************************************************************************************************ 
		// Detect if Enemy Hits Player
		//************************************************************************************************
				
		SpriteNode@ bulletNode = NodeManager::GetNode(NodeID);
		
		if (bulletNode is null)
		{
			return;
		}
		Vec3F enemyBulletPositionData = bulletNode.GetPosition();

		bool bulletHitEnemy = false;
		if(!bulletHitEnemy)
		{	
			SpriteNode@ playerNode = NodeManager::GetNode(Player1.Sprite.GetID());
	
			if (playerNode !is null && bulletNode !is null)
			{
				Vec3F playerNodePositionData = playerNode.GetPosition();
				if(CheckCollision(playerNodePositionData.x, playerNodePositionData.y, Player1.Width, Player1.Height, enemyBulletPositionData.x,enemyBulletPositionData.y, Width, Height))
				{
					NodeManager::DeleteNode(playerNode.GetID());
					NodeManager::DeleteNode(NodeID);
					EnemiesSpawned[EnemyID].Bullets.removeAt(EnemiesSpawned[EnemyID].Bullets.find(this));
					bulletHitEnemy = true;
				}
			}
		}				
		//------------------------------------------------------------------------------------------------

		//************************************************************************************************ 
		// Move Bullet if it wasn't hit
		//************************************************************************************************
		if(!bulletHitEnemy)
		{
			if (enemyBulletPositionData.x > -30 && enemyBulletPositionData.y > -30)
			{
				Vec3F bulletRotation = bulletNode.GetRotation();
				float bulletRadians = (bulletRotation.z + 90.0f) * PI() / 180.0f; // convert to radians
				            
				Vec3F bulletVelocity = Vec3F(Speed * cos(bulletRadians), -Speed * sin(bulletRadians), 0); // calculate X and Y components separately
                
				bulletNode.SetPosition(bulletNode.GetPosition() + bulletVelocity);
			}
			else
			{
				NodeManager::DeleteNode(NodeID);
			}
		}        
    }
	
	
	float custom_fmod(float a, float b)
{
    return a - b * floor(a / b);
}
	
	
void SpawnNewPlayerBullet()
{
    // Create a new bullet sprite node
    SpriteNode@ bullet = NodeManager::CreateSprite();
    bullet.SetTexturePath("asset:images\\game\\playerbullet.png");
    bullet.SetUV(RectF(0, 0, 1, 1));
    bullet.SetSize(SizeF(Width, Height));
    bullet.SetAnchor(Vec3F(Width / 2, Height / 2, 0));
    bullet.SetDepth(DepthOperationDisabled);
    bullet.SetVisible(true);
    NodeManager::AssignNode(bullet, lighting.GetID());

    // Store the bullet node ID in the player's bullets array
   // Player1.Bullets.insertLast(bullet.GetID());
   NodeID = bullet.GetID();
		 Player1.Bullets.insertLast(this);

    // Play the gun sound effect
    //AudioManager::PlayAudio("asset:Audio\\tetris.ogg", false);

    // Calculate bullet position and rotation based on player rotation
    Vec3F playerPosition = Player1.Sprite.GetPosition();
    Vec3F playerRotation = Player1.Sprite.GetRotation();
    float bulletRotationAngle = playerRotation.z;

    // Normalize bullet rotation angle to range [0, 360)
    bulletRotationAngle = bulletRotationAngle - floor(bulletRotationAngle / 360.0f) * 360.0f;

    // Calculate bullet position based on player position and rotation
    float bulletX = playerPosition.x + (Player1.Width / 2) + cos(bulletRotationAngle);
    float bulletY = playerPosition.y + (Player1.Height / 2) + sin(bulletRotationAngle);
    bullet.SetPosition(Vec3F(bulletX, bulletY, 0));
}

	void SpawnNewEnemyBullet(int EnemyID)
	{
		// SpriteNode@ BulletImg2 = NodeManager::CreateSprite();
		
		// BulletImg2.SetTexturePath("asset:images\\game\\enemybullet.png");
		// BulletImg2.SetUV(RectF(0, 0, 1, 1));
				
		//Calculate bullet position and rotation based on player rotation
		// Vec3F enemyPosition = EnemiesSpawned[EnemyID].Sprite.GetPosition();
		
		// BulletImg2.SetAnchor(Vec3F(Width / 2 , Height / 2, 0));

		// Vec3F enemyRotation = EnemiesSpawned[EnemyID].Sprite.GetRotation();
		
		//Calculate bullet rotation based on player rotation
		// float bulletRotationAngle = enemyRotation.z;
		// if (bulletRotationAngle > 360.0f) 
		// {
			// bulletRotationAngle -= 360.0f;
		// }

		// BulletImg2.SetRotation(Vec3F(0, 0, bulletRotationAngle));

		// BulletImg2.SetPosition(Vec3F((enemyPosition.x + (EnemiesSpawned[EnemyID].Width / 2) )+ cos(enemyRotation.z), (enemyPosition.y - (EnemiesSpawned[EnemyID].Height / 2)) - sin(enemyRotation.z), 0));		
		
		// BulletImg2.SetSize(SizeF(Width, Height));
		// BulletImg2.SetDepth(DepthOperationDisabled);
		
		//BulletImg2.SetVisible(true);	
		// NodeManager::AssignNode(BulletImg2, lighting.GetID());	
		
		// NodeID = BulletImg2.GetID();
		
		// EnemiesSpawned[EnemyID].Bullets.insertLast(this);
	}
	
	bool opEquals(Bullet@ input)
	{
		return input.NodeID == this.NodeID;
	}
	
}

//************************************************************************************************ 
// Table For Player + set up players
//************************************************************************************************

class Player
{
	SpriteNode@ Sprite = NodeManager::CreateSprite();
	
	int Width = 35;
	int Height = 30;

	int Score = 0;

	float Speed = 400;
	float RotationSpeed = 250;

	uint64 LastBulletFiredTime = 0;
	uint64 BulletsLastUpdatedTime = 0;
	array<Bullet@> Bullets;
	
	void SpawnNewPlayer()
	{
		Sprite.SetTexturePath("asset:images\\game\\FishShip.png");
		Sprite.SetUV(RectF(0, 0, 1, 1));
		Sprite.SetAnchor(Vec3F(Width / 2, Height / 2, 0));
		Sprite.SetSize(SizeF(Width, Height));
		Sprite.SetDepth(DepthOperationDisabled);
		NodeManager::AssignNode(Sprite, lighting.GetID());
		
		Sprite.SetPosition(Vec3F(windowSize.width / 2 - Width /2 , 15 + Height / 2, 0)); //windowSize.height if you want to spawn at top
		
		textP1Score.SetText("P1 Score = " + Score);
	}
}

Player@ Player1 = Player();

//************************************************************************************************ 
// Table For Enemy + Enemy Storage
//************************************************************************************************

class Enemy
{
	SpriteNode@ Sprite = NodeManager::CreateSprite();
	int Width = 35;
	int Height = 30;

	float Speed = Player1.Speed / 2;
	
	uint64 LastBulletFiredTime = 0;
	uint64 BulletsLastUpdatedTime = 0;

	array<Bullet@> Bullets;
	
	//************************************************************************************************ 
	// Spawn in a new Enemy (Regular or Boss)
	//************************************************************************************************ 
	void SpawnNewEnemy(bool isBoss)
	{	

		if (isBoss && !BossEnemySpawned)
		{
			Sprite.SetTexturePath("asset:images\\game\\boss.png");
			Sprite.SetSize(SizeF(bossWidth, bossHeight));		
		}
		else if (!isBoss && EnemiesSpawned.length() < 10)
		{
			Sprite.SetTexturePath("asset:images\\game\\PineappleShip.png");
			Sprite.SetSize(SizeF(Width, Height));
		}
	
		Sprite.SetUV(RectF(0, 0, 1, 1));
		Sprite.SetAnchor(Vec3F(0, 0, 0));
	
		float RandEnemyXPosition = 15;
		
		bool EnemyOverlaid = true;
	
		while (EnemyOverlaid)
		{
			RandEnemyXPosition = GetRandomIntInRange(30, windowSize.width - 30);
		
			if (EnemiesSpawned.length() == 0)
			{
				EnemyOverlaid = false;
			}
			int enemiesCollided = 0;
			
			for(uint i = 0; i < EnemiesSpawned.length(); i++)
			{
				SpriteNode@ enemyNode = NodeManager::GetNode(EnemiesSpawned[i].Sprite.GetID());
				Vec3F enemyNodePositionData = enemyNode.GetPosition();
		
				bool WillCollide = CheckCollision(enemyNodePositionData.x, enemyNodePositionData.y, Width, Height, RandEnemyXPosition, 400, Width, Height);
		
				if (WillCollide)
				{
					enemiesCollided++;
				}
			}
			if (enemiesCollided == 0)
			{
				EnemyOverlaid = false;
			}
		}
	
		Sprite.SetPosition(Vec3F(RandEnemyXPosition, 400, 0));
	
		Sprite.SetDepth(DepthOperationDisabled);
		//BulletImg2.SetVisible(true);	
		NodeManager::AssignNode(Sprite, lighting.GetID());	
	
		if (!isBoss)
		{
			EnemiesSpawned.insertLast(this);	
		}
		else if (isBoss)
		{
			BossEnemySpawned = true;
			bossHealth = 10;
			BossEnemyNodeID = Sprite.GetID();
		}
	}
}

array<Enemy@> EnemiesSpawned;


//************************************************************************************************ 
// Table For Boss Enemy
//************************************************************************************************

int bossWidth = 35;
int bossHeight = 30;

bool BossEnemySpawned = false;
int BossEnemyNodeID = 0;

int bossHealth = 10;

Vec2F BossEnemyPos = Vec2F();
float BossEnemySpeed = Player1.Speed / 2;

uint64 BossEnemyLastBulletFiredTime = 0;
uint64 BossEnemyBulletsLastUpdatedTime = 0;
array<int> BossEnemyBullets;

//************************************************************************************************ 
// Initialise the Game: Background + Scores + Spawn first player.
//************************************************************************************************
bool GameHasInitialized = false;

void InitialiseGame()
{
	Parallax1a.SetTexturePath("asset:images\\backgrounds\\Nebula Aqua-Pink.png");
    Parallax1a.SetUV(RectF(0, 0, 1, 1));
    Parallax1a.SetSize(SizeF(640, 480));
	Parallax1a.SetPosition(Vec3F(0, 0, 0));
	Parallax1a.SetDepth(DepthOperationDisabled);
    NodeManager::AssignNode(Parallax1a, lighting.GetID());
	Parallax1b.SetTexturePath("asset:images\\backgrounds\\Nebula Aqua-Pink.png");
    Parallax1b.SetUV(RectF(0, 0, 1, 1));
    Parallax1b.SetSize(SizeF(640, 480));
	Parallax1b.SetPosition(Vec3F(0, 480, 0));
	Parallax1b.SetDepth(DepthOperationDisabled);
    NodeManager::AssignNode(Parallax1b, lighting.GetID());

	Parallax2a.SetTexturePath("asset:images\\backgrounds\\Stars-Big_1_2_PC.png");
    Parallax2a.SetUV(RectF(0, 0, 1, 1));
    Parallax2a.SetSize(SizeF(640, 480));
	Parallax2a.SetPosition(Vec3F(0, 0, 1));
	Parallax2a.SetDepth(DepthOperationDisabled);
    NodeManager::AssignNode(Parallax2a, lighting.GetID());
	Parallax2b.SetTexturePath("asset:images\\backgrounds\\Stars-Big_1_2_PC.png");
    Parallax2b.SetUV(RectF(0, 0, 1, 1));
    Parallax2b.SetSize(SizeF(640, 480));
	Parallax2b.SetPosition(Vec3F(0, 480, 1));
	Parallax2b.SetDepth(DepthOperationDisabled);
    NodeManager::AssignNode(Parallax2b, lighting.GetID());

	Parallax3a.SetTexturePath("asset:images\\backgrounds\\Stars-Big_1_1_PC.png");
    Parallax3a.SetUV(RectF(0, 0, 1, 1));
    Parallax3a.SetSize(SizeF(640, 480));
	Parallax3a.SetPosition(Vec3F(0, 0, 2));
	Parallax3a.SetDepth(DepthOperationDisabled);
    NodeManager::AssignNode(Parallax3a, lighting.GetID());
	Parallax3b.SetTexturePath("asset:images\\backgrounds\\Stars-Big_1_1_PC.png");
    Parallax3b.SetUV(RectF(0, 0, 1, 1));
    Parallax3b.SetSize(SizeF(640, 480));
	Parallax3b.SetPosition(Vec3F(0, 480, 2));
	Parallax3b.SetDepth(DepthOperationDisabled);
    NodeManager::AssignNode(Parallax3b, lighting.GetID());
	
	textP1Score.SetFontName("FreeSans");
    textP1Score.SetFontStyle(0);
    textP1Score.SetFontSize(20);
    //TODO: SetFontStyle(FontStyleBold | FontStyleItalic | FontStyleUnderline);
    textP1Score.SetPosition(Vec3F(480, 455, 20));
    NodeManager::AssignNode(textP1Score, lighting.GetID());

    textFPS.SetFontName("FreeSans");
    textFPS.SetFontStyle(0);
    textFPS.SetFontSize(20);
    //TODO: SetFontStyle(FontStyleBold | FontStyleItalic | FontStyleUnderline);
    textFPS.SetPosition(Vec3F(50, 455, 200));
    NodeManager::AssignNode(textFPS, lighting.GetID());
    
    textMem.SetFontName("FreeSans");
    textMem.SetFontStyle(0);
    textMem.SetFontSize(20);
    //TODO: SetFontStyle(FontStyleBold | FontStyleItalic | FontStyleUnderline);
    textMem.SetPosition(Vec3F(200, 455, 200));
    NodeManager::AssignNode(textMem, lighting.GetID());


	
	//Spawn in the players...
	Player1.SpawnNewPlayer();
	
	//AudioManager::PlayAudio("asset:Audio\\tetris.ogg", true);

	GameHasInitialized = true;
}


//Adjust to preference for background scrolling
float speed1 = 80.0f;
float speed2 = 70.0f;
float speed3 = 35.0f;


void UpdateBackground(double dt)
{
	// Update the position of each background layer based on its scrolling speed
    Parallax1a.SetPosition(Parallax1a.GetPosition() - Vec3F(0, float(speed1 * dt), 0));
    Parallax2a.SetPosition(Parallax2a.GetPosition() - Vec3F(0, float(speed2 * dt), 0));
    Parallax3a.SetPosition(Parallax3a.GetPosition() - Vec3F(0, float(speed3 * dt), 0));

    // If a background layer goes off the screen, wrap it around to the top
    if (Parallax1a.GetPosition().y + Parallax1a.GetSize().height < 0)
    {
        Parallax1a.SetPosition(Parallax1a.GetPosition() + Vec3F(0, Parallax1a.GetSize().height, 0));
    }
    if (Parallax2a.GetPosition().y + Parallax2a.GetSize().height < 0)
    {
        Parallax2a.SetPosition(Parallax2a.GetPosition() + Vec3F(0, Parallax2a.GetSize().height, 1));
    }
    if (Parallax3a.GetPosition().y + Parallax3a.GetSize().height < 0)
    {
        Parallax3a.SetPosition(Parallax3a.GetPosition() + Vec3F(0, Parallax3a.GetSize().height, 0));
    }

	Parallax1b.SetPosition(Parallax1a.GetPosition() + Vec3F(0, Parallax1a.GetSize().height, 0));
	Parallax2b.SetPosition(Parallax2a.GetPosition() + Vec3F(0, Parallax2a.GetSize().height, 0));
	Parallax3b.SetPosition(Parallax3a.GetPosition() + Vec3F(0, Parallax3a.GetSize().height, 0));
}

//************************************************************************************************ 
// Main Function: Initialise all main assets
//************************************************************************************************
void Init()
{
    FontManager::LoadFont("skin:asset\\fonts\\freesans.sfn");
	PreLoader::LoadTexture("asset:images\\TR_Banner_Background.jpg");
	PreLoader::LoadTexture("asset:images\\game\\playerbullet.png");
	PreLoader::LoadTexture("asset:images\\game\\boss.png");
	PreLoader::LoadTexture("asset:images\\game\\PineappleShip.png");
	PreLoader::LoadTexture("asset:images\\backgrounds\\Nebula Aqua-Pink.png");
	PreLoader::LoadTexture("asset:images\\backgrounds\\Stars-Big_1_2_PC.png");
	PreLoader::LoadTexture("asset:images\\backgrounds\\Stars-Big_1_1_PC.png");

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
	
	
	//Do Intro Image before doing game initialisation:
	InitializeLoadingImage();


	

    DebugPrint(LogLevelInfo, "Initializing...");

    // Create window example
    WindowCreateWithSize(640, 480, "Nexgen Redux - Shooter Demo");


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
	InitialiseGame();
//--------------------------------------------------------------------------------------------------		
	
    //Initialise Tick for restricting render based updates to time based intervals
    previousTickTime = GetMillisecondsNow();
	
}

void Update(double dt)
{
	// For now we need to call this once rendering started
	if (PreLoader::AllTexturesLoaded() == false)
	{
		return;
	}


	//************************************************************************************************ 
	// Fps & Mem Counters
	//************************************************************************************************ 
		currentTime = GetMillisecondsNow();
		double durationFPS = GetDurationSeconds(previousTickTime, currentTime);

		// This should be not too frequenct as vector font rendering is expensive, also improves accuracy
		if (durationFPS > 2)
		{
			double fps = CalculateFramesPerSecond(frameCount, durationFPS);
			textFPS.SetText("fps = " + fps);
			//DebugPrint(LogLevelInfo, "fps = " + fps);
			frameCount = 0;
			previousTickTime = currentTime;
		}
    
		textMem.SetText("free mem = " + (GetFreePhysicalMemory() / (1024 * 1024)) + "MB");
	//-------------------------------------------------------------------------------------------------- 

// if(!FadedIn && !FadeInProgress)
// {
	// FadeInProgress = true;
	// FadeImage();
// }
// if(!FadedIn || !FadedOut)
// {
	// FadeImage();
// }
// if(FadedIn && !FadedOut)
// {
	// FadeImage();
// }

// if (FadedIn && FadedOut && !GameHasInitialized)
// {
	 //InitialiseGame();
// }

//if (GameHasInitialized)
 {

UpdateBackground(dt);

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

//Get the position of the players for movement calculations
Vec3F PlayerPos = Player1.Sprite.GetPosition();
Vec3F PlayerRotation = Player1.Sprite.GetRotation();

//-------------------------------------------------------------------------------------------------- 

//Move Player 1 Around screen

if (WArrowPressed == 1 || joystickButtonStates.buttonDpadUp == JoystickButtonStatePressed || joystickAxisStates.axisLeftY < - 0.2) {
    // Move up
    if ( PlayerPos.y < windowSize.height - Player1.Height) {
        PlayerPos.y = float(PlayerPos.y + (Player1.Speed * dt));
        Player1.Sprite.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
    }
    else {
        PlayerPos.y = windowSize.height - Player1.Height;
    }
}
if (SArrowPressed == 1 || joystickButtonStates.buttonDpadDown == JoystickButtonStatePressed || joystickAxisStates.axisLeftY > 0.2) {
    // Move down
    if (PlayerPos.y > 0) {
        PlayerPos.y = float(PlayerPos.y - (Player1.Speed * dt));
        Player1.Sprite.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
    }
    else {
        PlayerPos.y = 0;
    }
}
if (AArrowPressed == 1 || joystickButtonStates.buttonDpadLeft == JoystickButtonStatePressed || joystickAxisStates.axisLeftX < -0.2) {
    // Move left
    if (PlayerPos.x > 0) {
        PlayerPos.x = float(PlayerPos.x - (Player1.Speed * dt));
        Player1.Sprite.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
    }
    else {
        PlayerPos.x = 0;
    }
}
if (DArrowPressed == 1 || joystickButtonStates.buttonDpadRight == JoystickButtonStatePressed || joystickAxisStates.axisLeftX > 0.2) {
    // Move right
    if (PlayerPos.x < windowSize.width - bossWidth) {
        PlayerPos.x = float(PlayerPos.x + (Player1.Speed * dt));
        Player1.Sprite.SetPosition(Vec3F(PlayerPos.x, PlayerPos.y, 0));
    }
    else {
        PlayerPos.x = windowSize.width - bossWidth;
    }
}

//Rotate Player 1 Around Screen
if (RightArrowPressed == 1)
{
	//DebugPrint(LogLevelInfo, "Rotation = " + PlayerRotation.z);
    PlayerRotation = (Vec3F(PlayerRotation.x, PlayerRotation.y, PlayerRotation.z - Player1.RotationSpeed * dt));
    Player1.Sprite.SetRotation(PlayerRotation);
}

if (LeftArrowPressed == 1)
{
    PlayerRotation = (Vec3F(PlayerRotation.x, PlayerRotation.y, PlayerRotation.z + Player1.RotationSpeed * dt));
    Player1.Sprite.SetRotation(PlayerRotation);
}

if (joystickAxisStates.axisRightX < -1.5259e-05 || joystickAxisStates.axisRightY > 1.5259e-05 ) 
{
	float joystickX = joystickAxisStates.axisRightX;
	float joystickY = joystickAxisStates.axisRightY;
	
	//DebugPrint(LogLevelInfo, "joyX = " + joystickX);
	//DebugPrint(LogLevelInfo, "joyY = " + joystickY);
    
	// Calculate the rotation in degrees.
    float rotationInDegrees = atan2(joystickY, joystickX) * 180.0f / PI();
	
    PlayerRotation = (Vec3F(0, 0, rotationInDegrees - 90.0f)); //Slowly rotate right: PlayerRotation.x, PlayerRotation.y, PlayerRotation.z - Player1.RotationSpeed * dt
    Player1.Sprite.SetRotation(PlayerRotation);
}


//Fire Player 1's Primary Weapon

if (LeftMouseClicked == 1 || SpaceBarPressed == 1 || joystickButtonStates.buttonA == JoystickButtonStatePressed || joystickAxisStates.axisRightTrigger > 0.1)
{
	currentTime = GetMillisecondsNow();
    double durationPlayerBulletLastFiredToNow = GetDurationSeconds(Player1.LastBulletFiredTime, currentTime);
	if (durationPlayerBulletLastFiredToNow > 0.250)
	{	
		FirePlayerBullet();
		Player1.LastBulletFiredTime = currentTime;
	}
}


//************************************************************************************************ 
// Update Player1's Bullet locations on screen
//************************************************************************************************ 

	uint i = 0;
	currentTime = GetMillisecondsNow();
	double SecondsSinceLastBulletUpdate = GetDurationSeconds(Player1.BulletsLastUpdatedTime, currentTime);
	if (SecondsSinceLastBulletUpdate > 0.0125) //BulletSpeed
	{
		while(i < Player1.Bullets.length())
		{
			Bullet@ currentBullet = Player1.Bullets[i];
			
			currentBullet.UpdatePlayerBulletPosition();
			
			i++;
		}
		
		Player1.BulletsLastUpdatedTime = GetMillisecondsNow();	

	}


//------------------------------------------------------------------------------------------------ 

//************************************************************************************************ 
// Update Enemy's Bullet locations on screen
//************************************************************************************************ 

	int EnemyNo = 0;
	
	if (EnemiesSpawned.length() > 1)
	{	
		while(EnemyNo <= int (EnemiesSpawned.length() - 1))
		{
			currentTime = GetMillisecondsNow();
			double SecondsSinceLastEnemyBulletUpdate = GetDurationSeconds(EnemiesSpawned[EnemyNo].BulletsLastUpdatedTime, currentTime);
			if (SecondsSinceLastEnemyBulletUpdate > 0.0125) //BulletSpeed
			{
				int EnemyBulletCount = 0;
				while (EnemyBulletCount <= int (EnemiesSpawned[EnemyNo].Bullets.length() - 1))
				{
					Bullet@ currentBullet = EnemiesSpawned[EnemyNo].Bullets[EnemyBulletCount];
					if (currentBullet !is null)
					{
						currentBullet.UpdateEnemyBulletPosition(EnemyNo);
						EnemyBulletCount++;
					}
				}
				EnemiesSpawned[EnemyNo].BulletsLastUpdatedTime = GetMillisecondsNow();					
			}
			EnemyNo++;
		}		
	}
	
	


//------------------------------------------------------------------------------------------------ 


//************************************************************************************************ 
// Spawn Enemies!
//************************************************************************************************
	
	if ((EnemiesSpawned.length() < 2 && Player1.Score < 5) || (EnemiesSpawned.length() < 3 && Player1.Score >= 5) || (EnemiesSpawned.length() < 5 && Player1.Score >= 10))
	{		
		Enemy().SpawnNewEnemy(false); //Spawn a normal enemy
	}
	if (!BossEnemySpawned && Player1.Score >= 25)
	{
		Enemy().SpawnNewEnemy(true); //Spawn a boss.
	}
 

//------------------------------------------------------------------------------------------------

//************************************************************************************************ 
// Spawn Enemy Bullets!
//************************************************************************************************
	
	int EnemyCounter = 0;
	if (EnemiesSpawned.length() > 1)
	{	
		while(EnemyCounter <= int (EnemiesSpawned.length() - 1))
		{	
			Enemy@ currentEnemy = EnemiesSpawned[EnemyCounter];
			
			if (currentEnemy !is null)
			{
				currentTime = GetMillisecondsNow();
				double durationEnemyBulletLastFiredToNow = GetDurationSeconds(currentEnemy.LastBulletFiredTime, currentTime);
				if (durationEnemyBulletLastFiredToNow > 0.500)
				{	
					Bullet().SpawnNewEnemyBullet(EnemyCounter);
					currentEnemy.LastBulletFiredTime = currentTime;
				}
			}
			EnemyCounter++;
		}
	}
 

//------------------------------------------------------------------------------------------------


}

	//Update Player's Scores...
	textP1Score.SetText("P1 Score = " + Player1.Score);

    frameCount++;
	
}



//************************************************************************************************ 
// Create a Bullet for Player and Play Sound (Sound not supported by engine yet)
//************************************************************************************************ 
void FirePlayerBullet()
{	
	Bullet().SpawnNewPlayerBullet();
}





//************************************************************************************************ 
// Collision Checker: See below description.
//************************************************************************************************ 

// Collision detection ported function from http://love2d.org/wiki/BoundingBox.lua
// Returns true if two boxes overlap, false if they don't
// x1,y1 are the left-top coords of the first box, while w1,h1 are its width and height
// x2,y2,w2 & h2 are the same, but for the second box

bool CheckCollision(float x1, float y1, float w1, float h1, float x2, float y2, float w2, float h2)
{
    return x1 < x2 + w2 && x2 < x1 + w1 && y1 < y2 + h2 && y2 < y1 + h1;
}

//------------------------------------------------------------------------------------------------
