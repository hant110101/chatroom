#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define MAX_MESSAGE_LENGTH 256
#define MAX_ROOMS 100
#define MAX_ROOM_NAME_LENGTH 100
#define MAX_ROOM_USERS 100

struct Room {
    char name[MAX_ROOM_NAME_LENGTH];
    int clients[MAX_ROOM_USERS];
    int numClients;
};

struct Client {
    int socket;
    char username[100];
};

int numClients = 0;
struct Client clients[MAX_CLIENTS];
int numRooms = 0;
struct Room rooms[MAX_ROOMS];
pthread_mutex_t lock;

void addClient(int socket, char *username) {
    pthread_mutex_lock(&lock);
    clients[numClients].socket = socket;
    strcpy(clients[numClients].username, username);
    numClients++;
    pthread_mutex_unlock(&lock);
}

void removeClient(int socket) {
    pthread_mutex_lock(&lock);
    int i, j;
    for (i = 0; i < numClients; i++) {
        if (clients[i].socket == socket) {
            break;
        }
    }
    for (j = i; j < numClients - 1; j++) {
        clients[j] = clients[j + 1];
    }
    numClients--;
}


int findClient(int socket) {
    int i;
    for (i = 0; i < numClients; i++) {
        if (clients[i].socket == socket) {
            return i;
        }
    }
    return -1;
}

int addRoom(char *name) {
    pthread_mutex_lock(&lock);
    int i;
    for (i = 0; i < numRooms; i++) {
        if (strcmp(rooms[i].name, name) == 0) {
            pthread_mutex_unlock(&lock);
            return i;
        }
    }
    strcpy(rooms[numRooms].name, name);
    rooms[numRooms].numClients = 0;
    numRooms++;
    pthread_mutex_unlock(&lock);
    return numRooms - 1;
}

void addClientToRoom(int clientIndex, int roomIndex) {
    pthread_mutex_lock(&lock);
    rooms[roomIndex].clients[rooms[roomIndex].numClients] = clientIndex;
    rooms[roomIndex].numClients++;
    pthread_mutex_unlock(&lock);
}

void removeClientFromRoom(int clientIndex, int roomIndex) {
    pthread_mutex_lock(&lock);
    int i, j;
    for (i = 0; i < rooms[roomIndex].numClients; i++) {
        if (rooms[roomIndex].clients[i] == clientIndex) {
            break;
        }
    }
    for (j = i; j < rooms[roomIndex].numClients - 1; j++) {
        rooms[roomIndex].clients[j] = rooms[roomIndex].clients[j + 1];
    }
    rooms[roomIndex].numClients--;
    pthread_mutex_unlock(&lock);
}

void sendMessageToRoom(char *message, int sender, int roomIndex) {
    int i;
    for (i = 0; i < rooms[roomIndex].numClients; i++) {
        int clientIndex = rooms[roomIndex].clients[i];
        if (clientIndex != sender) {
            write(clients[clientIndex].socket, message, strlen(message));
        }
    }
}

void *handleClient(void *arg) {
    int socket = *((int *)arg);
    char message[MAX_MESSAGE_LENGTH];
    int login = 0;
    int roomIndex = -1;
    int clientIndex;

    while (1) {
        int recvSize = recv(socket, message, MAX_MESSAGE_LENGTH, 0);
        if (recvSize <= 0) {
            break;
        }
        message[recvSize] = '\0';
        if (!login) {
            addClient(socket, message);
            clientIndex = findClient(socket);
            sprintf(message, "Welcome, %s!\n", clients[clientIndex].username);
            write(socket, message, strlen(message));
            login = 1;
        } else {
            char *command = strtok(message, " ");
            if (strcmp(command, "JOIN") == 0) {
                char *roomName = strtok(NULL, " ");
                roomIndex = addRoom(roomName);
                addClientToRoom(clientIndex, roomIndex);
                sprintf(message, "%s joined the room %s\n", clients[clientIndex].username, roomName);
                sendMessageToRoom(message, clientIndex, roomIndex);
            } else if (strcmp(command, "LEAVE") == 0) {
                if (roomIndex >= 0) {
                    sprintf(message, "%s left the room %s\n", clients[clientIndex].username, rooms[roomIndex].name);
                    sendMessageToRoom(message, clientIndex, roomIndex);
                    removeClientFromRoom(clientIndex, roomIndex);
                    roomIndex = -1;
                } else {
                    sprintf(message, "You are not in any room\n");
                    write(socket, message, strlen(message));
                }
            } else if (strcmp(command, "EXIT") == 0) {
                sprintf(message, "%s has left the chat\n", clients[clientIndex].username);
                sendMessageToRoom(message, clientIndex, roomIndex);
                removeClientFromRoom(clientIndex, roomIndex);
                break;
            } else {
                if (roomIndex >= 0) {
                    sprintf(message + strlen(message), "[%s] %s: %s\n", rooms[roomIndex].name, clients[clientIndex].username, message);
                    sendMessageToRoom(message, clientIndex, roomIndex);
                } else {
                    sprintf(message, "You are not in any room\n");
                    write(socket, message, strlen(message));
                }
            }
        }
    }

    removeClient(socket);
    close(socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    int opt = 1;
    int addrLen = sizeof(clientAddr);
    pthread_t tid;

    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, (socklen_t*)&addrLen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        pthread_create(&tid, NULL, handleClient, &clientSocket);
        pthread_detach(tid);
    }

    return 0;
}



