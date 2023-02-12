#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_LENGTH 1024

char username[32];

void printInstructions() {
    printf("\nCommands:\n");
    printf("JOIN [room_name] - join a room\n");
    printf("LEAVE - leave the current room\n");
    printf("EXIT - exit the chat\n");
}

void *receiveMessage(void *socket) {
    int sockfd = *(int *)socket;
    char message[MAX_LENGTH];
    int length;

    while ((length = recv(sockfd, message, MAX_LENGTH, 0)) > 0) {
        message[length] = '\0';
        printf("%s\n", message);
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in serverAddr;
    char message[MAX_LENGTH];
    pthread_t tid;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    printf("Enter your username: \n");
    scanf("%s", username);
        send(sockfd, username, strlen(username), 0);
    pthread_create(&tid, NULL, receiveMessage, &sockfd);

    printInstructions();

    while (1) {
        memset(message, 0, MAX_LENGTH);
        fgets(message, MAX_LENGTH, stdin);
        send(sockfd, message, strlen(message), 0);

        if (strcmp(message, "EXIT\n") == 0) {
            break;
        }
    }

    close(sockfd);
    pthread_exit(NULL);

    return 0;
}
