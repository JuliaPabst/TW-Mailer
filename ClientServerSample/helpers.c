#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUF 1024
#define MESSAGE_BUF 4096 // Larger buffer for structured message

void sendMessage(int socket, const char *message) {
    printf("DEBUG: Sending message: '%s'\n", message);
    if (send(socket, message, strlen(message), 0) == -1) {
        perror("Error sending message");
    }
}

int main(int argc, char **argv) {
    int create_socket;
    char buffer[BUF];
    char message[MESSAGE_BUF];
    struct sockaddr_in address;
    int size;

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
        perror("Connection error - no server available");
        return EXIT_FAILURE;
    }

    printf("Connection with server (%s:%d) established\n", ip, port);

    ////////////////////////////////////////////////////////////////////////////
    // RECEIVE WELCOME MESSAGE
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size > 0) {
        buffer[size] = '\0';
        printf("%s\n", buffer);
    }

    ////////////////////////////////////////////////////////////////////////////
    // MAIN LOOP
    while (1) {
        printf(">> ");
        if (fgets(buffer, BUF, stdin) != NULL) {
            size = strlen(buffer);

            // Ensure the input ends with a newline
            if (buffer[size - 1] == '\n') {
                buffer[size - 1] = '\0'; // Replace newline with null-terminator
            }

            // Handle "QUIT" command
            if (strcmp(buffer, "QUIT") == 0) {
                sendMessage(create_socket, buffer);
                break;
            }

            // Handle "SEND" command
            if (strcmp(buffer, "SEND") == 0) {
                sendMessage(create_socket, buffer);

                // Collect the entire message in structured format
                snprintf(message, sizeof(message), "{\n");

                const char *prompts[] = {"Sender", "Receiver", "Subject", "Message"};
                char field[BUF];
                for (int i = 0; i < 4; ++i) {
                    printf(">> %s: ", prompts[i]);
                    if (fgets(field, BUF, stdin) != NULL) {
                        size = strlen(field);
                        if (field[size - 1] == '\n') {
                            field[size - 1] = '\0'; // Remove newline
                        }

                        // Handle multi-line message
                        if (i == 3) {
                            strcat(message, "  \"Message\": \"");
                            strcat(message, field);
                            strcat(message, "\\n");
                            while (1) {
                                printf(">> ");
                                if (fgets(field, BUF, stdin) != NULL) {
                                    size = strlen(field);
                                    if (field[size - 1] == '\n') {
                                        field[size - 1] = '\0'; // Remove newline
                                    }
                                    if (strcmp(field, ".") == 0) {
                                        strcat(message, "\"\n");
                                        break;
                                    }
                                    strcat(message, field);
                                    strcat(message, "\\n");
                                }
                            }
                        } else {
                            char temp[BUF];
                            snprintf(temp, sizeof(temp), "  \"%s\": \"%s\",\n", prompts[i], field);
                            strcat(message, temp);
                        }
                    }
                }

                strcat(message, "}\n");
                sendMessage(create_socket, message);

                // Receive server response
                size = recv(create_socket, buffer, BUF - 1, 0);
                if (size > 0) {
                    buffer[size] = '\0';
                    printf("<< %s\n", buffer);
                }
                continue;
            }

            // Send other commands and process responses
            sendMessage(create_socket, buffer);

            size = recv(create_socket, buffer, BUF - 1, 0);
            if (size > 0) {
                buffer[size] = '\0';
                printf("<< %s\n", buffer);
            }
        }
    }

    shutdown(create_socket, SHUT_RDWR);
    close(create_socket);
    return EXIT_SUCCESS;
}
