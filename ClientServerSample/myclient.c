#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define BUF 1024

void sendMessage(int socket, const char *message) {
    // printf("DEBUG: Sending message: '%s'\n", message);
    if (send(socket, message, strlen(message) + 1, 0) == -1) { // +1 for null terminator
        perror("Error sending message");
    }
}

int isValidInput(const char *input, int maxLength) {
    int len = strlen(input);
    return len > 0 && len <= maxLength;
}

void handleSendCommand(int create_socket) {
    char buffer[BUF];
    int size;

    // Send "SEND" command to the server
    sendMessage(create_socket, "SEND");

    // Collect sender, receiver, subject, and message
    const char *prompts[] = {"Sender (max 8 chars)", "Receiver (max 8 chars)", "Subject (max 80 chars)", "Message"};
    int maxLengths[] = {8, 8, 80, BUF - 1};

    for (int i = 0; i < 4; i++) {
        while (1) { // Retry loop for invalid input
            memset(buffer, 0, sizeof(buffer)); // Clear the buffer
            printf(">> %s: ", prompts[i]);
            if (fgets(buffer, BUF - 1, stdin) != NULL) {
                size = strlen(buffer);
                if (buffer[size - 1] == '\n') {
                    buffer[--size] = '\0'; // Remove newline
                }

                if (i < 3 && !isValidInput(buffer, maxLengths[i])) { // Validate sender, receiver, subject
                    printf("Invalid %s! Try again.\n", prompts[i]);
                    continue; // Retry current prompt
                }

                if (i == 3) { // Multi-line input for "Message"
                    sendMessage(create_socket, buffer); // Send first line of the message
                    while (1) {
                        printf(">> ");
                        memset(buffer, 0, sizeof(buffer)); // Clear buffer
                        if (fgets(buffer, BUF - 1, stdin) != NULL) {
                            size = strlen(buffer);
                            if (buffer[size - 1] == '\n') {
                                buffer[--size] = '\0';
                            }
                            if (strcmp(buffer, ".") == 0) { // End of message
                                sendMessage(create_socket, buffer);
                                break;
                            }
                            sendMessage(create_socket, buffer);
                        }
                    }
                    break; // Exit the retry loop for the message
                } else {
                    sendMessage(create_socket, buffer);
                    break; // Valid input, move to the next prompt
                }
            }
        }
    }

    // Wait for server response
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size > 0) {
        buffer[size] = '\0';
        printf("<< %s\n", buffer); // Print server response
    } else {
        perror("recv error");
    }
}


void handleListCommand(int create_socket) {
    char buffer[BUF];
    int size;

    // Send "LIST" command to the server
    sendMessage(create_socket, "LIST");

    // Prompt user for username until a valid one is provided
    while (1) { 
        printf(">> Username (max 8 chars): ");
        if (fgets(buffer, BUF - 1, stdin) != NULL) {
            size_t len = strlen(buffer);
            if (buffer[len - 1] == '\n') buffer[--len] = '\0'; // Remove newline

            // Check if username is valid
            if (!isValidInput(buffer, 8)) {
                printf("Invalid username! Try again.\n");
                continue;
            }

            sendMessage(create_socket, buffer);
            break;
        }
    }

    // Receive response from the server
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size > 0) {
        buffer[size] = '\0'; // Null-terminate string

        // Check if response is about no messages
        if (strstr(buffer, "This user has not received any messages yet!") != NULL) {
            printf("<< %s", buffer);
        } else {
            char *line = strtok(buffer, "\n");
            if (line && isdigit(line[0])) { // Check if first line is the count of messages
                int count = atoi(line);
                printf("<< Messages received: %d\n", count);
                int counter = 1;
                while ((line = strtok(NULL, "\n")) != NULL) {
                    printf("%d. %s\n", counter, line); // Print subjects of messages
                    counter++; 
                }
                printf("\n");
            } else {
                printf("<< %s\n", buffer); // Print error or unexpected response
            }
        }
    } else {
        perror("recv error");
    }
}




int main(int argc, char **argv) {
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
        perror("Connection error - no server available");
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

            if(strcmp(buffer, "SEND") == 0 || strcmp(buffer, "LIST") == 0){
                // Handle "SEND" command
                if (strcmp(buffer, "SEND") == 0) {
                    handleSendCommand(create_socket);

                    // Receive server response
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
                    }
                }   

                // Handle "LIST" command
                if (strcmp(buffer, "LIST") == 0) {
                    handleListCommand(create_socket);
                } 

                
                continue; // Skip further processing
            }

            isQuit = strcmp(buffer, "QUIT") == 0;

            // Send other commands to the server
            sendMessage(create_socket, buffer);

            // Receive response from the server
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
            }
        }
    } while (!isQuit);

    if (create_socket != -1) {
        shutdown(create_socket, SHUT_RDWR);
        close(create_socket);
    }

    return EXIT_SUCCESS;
}