#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

int main(int argc, char **argv) {
    // Check command line arguments
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <serverHost> <serverPort> <command> [args...]\n", argv[0]);
        fprintf(stderr, "Example: %s localhost 2200 A 147.188.193.15 22\n", argv[0]);
        return 1;
    }

    char *server_host = argv[1];
    int server_port = atoi(argv[2]);

    // Validate port number
    if (server_port <= 0 || server_port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    // Construct command from remaining arguments
    char command[BUFFER_SIZE] = "";
    for (int i = 3; i < argc; i++) {
        strcat(command, argv[i]);
        if (i < argc - 1) {
            strcat(command, " ");
        }
    }

    // Create socket
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket creation error\n");
        return 1;
    }

    // Initialize server address structure
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port);

    // Convert hostname to IP address
    if (inet_pton(AF_INET, server_host, &serv_addr.sin_addr) <= 0) {
        // If direct IP conversion fails, try to resolve hostname
        struct hostent *he = gethostbyname(server_host);
        if (he == NULL) {
            fprintf(stderr, "Invalid address/ Address not supported\n");
            close(sock);
            return 1;
        }
        memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Connection Failed\n");
        close(sock);
        return 1;
    }

    // Send command to server
    if (send(sock, command, strlen(command), 0) < 0) {
        fprintf(stderr, "Failed to send command\n");
        close(sock);
        return 1;
    }

    // Receive response from server
    char buffer[BUFFER_SIZE] = {0};
    int valread = read(sock, buffer, BUFFER_SIZE);
    if (valread < 0) {
        fprintf(stderr, "Failed to receive response\n");
        close(sock);
        return 1;
    }

    // Print server response to stdout
    printf("%s", buffer);

    // Close socket
    close(sock);
    return 0;
}