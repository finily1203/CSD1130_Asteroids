// UDPNetwork.cpp
#include "UDPNetwork.h"
#include <iostream>
#include <chrono>

// =================== UDPServer Implementation ===================

UDPServer::UDPServer() : socket(INVALID_SOCKET), isRunning(false), nextClientID(1) {
    // Initialize onMessage callbacks to empty functions to avoid nullptr checks
    onClientConnect = [](ClientID) {};
    onClientDisconnect = [](ClientID) {};
    onMessage = [](ClientID, const void*, size_t) {};
}

UDPServer::~UDPServer() {
    Shutdown();
}

bool UDPServer::Initialize(uint16_t port) {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }

    // Create UDP socket
    socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    // Set up server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    // Bind the socket
    result = bind(socket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "Socket bind failed: " << WSAGetLastError() << std::endl;
        closesocket(socket);
        WSACleanup();
        return false;
    }

    // Set socket to non-blocking mode
    u_long mode = 1;
    result = ioctlsocket(socket, FIONBIO, &mode);
    if (result == SOCKET_ERROR) {
        std::cerr << "Failed to set non-blocking mode: " << WSAGetLastError() << std::endl;
        closesocket(socket);
        WSACleanup();
        return false;
    }

    // Start network thread
    isRunning = true;
    networkThread = std::thread(&UDPServer::NetworkThread, this);

    std::cout << "UDP Server initialized on port " << port << std::endl;
    return true;
}

void UDPServer::Shutdown() {
    if (isRunning) {
        isRunning = false;

        if (networkThread.joinable()) {
            networkThread.join();
        }

        if (socket != INVALID_SOCKET) {
            closesocket(socket);
            socket = INVALID_SOCKET;
        }

        WSACleanup();

        // Clear clients
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.clear();
    }
}

void UDPServer::NetworkThread() {
    std::cout << "Server network thread started" << std::endl;

    while (isRunning) {
        // Process incoming messages
        ProcessIncomingMessages();

        // Check for client timeouts
        CheckClientTimeouts();

        // Give the CPU a break
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "Server network thread stopped" << std::endl;
}

void UDPServer::ProcessIncomingMessages() {
    char buffer[MAX_PACKET_SIZE];
    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    while (isRunning) {
        // Try to receive a message
        int bytesReceived = recvfrom(socket, buffer, MAX_PACKET_SIZE, 0,
            (sockaddr*)&clientAddr, &clientAddrSize);

        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                // No more messages available
                break;
            }
            else {
                std::cerr << "recvfrom failed: " << error << std::endl;
                break;
            }
        }

        // Must receive at least the header
        if (bytesReceived < sizeof(NetworkMessage)) {
            continue;
        }

        // Get message type
        NetworkMessage* header = reinterpret_cast<NetworkMessage*>(buffer);

        // Handle message based on type
        switch (header->type) {
        case MessageType::CONNECT_REQUEST:
            HandleConnectionRequest(clientAddr);
            break;

        case MessageType::DISCONNECT:
        {
            // Find the client and disconnect them
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto& pair : clients) {
                if (pair.second.address.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
                    pair.second.address.sin_port == clientAddr.sin_port) {
                    pair.second.active = false;
                    onClientDisconnect(pair.first);
                    std::cout << "Client " << (int)pair.first << " disconnected" << std::endl;
                    break;
                }
            }
            break;
        }

        case MessageType::HEARTBEAT:
        {
            // Update client heartbeat time
            std::lock_guard<std::mutex> lock(clientsMutex);
            for (auto& pair : clients) {
                if (pair.second.address.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
                    pair.second.address.sin_port == clientAddr.sin_port) {
                    pair.second.lastHeartbeatTime = std::chrono::steady_clock::now();
                    break;
                }
            }
            break;
        }

        default:
        {
            // Find client ID and call message handler
            ClientID senderID = 0;
            bool found = false;

            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (auto& pair : clients) {
                    if (pair.second.address.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
                        pair.second.address.sin_port == clientAddr.sin_port && pair.second.active) {
                        senderID = pair.first;
                        found = true;

                        // Update heartbeat time
                        pair.second.lastHeartbeatTime = std::chrono::steady_clock::now();

                        break;
                    }
                }
            }

            if (found) {
                onMessage(senderID, buffer, bytesReceived);
            }
            break;
        }
        }
    }
}

bool UDPServer::HandleConnectionRequest(const sockaddr_in& clientAddr) {
    // Check if this client is already connected
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& pair : clients) {
            if (pair.second.address.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
                pair.second.address.sin_port == clientAddr.sin_port && pair.second.active) {
                // Client already connected, resend the accept message
                ConnectAcceptMessage response;
                response.clientID = 0; // Server ID
                response.sequence = 0;
                response.assignedID = pair.first;
                response.totalPlayers = static_cast<uint8_t>(clients.size());

                sendto(socket, reinterpret_cast<const char*>(&response), sizeof(response), 0,
                    (sockaddr*)&clientAddr, sizeof(clientAddr));
                return true;
            }
        }

        // Check if we can accept more clients (limit to 4 players)
        if (clients.size() >= 4) {
            // Send reject message
            NetworkMessage response;
            response.type = MessageType::CONNECT_REJECT;
            response.clientID = 0; // Server ID
            response.sequence = 0;

            sendto(socket, reinterpret_cast<const char*>(&response), sizeof(response), 0,
                (sockaddr*)&clientAddr, sizeof(clientAddr));
            return false;
        }

        // Accept the new client
        ClientID newID = nextClientID++;

        ClientConnection newClient;
        newClient.address = clientAddr;
        newClient.id = newID;
        newClient.active = true;
        newClient.lastHeartbeatTime = std::chrono::steady_clock::now();

        // Convert IP address to string
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
        newClient.ip = ipStr;
        newClient.port = ntohs(clientAddr.sin_port);

        clients[newID] = newClient;

        // Send accept message
        ConnectAcceptMessage response;
        response.clientID = 0; // Server ID
        response.sequence = 0;
        response.assignedID = newID;
        response.totalPlayers = static_cast<uint8_t>(clients.size());

        sendto(socket, reinterpret_cast<const char*>(&response), sizeof(response), 0,
            (sockaddr*)&clientAddr, sizeof(clientAddr));

        std::cout << "New client connected: ID=" << (int)newID
            << ", IP=" << newClient.ip
            << ", Port=" << newClient.port << std::endl;

        // Call the connect callback
        onClientConnect(newID);
        return true;
    }
}

void UDPServer::CheckClientTimeouts() {
    auto now = std::chrono::steady_clock::now();
    constexpr auto TIMEOUT_DURATION = std::chrono::seconds(5);

    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto it = clients.begin(); it != clients.end();) {
        if (it->second.active &&
            now - it->second.lastHeartbeatTime > TIMEOUT_DURATION) {
            // Client timed out
            std::cout << "Client " << (int)it->first << " timed out" << std::endl;
            it->second.active = false;
            onClientDisconnect(it->first);
            ++it;
        }
        else {
            ++it;
        }
    }
}

bool UDPServer::SendToClient(ClientID clientID, const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(clientID);
    if (it != clients.end() && it->second.active) {
        int result = sendto(socket, reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
            (sockaddr*)&it->second.address, sizeof(it->second.address));
        return result != SOCKET_ERROR;
    }
    return false;
}

bool UDPServer::BroadcastToAll(const void* data, size_t size) {
    bool success = true;
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& pair : clients) {
        if (pair.second.active) {
            int result = sendto(socket, reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
                (sockaddr*)&pair.second.address, sizeof(pair.second.address));
            if (result == SOCKET_ERROR) {
                success = false;
            }
        }
    }
    return success;
}

size_t UDPServer::GetClientCount() const {
    std::lock_guard<std::mutex> lock(clientsMutex);
    size_t count = 0;
    for (auto& pair : clients) {
        if (pair.second.active) {
            count++;
        }
    }
    return count;
}

bool UDPServer::IsClientConnected(ClientID clientID) const {
    std::lock_guard<std::mutex> lock(clientsMutex);
    auto it = clients.find(clientID);
    return (it != clients.end() && it->second.active);
}

// =================== UDPClient Implementation ===================

UDPClient::UDPClient() : socket(INVALID_SOCKET), isRunning(false), isConnected(false),
clientID(0), sequenceNumber(0) {
    // Initialize callbacks to empty functions to avoid nullptr checks
    onConnect = [](ClientID) {};
    onDisconnect = []() {};
    onMessage = [](const void*, size_t) {};
}

UDPClient::~UDPClient() {
    Shutdown();
}

bool UDPClient::Initialize() {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return false;
    }

    // Create UDP socket
    socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return false;
    }

    // Set socket to non-blocking mode
    u_long mode = 1;
    result = ioctlsocket(socket, FIONBIO, &mode);
    if (result == SOCKET_ERROR) {
        std::cerr << "Failed to set non-blocking mode: " << WSAGetLastError() << std::endl;
        closesocket(socket);
        WSACleanup();
        return false;
    }

    // Bind to a random port
    sockaddr_in clientAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = 0; // Let the system assign a port

    result = bind(socket, (sockaddr*)&clientAddr, sizeof(clientAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "Socket bind failed: " << WSAGetLastError() << std::endl;
        closesocket(socket);
        WSACleanup();
        return false;
    }

    isRunning = true;
    networkThread = std::thread(&UDPClient::NetworkThread, this);

    std::cout << "UDP Client initialized" << std::endl;
    return true;
}

void UDPClient::Shutdown() {
    Disconnect();

    if (isRunning) {
        isRunning = false;

        if (networkThread.joinable()) {
            networkThread.join();
        }

        if (socket != INVALID_SOCKET) {
            closesocket(socket);
            socket = INVALID_SOCKET;
        }

        WSACleanup();
    }
}

bool UDPClient::Connect(const std::string& serverIP, uint16_t serverPort) {
    if (isConnected) {
        std::cerr << "Already connected to server" << std::endl;
        return false;
    }

    // Set up server address
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(serverPort);

    // Send connect request message
    NetworkMessage connectMsg;
    connectMsg.type = MessageType::CONNECT_REQUEST;
    connectMsg.clientID = 0;
    connectMsg.sequence = sequenceNumber++;

    int result = sendto(socket, reinterpret_cast<const char*>(&connectMsg), sizeof(connectMsg), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr));

    if (result == SOCKET_ERROR) {
        std::cerr << "Failed to send connect request: " << WSAGetLastError() << std::endl;
        return false;
    }

    // Wait for connection response (handled in NetworkThread)
    std::cout << "Connecting to server at " << serverIP << ":" << serverPort << "..." << std::endl;

    // We don't wait here - the network thread will handle the response
    return true;
}

void UDPClient::Disconnect() {
    if (isConnected) {
        // Send disconnect message
        NetworkMessage disconnectMsg;
        disconnectMsg.type = MessageType::DISCONNECT;
        disconnectMsg.clientID = clientID;
        disconnectMsg.sequence = sequenceNumber++;

        sendto(socket, reinterpret_cast<const char*>(&disconnectMsg), sizeof(disconnectMsg), 0,
            (sockaddr*)&serverAddr, sizeof(serverAddr));

        isConnected = false;
        clientID = 0;

        onDisconnect();
        std::cout << "Disconnected from server" << std::endl;
    }
}

bool UDPClient::SendToServer(const void* data, size_t size) {
    if (!isConnected) {
        return false;
    }

    int result = sendto(socket, reinterpret_cast<const char*>(data), static_cast<int>(size), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr));

    return result != SOCKET_ERROR;
}

void UDPClient::NetworkThread() {
    std::cout << "Client network thread started" << std::endl;

    char buffer[MAX_PACKET_SIZE];
    sockaddr_in senderAddr;
    int senderAddrSize = sizeof(senderAddr);

    auto lastHeartbeatTime = std::chrono::steady_clock::now();
    constexpr auto HEARTBEAT_INTERVAL = std::chrono::seconds(1);

    while (isRunning) {
        // Send heartbeats if connected
        auto currentTime = std::chrono::steady_clock::now();
        if (isConnected && currentTime - lastHeartbeatTime > HEARTBEAT_INTERVAL) {
            SendHeartbeat();
            lastHeartbeatTime = currentTime;
        }

        // Try to receive a message
        int bytesReceived = recvfrom(socket, buffer, MAX_PACKET_SIZE, 0,
            (sockaddr*)&senderAddr, &senderAddrSize);

        if (bytesReceived == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                // No messages available, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            else {
                std::cerr << "recvfrom failed: " << error << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        // Must receive at least the header
        if (bytesReceived < sizeof(NetworkMessage)) {
            continue;
        }

        // Handle message based on type
        NetworkMessage* header = reinterpret_cast<NetworkMessage*>(buffer);

        // Check if message is from our server
        if (senderAddr.sin_addr.s_addr != serverAddr.sin_addr.s_addr ||
            senderAddr.sin_port != serverAddr.sin_port) {
            // Message from unknown source, ignore
            continue;
        }

        switch (header->type) {
        case MessageType::CONNECT_ACCEPT:
        {
            if (!isConnected) {
                ConnectAcceptMessage* msg = reinterpret_cast<ConnectAcceptMessage*>(buffer);
                clientID = msg->assignedID;
                isConnected = true;
                std::cout << "Connected to server as client " << (int)clientID << std::endl;
                onConnect(clientID);
            }
            break;
        }

        case MessageType::CONNECT_REJECT:
        {
            std::cout << "Connection rejected by server" << std::endl;
            isConnected = false;
            onDisconnect();
            break;
        }

        case MessageType::DISCONNECT:
        {
            if (isConnected) {
                std::cout << "Disconnected by server" << std::endl;
                isConnected = false;
                clientID = 0;
                onDisconnect();
            }
            break;
        }

        default:
            // Pass message to handler
            onMessage(buffer, bytesReceived);
            break;
        }
    }

    std::cout << "Client network thread stopped" << std::endl;
}

void UDPClient::SendHeartbeat() {
    if (!isConnected) {
        return;
    }

    NetworkMessage heartbeatMsg;
    heartbeatMsg.type = MessageType::HEARTBEAT;
    heartbeatMsg.clientID = clientID;
    heartbeatMsg.sequence = sequenceNumber++;

    sendto(socket, reinterpret_cast<const char*>(&heartbeatMsg), sizeof(heartbeatMsg), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr));
}