#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <serverHost> <serverPort> <command> [args...]\n", argv[0]);
        fprintf(stderr, "Example: %s localhost 2200 A 147.188.193.15 22\n", argv[0]);
        return 1;
    }

    char *server_host = argv[1];
    int server_port = atoi(argv[2]);

    if (server_port <= 0 || server_port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    char command[BUFFER_SIZE] = "";
    for (int i = 3; i < argc; i++) {
        strcat(command, argv[i]);
        if (i < argc - 1) {
            strcat(command, " ");
        }
    }

    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket creation error\n");
        return 1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_host, &serv_addr.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(server_host);
        if (he == NULL) {
            fprintf(stderr, "Invalid address/ Address not supported\n");
            close(sock);
            return 1;
        }
        memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Connection Failed\n");
        close(sock);
        return 1;
    }

    if (send(sock, command, strlen(command), 0) < 0) {
        fprintf(stderr, "Failed to send command\n");
        close(sock);
        return 1;
    }

    char buffer[BUFFER_SIZE] = {0};
    int valread = read(sock, buffer, BUFFER_SIZE);
    if (valread < 0) {
        fprintf(stderr, "Failed to receive response\n");
        close(sock);
        return 1;
    }

    printf("%s", buffer);

    close(sock);
    return 0;
}