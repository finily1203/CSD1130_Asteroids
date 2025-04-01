// UDPNetwork.h
#ifndef UDP_NETWORK_H
#define UDP_NETWORK_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <queue>
#include <functional>

// Maximum size for UDP packets
constexpr size_t MAX_PACKET_SIZE = 1024;

// Client ID type
typedef uint8_t ClientID;

// Network message types
enum class MessageType : uint8_t {
    CONNECT_REQUEST = 1,
    CONNECT_ACCEPT = 2,
    CONNECT_REJECT = 3,
    DISCONNECT = 4,
    GAME_STATE = 5,
    PLAYER_INPUT = 6,
    GAME_START = 7,
    GAME_END = 8,
    HEARTBEAT = 9
};

// Base message structure
#pragma pack(push, 1)
struct NetworkMessage {
    MessageType type;
    ClientID clientID;
    uint16_t sequence;

    NetworkMessage() : type(MessageType::HEARTBEAT), clientID(0), sequence(0) {}
    NetworkMessage(MessageType t, ClientID id, uint16_t seq) : type(t), clientID(id), sequence(seq) {}
};

// Player input message
struct PlayerInputMessage : NetworkMessage {
    bool up;
    bool down;
    bool left;
    bool right;
    bool fire;

    PlayerInputMessage() : NetworkMessage(MessageType::PLAYER_INPUT, 0, 0),
        up(false), down(false), left(false), right(false), fire(false) {
    }
};

// Ship state data
struct ShipState {
    float posX;
    float posY;
    float dirCurr;
    float velocityX;
    float velocityY;
    bool active;
    uint32_t score;
    uint8_t lives;

    ShipState() : posX(0), posY(0), dirCurr(0), velocityX(0), velocityY(0),
        active(true), score(0), lives(3) {
    }
};

// Asteroid state data
struct AsteroidState {
    uint16_t id;
    float posX;
    float posY;
    float velocityX;
    float velocityY;
    float scale;
    bool active;

    AsteroidState() : id(0), posX(0), posY(0), velocityX(0), velocityY(0),
        scale(1.0f), active(true) {
    }
};

// Bullet state data
struct BulletState {
    uint16_t id;
    ClientID ownerID;
    float posX;
    float posY;
    float velocityX;
    float velocityY;
    bool active;

    BulletState() : id(0), ownerID(0), posX(0), posY(0), velocityX(0), velocityY(0), active(true) {}
};

// Game state message
struct GameStateMessage : NetworkMessage {
    uint8_t playerCount;
    uint16_t asteroidCount;
    uint16_t bulletCount;
    uint8_t gameStatus; // 0 = waiting, 1 = in progress, 2 = game over

    // Variable-length data follows:
    // ShipState[playerCount] - ship states
    // AsteroidState[asteroidCount] - asteroid states
    // BulletState[bulletCount] - bullet states

    GameStateMessage() : NetworkMessage(MessageType::GAME_STATE, 0, 0),
        playerCount(0), asteroidCount(0), bulletCount(0), gameStatus(0) {
    }
};

// Connection accept message
struct ConnectAcceptMessage : NetworkMessage {
    ClientID assignedID;
    uint8_t totalPlayers;

    ConnectAcceptMessage() : NetworkMessage(MessageType::CONNECT_ACCEPT, 0, 0),
        assignedID(0), totalPlayers(0) {
    }
};

// Game end message
struct GameEndMessage : NetworkMessage {
    ClientID winnerID;
    uint32_t winnerScore;
    uint32_t scores[4]; // Array of scores for all players

    GameEndMessage() : NetworkMessage(MessageType::GAME_END, 0, 0),
        winnerID(0), winnerScore(0) {
        for (int i = 0; i < 4; i++) scores[i] = 0;
    }
};
#pragma pack(pop)

// Client connection data for server
struct ClientConnection {
    sockaddr_in address;
    std::string ip;
    uint16_t port;
    ClientID id;
    bool active;
    uint16_t lastReceivedSequence;
    std::chrono::steady_clock::time_point lastHeartbeatTime;

    ClientConnection() : id(0), active(false), lastReceivedSequence(0) {}
};

// UDPServer class
class UDPServer {
public:
    UDPServer();
    ~UDPServer();

    bool Initialize(uint16_t port);
    void Shutdown();

    bool IsRunning() const { return isRunning; }

    // Send data to specific client
    bool SendToClient(ClientID clientID, const void* data, size_t size);

    // Broadcast data to all clients
    bool BroadcastToAll(const void* data, size_t size);

    // Get connected client count
    size_t GetClientCount() const;

    // Check if a client is connected
    bool IsClientConnected(ClientID clientID) const;

    // Set callbacks for message handling
    void SetConnectCallback(std::function<void(ClientID)> callback) { onClientConnect = callback; }
    void SetDisconnectCallback(std::function<void(ClientID)> callback) { onClientDisconnect = callback; }
    void SetMessageCallback(std::function<void(ClientID, const void*, size_t)> callback) { onMessage = callback; }

private:
    void NetworkThread();
    void CheckClientTimeouts();
    void ProcessIncomingMessages();
    bool HandleConnectionRequest(const sockaddr_in& clientAddr);

    SOCKET socket;
    std::atomic<bool> isRunning;
    std::thread networkThread;

    mutable std::mutex clientsMutex;
    std::map<ClientID, ClientConnection> clients;
    ClientID nextClientID;

    std::function<void(ClientID)> onClientConnect;
    std::function<void(ClientID)> onClientDisconnect;
    std::function<void(ClientID, const void*, size_t)> onMessage;
};

// UDPClient class
class UDPClient {
public:
    UDPClient();
    ~UDPClient();

    bool Initialize();
    void Shutdown();

    bool IsConnected() const { return isConnected; }
    ClientID GetClientID() const { return clientID; }

    // Connect to server
    bool Connect(const std::string& serverIP, uint16_t serverPort);

    // Disconnect from server
    void Disconnect();

    // Send data to server
    bool SendToServer(const void* data, size_t size);

    // Set callbacks for message handling
    void SetConnectCallback(std::function<void(ClientID)> callback) { onConnect = callback; }
    void SetDisconnectCallback(std::function<void()> callback) { onDisconnect = callback; }
    void SetMessageCallback(std::function<void(const void*, size_t)> callback) { onMessage = callback; }

private:
    void NetworkThread();
    void SendHeartbeat();

    SOCKET socket;
    std::atomic<bool> isRunning;
    std::atomic<bool> isConnected;
    std::thread networkThread;

    sockaddr_in serverAddr;
    ClientID clientID;
    uint16_t sequenceNumber;

    std::function<void(ClientID)> onConnect;
    std::function<void()> onDisconnect;
    std::function<void(const void*, size_t)> onMessage;
};

#endif // UDP_NETWORK_H