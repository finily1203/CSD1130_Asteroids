// GameServer.cpp
#include "GameServer.h"
#include <algorithm>
#include <random>
#include <iostream>

/******************************************************************************/
/*!
    Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX = 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX = 2048;			// The total number of different game object instances


const unsigned int	SHIP_INITIAL_NUM = 3;			// initial number of ship lives
const float			SHIP_SCALE_X = 16.0f;		// ship scale x
const float			SHIP_SCALE_Y = 16.0f;		// ship scale y
const float			BULLET_SCALE_X = 20.0f;		// bullet scale x
const float			BULLET_SCALE_Y = 3.0f;			// bullet scale y
const float			ASTEROID_MIN_SCALE_X = 10.0f;		// asteroid minimum scale x
const float			ASTEROID_MAX_SCALE_X = 60.0f;		// asteroid maximum scale x
const float			ASTEROID_MIN_SCALE_Y = 10.0f;		// asteroid minimum scale y
const float			ASTEROID_MAX_SCALE_Y = 60.0f;		// asteroid maximum scale y

const float			WALL_SCALE_X = 64.0f;		// wall scale x
const float			WALL_SCALE_Y = 164.0f;		// wall scale y

const float			SHIP_ACCEL_FORWARD = 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD = 100.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED = (2.0f * PI);	// ship rotation speed (degree/second)

const float			BULLET_SPEED = 400.0f;		// bullet speed (m/s)

const float         BOUNDING_RECT_SIZE = 1.0f;         // this is the normalized bounding rectangle (width and height) sizes - AABB collision data

// Additional texture and font
s8 pFont;
AEGfxTexture* pTex, * pTex1, * pTexship;
AEGfxVertexList* pMesh = 0;
// -----------------------------------------------------------------------------
enum TYPE
{
    // list of game object types
    TYPE_SHIP = 0,
    TYPE_BULLET,
    TYPE_ASTEROID,
    TYPE_WALL,

    TYPE_NUM
};

// -----------------------------------------------------------------------------
// object flag definition

const unsigned long FLAG_ACTIVE = 0x00000001;
static bool onValueChange = true;
static bool over = false;
/******************************************************************************/
/*!
    Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
    unsigned long		type;		// object type
    AEGfxVertexList* pMesh;		// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
    GameObj* pObject;	// pointer to the 'original' shape
    unsigned long		flag;		// bit flag or-ed together
    AEVec2				scale;		// scaling value of the object instance
    AEVec2				posCurr;	// object current position

    AEVec2				posPrev;	// object previous position -> it's the position calculated in the previous loop

    AEVec2				velCurr;	// object current velocity
    float				dirCurr;	// object current direction
    AABB				boundingBox;// object bouding box that encapsulates the object
    AEMtx33				transform;	// object transformation matrix: Each frame, 
    // calculate the object instance's transformation matrix and save it here

// Add these new properties for multiplayer
    uint16_t            id;         // for identifying asteroids and bullets
    uint8_t             clientID;   // for identifying which player owns the object
    float               lifeTime;   // for bullets lifetime tracking
};

/******************************************************************************/
/*!
    Static Variables
*/
/******************************************************************************/

// list of original object
static GameObj				sGameObjList[GAME_OBJ_NUM_MAX];				// Each element in this array represents a unique game object (shape)
static unsigned long		sGameObjNum;								// The number of defined game objects

// list of object instances
static GameObjInst			sGameObjInstList[GAME_OBJ_INST_NUM_MAX];	// Each element in this array represents a unique game object instance (sprite)
static unsigned long		sGameObjInstNum;							// The number of used game object instances

// pointer to the ship object
static GameObjInst* spShip;										// Pointer to the "Ship" game object instance

// pointer to the wall object
static GameObjInst* spWall;										// Pointer to the "Wall" game object instance

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Current score

// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst* gameObjInstCreate(unsigned long type, AEVec2* scale,
    AEVec2* pPos, AEVec2* pVel, float dir);
void				gameObjInstDestroy(GameObjInst* pInst);

GameServer::GameServer()
    : isRunning(false),
    gameInProgress(false),
    gameStateTimer(0.0f),
    gameEndTimer(0.0f) {
}

GameServer::~GameServer() {
    Shutdown();
}

bool GameServer::Initialize(uint16_t port) {
    // Set up network callbacks
    server.SetConnectCallback([this](ClientID clientID) { OnClientConnect(clientID); });
    server.SetDisconnectCallback([this](ClientID clientID) { OnClientDisconnect(clientID); });
    server.SetMessageCallback([this](ClientID clientID, const void* data, size_t size) {
        OnMessage(clientID, data, size);
        });

    // Initialize UDP server
    if (!server.Initialize(port)) {
        return false;
    }

    isRunning = true;
    gameInProgress = false;

    std::cout << "Game server initialized on port " << port << std::endl;
    return true;
}

void GameServer::Shutdown() {
    if (isRunning) {
        // Stop the server
        server.Shutdown();
        isRunning = false;

        // Clean up player ships
        {
            std::lock_guard<std::mutex> lock(playersMutex);
            for (auto& pair : players) {
                if (pair.second.ship) {
                    gameObjInstDestroy(pair.second.ship);
                    pair.second.ship = nullptr;
                }
            }
            players.clear();
        }

        // Clean up game objects
        {
            std::lock_guard<std::mutex> lock(gameObjectsMutex);

            // Clean up asteroids
            for (auto* asteroid : asteroids) {
                if (asteroid) {
                    gameObjInstDestroy(asteroid);
                }
            }
            asteroids.clear();

            // Clean up bullets
            for (auto* bullet : bullets) {
                if (bullet) {
                    gameObjInstDestroy(bullet);
                }
            }
            bullets.clear();
        }

        std::cout << "Game server shut down" << std::endl;
    }
}

void GameServer::Update(float dt) {
    if (!isRunning) {
        return;
    }

    // Update game state
    if (gameInProgress) {
        UpdateGameState(dt);

        // Send game state updates at fixed intervals
        gameStateTimer += dt;
        if (gameStateTimer >= GAME_STATE_UPDATE_INTERVAL) {
            SendGameState();
            gameStateTimer = 0.0f;
        }

        // Check for game end
        CheckGameEndConditions();
    }
    else {
        // Check if we have enough players to start a new game
        if (server.GetClientCount() > 0) {
            // Start a new game
            ResetGame();
            gameInProgress = true;
            std::cout << "Game started with " << server.GetClientCount() << " players" << std::endl;
        }
    }

    // If game is over and timer expired, reset the game
    if (!gameInProgress && gameEndTimer > 0.0f) {
        gameEndTimer -= dt;
        if (gameEndTimer <= 0.0f) {
            // Reset and start a new game if we have players
            if (server.GetClientCount() > 0) {
                ResetGame();
                gameInProgress = true;
                std::cout << "New game started with " << server.GetClientCount() << " players" << std::endl;
            }
        }
    }
}

void GameServer::OnClientConnect(ClientID clientID) {
    std::cout << "Client " << (int)clientID << " connected" << std::endl;

    {
        std::lock_guard<std::mutex> lock(playersMutex);

        // Create player data
        PlayerData newPlayer;
        newPlayer.ship = nullptr;
        newPlayer.isAlive = true;
        newPlayer.score = 0;
        newPlayer.lives = INITIAL_LIVES;

        // Add to players map
        players[clientID] = newPlayer;
    }

    // If game is in progress, add the player to the game
    if (gameInProgress) {
        CreatePlayerShip(clientID);
    }
    else if (server.GetClientCount() == 1) {
        // First player - start the game
        ResetGame();
        gameInProgress = true;
        std::cout << "Game started with player " << (int)clientID << std::endl;
    }
}

void GameServer::OnClientDisconnect(ClientID clientID) {
    std::cout << "Client " << (int)clientID << " disconnected" << std::endl;

    // Remove player ship and data
    RemovePlayerShip(clientID);

    {
        std::lock_guard<std::mutex> lock(playersMutex);
        players.erase(clientID);
    }

    // If no players left, end game
    if (server.GetClientCount() == 0) {
        gameInProgress = false;
        gameEndTimer = 0.0f;
        std::cout << "Game ended - no players remaining" << std::endl;
    }
}

void GameServer::OnMessage(ClientID clientID, const void* data, size_t size) {
    // Process message based on type
    if (size < sizeof(NetworkMessage)) {
        return;
    }

    const NetworkMessage* header = static_cast<const NetworkMessage*>(data);

    switch (header->type) {
    case MessageType::PLAYER_INPUT:
        if (size >= sizeof(PlayerInputMessage)) {
            const PlayerInputMessage* inputMsg = static_cast<const PlayerInputMessage*>(data);
            ProcessPlayerInput(clientID, inputMsg);
        }
        break;

    default:
        // Ignore other message types
        break;
    }
}

void GameServer::ProcessPlayerInput(ClientID clientID, const PlayerInputMessage* inputMsg) {
    std::lock_guard<std::mutex> lock(playersMutex);

    auto it = players.find(clientID);
    if (it == players.end()) {
        return;
    }

    // Store the input for use in the game update
    it->second.lastInput = *inputMsg;
}

void GameServer::UpdateGameState(float dt) {
    std::lock_guard<std::mutex> lockPlayers(playersMutex);
    std::lock_guard<std::mutex> lockObjects(gameObjectsMutex);

    // Process player inputs and update ships
    for (auto& pair : players) {
        ClientID clientID = pair.first;
        PlayerData& player = pair.second;

        if (player.ship && (player.ship->flag & FLAG_ACTIVE) && player.isAlive) {
            GameObjInst* ship = player.ship;

            // Apply controls based on last input
            if (player.lastInput.up) {
                // Apply forward acceleration
                AEVec2 added;
                AEVec2 acceleration_vec;
                AEVec2Set(&added, cosf(ship->dirCurr), sinf(ship->dirCurr));
                AEVec2Scale(&acceleration_vec, &added, SHIP_ACCEL_FORWARD * dt);
                AEVec2Add(&ship->velCurr, &ship->velCurr, &acceleration_vec);
            }

            if (player.lastInput.down) {
                // Apply backward acceleration
                AEVec2 added;
                AEVec2 acceleration_vec;
                AEVec2Set(&added, -cosf(ship->dirCurr), -sinf(ship->dirCurr));
                AEVec2Scale(&acceleration_vec, &added, SHIP_ACCEL_BACKWARD * dt);
                AEVec2Add(&ship->velCurr, &ship->velCurr, &acceleration_vec);
            }

            if (player.lastInput.left) {
                // Rotate left
                ship->dirCurr += SHIP_ROT_SPEED * dt;
                ship->dirCurr = AEWrap(ship->dirCurr, -PI, PI);
            }

            if (player.lastInput.right) {
                // Rotate right
                ship->dirCurr -= SHIP_ROT_SPEED * dt;
                ship->dirCurr = AEWrap(ship->dirCurr, -PI, PI);
            }

            // Apply friction
            AEVec2Scale(&ship->velCurr, &ship->velCurr, 0.99f);

            // Fire bullet if requested
            if (player.lastInput.fire) {
                // Only fire if the fire button was just pressed
                if (!player.lastInput.fire) {
                    AEVec2 bulletVel;
                    AEVec2Set(&bulletVel, cosf(ship->dirCurr), sinf(ship->dirCurr));
                    AEVec2Scale(&bulletVel, &bulletVel, BULLET_SPEED);

                    AEVec2 scale;
                    AEVec2Set(&scale, BULLET_SCALE_X, BULLET_SCALE_Y);

                    GameObjInst* bullet = gameObjInstCreate(TYPE_BULLET, &scale, &ship->posCurr, &bulletVel, ship->dirCurr);

                    if (bullet) {
                        // Store the client ID as owner of the bullet
                        bullet->clientID = clientID;
                        // Store creation time for lifetime management
                        bullet->lifeTime = BULLET_LIFETIME;
                        bullets.push_back(bullet);
                    }
                }
            }
        }
    }

    // Update all game objects
    for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++) {
        GameObjInst* pInst = sGameObjInstList + i;

        // Skip non-active instances
        if ((pInst->flag & FLAG_ACTIVE) == 0)
            continue;

        // Save previous position
        pInst->posPrev.x = pInst->posCurr.x;
        pInst->posPrev.y = pInst->posCurr.y;

        // Update position based on velocity
        pInst->posCurr.x += pInst->velCurr.x * dt;
        pInst->posCurr.y += pInst->velCurr.y * dt;

        // Update bullet lifetime
        if (pInst->pObject->type == TYPE_BULLET) {
            pInst->lifeTime -= dt;
            if (pInst->lifeTime <= 0.0f) {
                // Remove expired bullet
                for (auto it = bullets.begin(); it != bullets.end(); ++it) {
                    if (*it == pInst) {
                        bullets.erase(it);
                        break;
                    }
                }
                gameObjInstDestroy(pInst);
                continue;
            }
        }

        // Wrap position for ships and asteroids
        if (pInst->pObject->type == TYPE_SHIP || pInst->pObject->type == TYPE_ASTEROID) {
            pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - pInst->scale.x,
                AEGfxGetWinMaxX() + pInst->scale.x);
            pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - pInst->scale.y,
                AEGfxGetWinMaxY() + pInst->scale.y);
        }

        // Update transformation matrix
        AEMtx33 scale, rot, trans;
        AEMtx33Scale(&scale, pInst->scale.x, pInst->scale.y);
        AEMtx33Rot(&rot, pInst->dirCurr);
        AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y);

        AEMtx33 temp;
        AEMtx33Concat(&temp, &rot, &scale);
        AEMtx33Concat(&pInst->transform, &trans, &temp);

        // Update bounding box
        AEVec2 tmp;
        AEVec2Scale(&tmp, &pInst->scale, BOUNDING_RECT_SIZE / 2.0f);
        AEVec2Sub(&pInst->boundingBox.min, &pInst->posCurr, &tmp);
        AEVec2Add(&pInst->boundingBox.max, &pInst->posCurr, &tmp);
    }

    // Check for collisions
    CheckForCollisions();

    // Spawn new asteroids if needed
    if (asteroids.size() < INITIAL_ASTEROID_COUNT && asteroids.size() < MAX_ASTEROID_COUNT) {
        // Random position at the edge of the screen
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> disX(AEGfxGetWinMinX(), AEGfxGetWinMaxX());
        std::uniform_real_distribution<float> disY(AEGfxGetWinMinY(), AEGfxGetWinMaxY());
        std::uniform_real_distribution<float> disVel(-60.0f, 60.0f);
        std::uniform_real_distribution<float> disScale(0.8f, 1.5f);

        float x, y;
        int side = gen() % 4;
        switch (side) {
        case 0: // Top
            x = disX(gen);
            y = AEGfxGetWinMinY() - 20.0f;
            break;
        case 1: // Right
            x = AEGfxGetWinMaxX() + 20.0f;
            y = disY(gen);
            break;
        case 2: // Bottom
            x = disX(gen);
            y = AEGfxGetWinMaxY() + 20.0f;
            break;
        case 3: // Left
            x = AEGfxGetWinMinX() - 20.0f;
            y = disY(gen);
            break;
        }

        CreateAsteroid(x, y, disVel(gen), disVel(gen),
            ASTEROID_MAX_SCALE_X * disScale(gen));
    }
}

void GameServer::CheckForCollisions() {
    std::lock_guard<std::mutex> lockPlayers(playersMutex);
    std::lock_guard<std::mutex> lockObjects(gameObjectsMutex);

    // Check bullet-asteroid collisions
    for (auto bulletIt = bullets.begin(); bulletIt != bullets.end();) {
        GameObjInst* bullet = *bulletIt;
        bool bulletDestroyed = false;

        for (auto asteroidIt = asteroids.begin(); asteroidIt != asteroids.end();) {
            GameObjInst* asteroid = *asteroidIt;

            if ((bullet->flag & FLAG_ACTIVE) && (asteroid->flag & FLAG_ACTIVE)) {
                float collisionTime;
                if (CollisionIntersection_RectRect(bullet->boundingBox, bullet->velCurr,
                    asteroid->boundingBox, asteroid->velCurr,
                    collisionTime)) {
                    // Collision detected!

                    // Award points to the player who fired the bullet
                    auto playerIt = players.find(bullet->clientID);
                    if (playerIt != players.end()) {
                        playerIt->second.score += 100;
                    }

                    // Split the asteroid if it's large enough
                    if (asteroid->scale.x >= ASTEROID_MIN_SCALE_X * 2.0f) {
                        SplitAsteroid(asteroid);
                    }

                    // Remove the asteroid
                    asteroidIt = asteroids.erase(asteroidIt);
                    gameObjInstDestroy(asteroid);

                    // Remove the bullet
                    bulletIt = bullets.erase(bulletIt);
                    gameObjInstDestroy(bullet);
                    bulletDestroyed = true;
                    break;
                }
            }

            if (!bulletDestroyed) {
                ++asteroidIt;
            }
        }

        if (!bulletDestroyed) {
            ++bulletIt;
        }
    }

    // Check ship-asteroid collisions
    for (auto& pair : players) {
        PlayerData& player = pair.second;

        if (player.ship && (player.ship->flag & FLAG_ACTIVE) && player.isAlive) {
            for (auto asteroidIt = asteroids.begin(); asteroidIt != asteroids.end(); ++asteroidIt) {
                GameObjInst* asteroid = *asteroidIt;

                if (asteroid->flag & FLAG_ACTIVE) {
                    float collisionTime;
                    if (CollisionIntersection_RectRect(player.ship->boundingBox, player.ship->velCurr,
                        asteroid->boundingBox, asteroid->velCurr,
                        collisionTime)) {
                        // Collision detected - player loses a life
                        player.lives--;

                        if (player.lives <= 0) {
                            // Player is out of lives
                            player.isAlive = false;
                        }
                        else {
                            // Reset ship position
                            AEVec2Set(&player.ship->posCurr, 0.0f, 0.0f);
                            AEVec2Set(&player.ship->velCurr, 0.0f, 0.0f);
                            player.ship->dirCurr = 0.0f;
                        }

                        break;
                    }
                }
            }
        }
    }
}

void GameServer::CheckGameEndConditions() {
    std::lock_guard<std::mutex> lock(playersMutex);

    // Count active players
    int activePlayers = 0;
    for (auto& pair : players) {
        if (pair.second.isAlive) {
            activePlayers++;
        }
    }

    // Game ends if no players are alive or if only one player remains in multiplayer
    if (activePlayers == 0 || (server.GetClientCount() > 1 && activePlayers <= 1)) {
        // Game over!
        gameInProgress = false;
        gameEndTimer = GAME_END_DURATION;

        // Create game end message
        GameEndMessage endMsg;
        endMsg.type = MessageType::GAME_END;
        endMsg.clientID = 0; // Server ID
        endMsg.sequence = 0;

        // Find the winner (highest score)
        uint32_t highestScore = 0;
        ClientID winnerID = 0;

        int i = 0;
        for (auto& pair : players) {
            // Store scores in the message
            if (i < 4) {
                endMsg.scores[i] = pair.second.score;
            }

            if (pair.second.score > highestScore) {
                highestScore = pair.second.score;
                winnerID = pair.first;
            }

            i++;
        }

        endMsg.winnerID = winnerID;
        endMsg.winnerScore = highestScore;

        // Send game end message to all clients
        server.BroadcastToAll(&endMsg, sizeof(endMsg));

        std::cout << "Game ended - Winner is Player " << (int)winnerID
            << " with score " << highestScore << std::endl;
    }
}

void GameServer::SendGameState() {
    std::lock_guard<std::mutex> lockPlayers(playersMutex);
    std::lock_guard<std::mutex> lockObjects(gameObjectsMutex);

    // Calculate total size needed for the message
    size_t playerStateSize = sizeof(ShipState) * players.size();
    size_t asteroidStateSize = sizeof(AsteroidState) * asteroids.size();
    size_t bulletStateSize = sizeof(BulletState) * bullets.size();

    size_t totalSize = sizeof(GameStateMessage) + playerStateSize + asteroidStateSize + bulletStateSize;

    // Create buffer for the message
    std::vector<char> buffer(totalSize);
    GameStateMessage* msg = reinterpret_cast<GameStateMessage*>(buffer.data());

    // Set header data
    msg->type = MessageType::GAME_STATE;
    msg->clientID = 0; // Server ID
    msg->sequence = 0;
    msg->playerCount = static_cast<uint8_t>(players.size());
    msg->asteroidCount = static_cast<uint16_t>(asteroids.size());
    msg->bulletCount = static_cast<uint16_t>(bullets.size());
    msg->gameStatus = gameInProgress ? 1 : 0;

    // Add player ships data
    ShipState* shipStates = reinterpret_cast<ShipState*>(buffer.data() + sizeof(GameStateMessage));
    int shipIndex = 0;

    for (auto& pair : players) {
        ClientID clientID = pair.first;
        PlayerData& player = pair.second;

        ShipState& shipState = shipStates[shipIndex++];
        shipState.active = player.isAlive && player.ship && (player.ship->flag & FLAG_ACTIVE);

        if (shipState.active) {
            shipState.posX = player.ship->posCurr.x;
            shipState.posY = player.ship->posCurr.y;
            shipState.dirCurr = player.ship->dirCurr;
            shipState.velocityX = player.ship->velCurr.x;
            shipState.velocityY = player.ship->velCurr.y;
        }
        else {
            shipState.posX = 0.0f;
            shipState.posY = 0.0f;
            shipState.dirCurr = 0.0f;
            shipState.velocityX = 0.0f;
            shipState.velocityY = 0.0f;
        }

        shipState.score = player.score;
        shipState.lives = player.lives;
    }

    // Add asteroids data
    AsteroidState* asteroidStates = reinterpret_cast<AsteroidState*>(
        buffer.data() + sizeof(GameStateMessage) + playerStateSize);

    for (size_t i = 0; i < asteroids.size(); i++) {
        GameObjInst* asteroid = asteroids[i];
        AsteroidState& asteroidState = asteroidStates[i];

        asteroidState.id = static_cast<uint16_t>(i);
        asteroidState.active = (asteroid->flag & FLAG_ACTIVE) != 0;
        asteroidState.posX = asteroid->posCurr.x;
        asteroidState.posY = asteroid->posCurr.y;
        asteroidState.velocityX = asteroid->velCurr.x;
        asteroidState.velocityY = asteroid->velCurr.y;
        asteroidState.scale = asteroid->scale.x;
    }

    // Add bullets data
    BulletState* bulletStates = reinterpret_cast<BulletState*>(
        buffer.data() + sizeof(GameStateMessage) + playerStateSize + asteroidStateSize);

    for (size_t i = 0; i < bullets.size(); i++) {
        GameObjInst* bullet = bullets[i];
        BulletState& bulletState = bulletStates[i];

        bulletState.id = static_cast<uint16_t>(i);
        bulletState.active = (bullet->flag & FLAG_ACTIVE) != 0;
        bulletState.ownerID = bullet->clientID;
        bulletState.posX = bullet->posCurr.x;
        bulletState.posY = bullet->posCurr.y;
        bulletState.velocityX = bullet->velCurr.x;
        bulletState.velocityY = bullet->velCurr.y;
    }

    // Send the game state to all clients
    server.BroadcastToAll(buffer.data(), buffer.size());
}

void GameServer::ResetGame() {
    std::lock_guard<std::mutex> lockPlayers(playersMutex);
    std::lock_guard<std::mutex> lockObjects(gameObjectsMutex);

    // Clear all game objects
    for (auto* asteroid : asteroids) {
        if (asteroid) {
            gameObjInstDestroy(asteroid);
        }
    }
    asteroids.clear();

    for (auto* bullet : bullets) {
        if (bullet) {
            gameObjInstDestroy(bullet);
        }
    }
    bullets.clear();

    // Reset player data and create ships
    for (auto& pair : players) {
        ClientID clientID = pair.first;
        PlayerData& player = pair.second;

        if (player.ship) {
            gameObjInstDestroy(player.ship);
            player.ship = nullptr;
        }

        player.score = 0;
        player.lives = INITIAL_LIVES;
        player.isAlive = true;

        // Create new ship for the player
        CreatePlayerShip(clientID);
    }

    // Create initial asteroids
    CreateInitialAsteroids();

    gameStateTimer = 0.0f;
}

void GameServer::CreatePlayerShip(ClientID clientID) {
    std::lock_guard<std::mutex> lock(playersMutex);

    auto it = players.find(clientID);
    if (it == players.end()) {
        return;
    }

    // Calculate spawn position based on player number
    float spawnAngle = (static_cast<float>(clientID) - 1) * (2.0f * PI / 4.0f);
    float spawnDist = 100.0f;
    float spawnX = cosf(spawnAngle) * spawnDist;
    float spawnY = sinf(spawnAngle) * spawnDist;

    // Create ship
    AEVec2 scale, pos;
    AEVec2Set(&scale, SHIP_SCALE_X * 2.5f, SHIP_SCALE_Y * 2.5f);
    AEVec2Set(&pos, spawnX, spawnY);

    GameObjInst* ship = gameObjInstCreate(TYPE_SHIP, &scale, &pos, nullptr, spawnAngle + PI);

    if (ship) {
        // Store client ID with the ship
        ship->clientID = clientID;

        // Store ship in player data
        it->second.ship = ship;
        it->second.isAlive = true;
    }
}

void GameServer::RemovePlayerShip(ClientID clientID) {
    std::lock_guard<std::mutex> lock(playersMutex);

    auto it = players.find(clientID);
    if (it != players.end() && it->second.ship) {
        gameObjInstDestroy(it->second.ship);
        it->second.ship = nullptr;
        it->second.isAlive = false;
    }
}

void GameServer::CreateInitialAsteroids() {
    std::lock_guard<std::mutex> lock(gameObjectsMutex);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> disPos(-250.0f, 250.0f);
    std::uniform_real_distribution<float> disVel(-60.0f, 60.0f);
    std::uniform_real_distribution<float> disScale(0.8f, 1.5f);

    for (unsigned int i = 0; i < INITIAL_ASTEROID_COUNT; i++) {
        // Generate position away from the center (where players spawn)
        float posX, posY;
        float distFromCenter;

        do {
            posX = disPos(gen);
            posY = disPos(gen);
            distFromCenter = sqrtf(posX * posX + posY * posY);
        } while (distFromCenter < 150.0f); // Keep asteroids away from player spawn positions

        CreateAsteroid(posX, posY, disVel(gen), disVel(gen),
            ASTEROID_MAX_SCALE_X * disScale(gen));
    }
}

void GameServer::CreateAsteroid(float x, float y, float velX, float velY, float scale) {
    std::lock_guard<std::mutex> lock(gameObjectsMutex);

    AEVec2 pos, vel, scaleVec;
    AEVec2Set(&pos, x, y);
    AEVec2Set(&vel, velX, velY);
    AEVec2Set(&scaleVec, scale, scale);

    GameObjInst* asteroid = gameObjInstCreate(TYPE_ASTEROID, &scaleVec, &pos, &vel, 0.0f);

    if (asteroid) {
        asteroids.push_back(asteroid);
    }
}

void GameServer::SplitAsteroid(GameObjInst* asteroid) {
    if (!asteroid || !(asteroid->flag & FLAG_ACTIVE)) {
        return;
    }

    // Create two smaller asteroids
    float newScale = asteroid->scale.x * 0.6f;

    if (newScale < ASTEROID_MIN_SCALE_X) {
        return; // Too small to split
    }

    // Calculate split velocities (perpendicular to original)
    float perpX = -asteroid->velCurr.y;
    float perpY = asteroid->velCurr.x;
    float perpLen = sqrtf(perpX * perpX + perpY * perpY);

    if (perpLen > 0.0f) {
        perpX /= perpLen;
        perpY /= perpLen;
    }

    float splitSpeed = 30.0f;

    // Create first fragment
    float vel1X = asteroid->velCurr.x * 0.8f + perpX * splitSpeed;
    float vel1Y = asteroid->velCurr.y * 0.8f + perpY * splitSpeed;

    CreateAsteroid(asteroid->posCurr.x, asteroid->posCurr.y, vel1X, vel1Y, newScale);

    // Create second fragment
    float vel2X = asteroid->velCurr.x * 0.8f - perpX * splitSpeed;
    float vel2Y = asteroid->velCurr.y * 0.8f - perpY * splitSpeed;

    CreateAsteroid(asteroid->posCurr.x, asteroid->posCurr.y, vel2X, vel2Y, newScale);
}