#include "myclient.h"

#define BUF 1024

void sendMessage(int socket, const char *message) {
    if (send(socket, message, strlen(message) + 1, 0) == -1) { // +1 for null terminator
        perror("Error sending message");
    }
}

int isValidInput(const char *input, int maxLength) {
    int len = strlen(input);
    return len > 0 && len <= maxLength;
}

char *handleLoginCommand(int create_socket) {
    char buffer[BUF];
    char username[256];
    char password[256];
    int size;

    // Prompt for username
    printf(">> LDAP Username: ");
    if (fgets(username, sizeof(username), stdin) == NULL) {
        perror("Error reading username");
        return NULL; // Login failed
    }
    size = strlen(username);
    if (username[size - 1] == '\n') {
        username[--size] = '\0'; // Remove newline
    }

    // Prompt for password (hides input)
    printf(">> LDAP Password: ");
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // Get current terminal attributes
    newt = oldt;
    newt.c_lflag &= ~(ECHO);        // Disable echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt); // Apply changes
    if (fgets(password, sizeof(password), stdin) == NULL) {
        perror("Error reading password");
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore terminal attributes
        return NULL; // Login failed
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // Restore terminal attributes
    printf("\n");

    size = strlen(password);
    if (password[size - 1] == '\n') {
        password[--size] = '\0'; // Remove newline
    }

    // Send LOGIN command to the server
    snprintf(buffer, sizeof(buffer), "LOGIN\n%s\n%s\n", username, password);
    sendMessage(create_socket, buffer);

    // Wait for server response
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size > 0) {
        buffer[size] = '\0'; // Null-terminate server response
        if (strncmp(buffer, "OK", 2) == 0) {
            printf("<< Login successful!\n");
            return username; // Login successful
        } else if (strncmp(buffer, "ERR", 3) == 0) {
            printf("<< Login failed: %s\n", buffer + 4); // Print error details if provided
            return NULL; // Login failed
        } else {
            printf("<< Unexpected server response: %s\n", buffer);
            return NULL; // Login failed
        }
    } else {
        perror("recv error");
        return NULL; // Login failed
    }
}

void handleSendCommand(int create_socket, char *username) {
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
                    // Send the first line of the message
                    sendMessage(create_socket, buffer);

                    while (1) {
                        printf(">> ");
                        memset(buffer, 0, sizeof(buffer)); // Clear buffer
                        if (fgets(buffer, BUF - 1, stdin) != NULL) {
                            size = strlen(buffer);
                            if (buffer[size - 1] == '\n') {
                                buffer[--size] = '\0';
                            }
                            sendMessage(create_socket, buffer); // Send every line, including empty ones
                            if (strcmp(buffer, ".") == 0) { // End of message
                                break;
                            }
                        }
                    }
                    break;
                } else {
                    sendMessage(create_socket, buffer);
                    break;
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

void handleListCommand(int create_socket, char *username) {
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

        // Check if response indicates no messages
        char *line = strtok(buffer, "\n");
        if (line && isdigit(line[0])) { // Check if first line is the count of messages
            int count = atoi(line);
            printf("<< Messages received: %d\n", count);

            if (count == 0) {
                line = strtok(NULL, "\n"); // Get the next line
                if (line) {
                    printf("<< %s\n\n", line); // Print "This user has not received any messages yet!"
                }
            } else {
                int counter = 1;
                while ((line = strtok(NULL, "\n")) != NULL) {
                    printf("%d. %s\n", counter, line); // Print subjects of messages
                    counter++; 
                }
                printf("\n");
            }
        } else {
            printf("<< %s\n", buffer); // Print error or unexpected response
        }
    } else {
        perror("recv error");
    }
}

void handleReadCommand(int create_socket, char *username) {
    char buffer[BUF];
    int size;

    // Send "READ" command to the server
    sendMessage(create_socket, "READ");

    // Prompt for username
    while (1) {
        printf(">> Username (max 8 chars): ");
        if (fgets(buffer, BUF - 1, stdin) != NULL) {
            size_t len = strlen(buffer);
            if (buffer[len - 1] == '\n') buffer[--len] = '\0';

            if (!isValidInput(buffer, 8)) {
                printf("Invalid username! Try again.\n");
                continue;
            }

            sendMessage(create_socket, buffer);
            break;
        }
    }

    // Prompt for message number
    while (1) {
        printf(">> Message Number: ");
        if (fgets(buffer, BUF - 1, stdin) != NULL) {
            size_t len = strlen(buffer);
            if (buffer[len - 1] == '\n') buffer[--len] = '\0';

            if (sscanf(buffer, "%d", &size) != 1 || size <= 0) {
                printf("Invalid message number! Try again.\n");
                continue;
            }

            sendMessage(create_socket, buffer);
            break;
        }
    }

    // Receive the complete response
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size > 0) {
        buffer[size] = '\0';
        if (strncmp(buffer, "ERR", 3) == 0) {
            char *error_msg = strchr(buffer, '\n'); // Locate the additional error message
            printf("<< ERR\n");
            if (error_msg) {
                printf("<< %s\n", error_msg + 1); // Print additional error message
            }
        } else if (strncmp(buffer, "OK", 2) == 0) {
            printf("<< OK\n%s\n", buffer + 3);
        } else {
            printf("%s\n", buffer);
        }
    } else if (size == 0) {
        printf("Server closed remote socket\n");
    } else {
        perror("recv error");
    }
}

void handleDelCommand(int create_socket, char *username) {
    char buffer[BUF];
    int size;

    sendMessage(create_socket, "DEL");

    // Ask for username and validate it
    while (1) {
        printf(">> Username (max 8 chars): ");
        if (fgets(buffer, BUF - 1, stdin) != NULL) {
            size_t len = strlen(buffer);
            if (buffer[len - 1] == '\n') buffer[--len] = '\0'; // remove newline

            if (!isValidInput(buffer, 8)) {
                printf("Invalid username! Try again.\n");
                continue;
            }

            sendMessage(create_socket, buffer); // Send the username to the server
            break;
        }
    }

    // Ask and validate message number
    while (1) {
        printf(">> Message number: ");
        if (fgets(buffer, BUF - 1, stdin) != NULL) {
            size_t len = strlen(buffer);
            if (buffer[len - 1] == '\n') buffer[--len] = '\0';

            if (sscanf(buffer, "%d", &size) != 1 || size <= 0) {
                printf("Invalid message number! Try again.\n");
                continue;
            }

            sendMessage(create_socket, buffer); // Send the message number to the server
            break;
        }
    }

    // Get the response from the server
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size > 0) {
        buffer[size] = '\0'; // Terminate string

        // Error and success messages
        if (strncmp(buffer, "ERR", 3) == 0) {
            char *error_msg = strchr(buffer, '\n'); 
            printf("<< ERR\n");
            if (error_msg) {
                printf("<< %s\n", error_msg + 1); 
            }
        } else if (strncmp(buffer, "OK", 2) == 0) {
            char *success_msg = strchr(buffer, '\n'); 
            printf("<< OK\n");
            if (success_msg) {
                printf("<< %s\n", success_msg + 1); 
            }
        } else {
            printf("<< %s\n", buffer); 
        }
    } else if (size == 0) {
        printf("Server closed remote socket\n");
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
    char *username = NULL;

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

            if(username == NULL && (strcmp(buffer, "SEND") == 0 || strcmp(buffer, "LIST") == 0 || strcmp(buffer, "READ") == 0 || strcmp(buffer, "DEL") == 0)){
                printf("You need to login first! Use the command LOGIN!");
                continue;
            }

            if(strcmp(buffer, "SEND") == 0 || strcmp(buffer, "LIST") == 0 || strcmp(buffer, "READ") == 0 || strcmp(buffer, "DEL") == 0 || strcmp(buffer, "LOGIN") == 0) {
                // Handle "LOGIN" command
                if (strcmp(buffer, "SEND") == 0) {
                    username = handleLoginCommand(create_socket);
                }
                // Handle "SEND" command
                if (strcmp(buffer, "SEND") == 0) {
                    handleSendCommand(create_socket, username);

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
                } else if (strcmp(buffer, "LIST") == 0) {
                    handleListCommand(create_socket, username);
                } else if (strcmp(buffer, "READ") == 0) {
                    handleReadCommand(create_socket, username);
                } else if (strcmp(buffer, "DEL") == 0) {
                    handleDelCommand(create_socket, username);
                }

                memset(buffer, 0, sizeof(buffer)); // Clear buffer                
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