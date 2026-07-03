#include <iostream>
#include <vector>
#include <map>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

struct PlayerNetworkData {
    int id;
    float posX, posY, posZ;
    bool isShooting;
    float targetX, targetY, targetZ;
};

std::map<SOCKET, int> clientMap;
std::map<int, PlayerNetworkData> playerPositions;
int nextPlayerId = 1;

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    u_long mode = 1;
    ioctlsocket(serverSocket, FIONBIO, &mode);

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(1234);

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 32);
    std::cout << "Temiz TCP Sunucu aktif (Port: 1234)...\n";

    std::vector<SOCKET> clients;

    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET newClient = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        
        if (newClient != INVALID_SOCKET) {
            ioctlsocket(newClient, FIONBIO, &mode);
            clients.push_back(newClient);
            int assignedId = nextPlayerId++;
            clientMap[newClient] = assignedId;
            send(newClient, (char*)&assignedId, sizeof(int), 0);
            std::cout << "Oyuncu baglandi. ID: " << assignedId << std::endl;
        }

        for (auto it = clients.begin(); it != clients.end();) {
            SOCKET client = *it;
            PlayerNetworkData incomingData;
            int bytesReceived = recv(client, (char*)&incomingData, sizeof(PlayerNetworkData), 0);

            if (bytesReceived == sizeof(PlayerNetworkData)) {
                playerPositions[incomingData.id] = incomingData;
                for (SOCKET otherClient : clients) {
                    if (otherClient != client) {
                        send(otherClient, (char*)&incomingData, sizeof(PlayerNetworkData), 0);
                    }
                }
                it++;
            } 
            else if (bytesReceived == 0 || (bytesReceived == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                int disconnectedId = clientMap[client];
                std::cout << "Oyuncu koptu, siliniyor. ID: " << disconnectedId << std::endl;
                
                playerPositions.erase(disconnectedId);
                clientMap.erase(client);
                closesocket(client);
                it = clients.erase(it);

                // Kopan oyuncuyu digerlerine bildirmek icin Y pozisyonunu -999 yapıp uçuruyoruz
                PlayerNetworkData disconnectPacket = { disconnectedId, 0, -999.0f, 0, false, 0, 0, 0 };
                for (SOCKET otherClient : clients) {
                    send(otherClient, (char*)&disconnectPacket, sizeof(PlayerNetworkData), 0);
                }
            } else {
                it++;
            }
        }
        Sleep(10);
    }
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}