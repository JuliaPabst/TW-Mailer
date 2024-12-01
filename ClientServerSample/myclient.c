#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUF 1024

int main(int argc, char **argv)
{
    int create_socket;
    char buffer[BUF];
    struct sockaddr_in address;
    int size;
    int isQuit;

    ////////////////////////////////////////////////////////////////////////////
    // Check Command-Line Arguments
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////////////////////////////////
    // CREATE A SOCKET
    if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket error");
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////////////////////////////////
    // INIT ADDRESS
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (inet_aton(ip, &address.sin_addr) == 0) {
        fprintf(stderr, "Invalid IP address: %s\n", ip);
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////////////////////////////////
    // CREATE A CONNECTION
    if (connect(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("Connect error - no server available");
        return EXIT_FAILURE;
    }

    printf("Connection with server (%s:%d) established\n", ip, port);

    ////////////////////////////////////////////////////////////////////////////
    // RECEIVE DATA
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size == -1) {
        perror("recv error");
    } else if (size == 0) {
        printf("Server closed remote socket\n");
    } else {
        buffer[size] = '\0';
        printf("%s", buffer);
    }

    do {
        printf(">> ");
        if (fgets(buffer, BUF - 1, stdin) != NULL) {
            size = strlen(buffer);

            if (buffer[size - 1] == '\n') {
                buffer[--size] = '\0';
            }

            isQuit = strcmp(buffer, "quit") == 0;

            if (send(create_socket, buffer, size + 1, 0) == -1) {
                perror("send error");
                break;
            }

            size = recv(create_socket, buffer, BUF - 1, 0);
            if (size == -1) {
                perror("recv error");
                break;
            } else if (size == 0) {
                printf("Server closed remote socket\n");
                break;
            } else {
                buffer[size] = '\0';
                printf("<< %s\n", buffer);
                if (strcmp("OK", buffer) != 0) {
                    fprintf(stderr, "Server error occurred, aborting.\n");
                    break;
                }
            }
        }
    } while (!isQuit);

    if (create_socket != -1) {
        shutdown(create_socket, SHUT_RDWR);
        close(create_socket);
    }

    return EXIT_SUCCESS;
}
