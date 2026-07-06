#include <iostream>
#include <vector>
#include <map>
#include <cstring>

#ifndef _WIN32
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <cerrno>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket(s) close(s)
    void Sleep(unsigned long ms) { usleep(ms * 1000); }
    int GetLastNetworkError() { return errno; } 
#else
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    int GetLastNetworkError() { return WSAGetLastError(); }
#endif

enum PacketType {
    PACKET_PLAYER_DATA = 1,
    PACKET_BLOCK_ACTION = 2
};

struct PlayerNetworkData {
    int id;
    float posX, posY, posZ;
    bool isShooting;
    float targetX, targetY, targetZ;
    int health; 
    char chatMsg[64]; 
};

struct BlockActionPacket {
    int x, y, z;
    int blockType; 
};

struct NetworkPacket {
    int type;
    union {
        PlayerNetworkData playerData;
        BlockActionPacket blockData;
    } data;
};

std::map<SOCKET, int> clientMap;
int nextPlayerId = 1;

int main() {
    int port = 1234; 

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;
#endif

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) return 1;

#ifndef _WIN32
    int flags = fcntl(serverSocket, F_GETFL, 0);
    fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK);
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#else
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode);
#endif

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    listen(serverSocket, 32);
    std::cout << "Fortnite BR Sunucusu 1234 Portunda Aktif!" << std::endl;

    std::vector<SOCKET> clients;

    while (true) {
        sockaddr_in clientAddr;
#ifndef _WIN32
        socklen_t clientSize = sizeof(clientAddr);
#else
        int clientSize = sizeof(clientAddr);
#endif
        SOCKET newClient = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        
        if (newClient != INVALID_SOCKET) {
#ifndef _WIN32
            int c_flags = fcntl(newClient, F_GETFL, 0);
            fcntl(newClient, F_SETFL, c_flags | O_NONBLOCK);
#else
            ioctlsocket(newClient, FIONBIO, &mode);
#endif
            clients.push_back(newClient);
            int assignedId = nextPlayerId++;
            clientMap[newClient] = assignedId;
            send(newClient, (char*)&assignedId, sizeof(int), 0);
            std::cout << "Oyuncu Baglandi. ID: " << assignedId << std::endl;
        }

        for (auto it = clients.begin(); it != clients.end();) {
            SOCKET client = *it;
            NetworkPacket packet;
            int bytesReceived = recv(client, (char*)&packet, sizeof(NetworkPacket), 0);

            if (bytesReceived == sizeof(NetworkPacket)) {
                for (SOCKET otherClient : clients) {
                    if (otherClient != client) {
                        send(otherClient, (char*)&packet, sizeof(NetworkPacket), 0);
                    }
                }
                it++;
            } 
            else if (bytesReceived == 0 || (bytesReceived == SOCKET_ERROR && 
#ifndef _WIN32
                GetLastNetworkError() != EAGAIN && GetLastNetworkError() != EWOULDBLOCK
#else
                GetLastNetworkError() != WSAEWOULDBLOCK
#endif
            )) {
                int disconnectedId = clientMap[client];
                std::cout << "Oyuncu Ayrildi: ID " << disconnectedId << std::endl;

                clientMap.erase(client);
                closesocket(client);
                it = clients.erase(it);

                NetworkPacket dcPacket;
                dcPacket.type = PACKET_PLAYER_DATA;
                dcPacket.data.playerData = { disconnectedId, 0, -999.0f, 0, false, 0, 0, 0, 0, "" };
                
                for (SOCKET otherClient : clients) {
                    send(otherClient, (char*)&dcPacket, sizeof(NetworkPacket), 0);
                }
            } else {
                it++;
            }
        }
        Sleep(10);
    }

    closesocket(serverSocket);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
