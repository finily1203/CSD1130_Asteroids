// GameServer.h
#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include "UDPNetwork.h"
#include "main.h"
#include <vector>
#include <map>
#include <mutex>

// Forward declarations
struct GameObjInst;

// Game server class
class GameServer {
public:
    GameServer();
    ~GameServer();

    // Initialize the server
    bool Initialize(uint16_t port);

    // Shutdown the server
    void Shutdown();

    // Run one frame of the game server
    void Update(float dt);

    // Get current player count
    size_t GetPlayerCount() const { return server.GetClientCount(); }

    // Get if the server is running
    bool IsRunning() const { return isRunning; }

private:
    // Network event handlers
    void OnClientConnect(ClientID clientID);
    void OnClientDisconnect(ClientID clientID);
    void OnMessage(ClientID clientID, const void* data, size_t size);

    // Process player input
    void ProcessPlayerInput(ClientID clientID, const PlayerInputMessage* inputMsg);

    // Game state management
    void UpdateGameState(float dt);
    void CheckForCollisions();
    void CheckGameEndConditions();
    void SendGameState();
    void ResetGame();

    // Player management
    void CreatePlayerShip(ClientID clientID);
    void RemovePlayerShip(ClientID clientID);

    // Asteroid management
    void CreateInitialAsteroids();
    void CreateAsteroid(float x, float y, float velX, float velY, float scale);
    void SplitAsteroid(GameObjInst* asteroid);

    UDPServer server;
    bool isRunning;
    bool gameInProgress;
    float gameStateTimer;        // Time since last game state broadcast
    float gameEndTimer;          // Timer for game end state

    // Player data
    struct PlayerData {
        GameObjInst* ship;
        bool isAlive;
        uint32_t score;
        uint8_t lives;
        PlayerInputMessage lastInput;
    };

    std::map<ClientID, PlayerData> players;
    std::mutex playersMutex;

    // Game objects
    std::vector<GameObjInst*> asteroids;
    std::vector<GameObjInst*> bullets;
    std::mutex gameObjectsMutex;

    // Game settings
    static constexpr float GAME_STATE_UPDATE_INTERVAL = 1.0f / 20.0f;  // 20 updates per second
    static constexpr float GAME_END_DURATION = 5.0f;                   // 5 seconds for end game screen
    static constexpr unsigned int INITIAL_ASTEROID_COUNT = 4;
    static constexpr unsigned int MAX_ASTEROID_COUNT = 20;
    static constexpr unsigned int INITIAL_LIVES = 3;
    static constexpr float BULLET_LIFETIME = 2.0f;                     // Bullets live for 2 seconds
};

#endif // GAME_SERVER_H