#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>      
#include <sys/socket.h>
#include "helpers.h"
#include <dirent.h>
#include <errno.h>

void signalHandler(int sig) {
    // Suppress unused parameter warning
    (void)sig;

    printf("Signal received: %d\n", sig);
    fflush(stdout);

    // Handle cleanup or graceful shutdown
    exit(0);
}

int readline(int socket, char *buffer, size_t size) {
    size_t i = 0;
    char c;

    while (i < size - 1) { // Reserve space for null terminator
        ssize_t bytes = recv(socket, &c, 1, 0);
        if (bytes == -1) {
            perror("recv error");
            return -1; // Return error
        } else if (bytes == 0) {
            // Connection closed by the client
            if (i == 0) {
                return -1; // No data read
            }
            break;
        }

        if (c == '\0') {
            break; // End of line
        }

        buffer[i++] = c; // Store character in buffer
    }

    buffer[i] = '\0'; // Null-terminate the string
    printf("DEBUG: Received line: '%s'\n", buffer); // Add debug statement
    return (int)i; // Return the number of characters read
}

int isValidUsername(const char *username) {
    if (strlen(username) > 8) return 0;
    for (size_t i = 0; i < strlen(username); ++i) {
        if (!isalnum(username[i])) return 0;
    }
    return 1;
}

void handleSendCommand(int client_socket, const char *mail_spool_dir) {
    char sender[81], receiver[81], subject[81], message[BUF];
    char buffer[BUF];
    char inbox_path[256];
    FILE *inbox_file;

    // Read Sender
    if (readline(client_socket, sender, sizeof(sender)) <= 0 || !isValidUsername(sender)) {
        printf("DEBUG: Invalid or missing sender received.\n");
        send(client_socket, "Sender username was invalid (should not be longer than 8 characters) or missing.\n", 4, 0);
        return;
    } else {
        send(client_socket, "Sender received\n", 4, 0);
    }

    printf("DEBUG: Sender: %s\n", sender); // Debug sender

    // Read Receiver
    if (readline(client_socket, receiver, sizeof(receiver)) <= 0 || !isValidUsername(receiver)) {
        printf("DEBUG: Invalid or missing receiver received.\n");
        send(client_socket, "Receiver username was invalid (should not be longer than 8 characters) or missing.\n", 4, 0);
        return;
    }
    printf("DEBUG: Receiver: %s\n", receiver); // Debug receiver

    // Read Subject
    if (readline(client_socket, subject, sizeof(subject)) <= 0 || strlen(subject) > 80) {
        printf("DEBUG: Invalid or missing subject received.\n");
        send(client_socket, "Receiver subject was invalid (should not be longer than 8 characters) or missing.\n", 4, 0);
        return;
    }
    printf("DEBUG: Subject: %s\n", subject); // Debug subject

    // Read Message
    message[0] = '\0'; // Clear the message buffer
    printf("DEBUG: Starting to read message lines.\n");

    while (1) {
        int bytes_read = readline(client_socket, buffer, sizeof(buffer));
        if (bytes_read < 0) { // Connection error or disconnection
            printf("DEBUG: Error or disconnection while reading message lines.\n");
            send(client_socket, "ERR\n", 4, 0);
            return;
        }
        printf("DEBUG: Message line received: '%s'\n", buffer); // Debug message line

        if (strcmp(buffer, ".") == 0) { // End of message detected
            printf("DEBUG: End of message detected.\n");
            break;
        }

        if (strlen(message) + strlen(buffer) + 2 >= BUF) { // +2 for newline and null terminator
            printf("DEBUG: Message buffer overflow detected.\n");
            send(client_socket, "ERR\n", 4, 0);
            return;
        }

        strcat(message, buffer);
        strcat(message, "\n"); // Append a newline to each line
    }

    printf("DEBUG: Complete message:\n%s", message); // Debug full message

    // Prepare receiver's inbox path
    snprintf(inbox_path, sizeof(inbox_path), "%s/%s_inbox.txt", mail_spool_dir, receiver);
    printf("DEBUG: Inbox path: %s\n", inbox_path); // Debug inbox path

    // Append the message to the receiver's inbox
    inbox_file = fopen(inbox_path, "a");
    if (!inbox_file) {
        perror("DEBUG: Error opening inbox file");
        send(client_socket, "Message not sent to inbox of receiver.\n", 4, 0);
        return;
    }
    fprintf(inbox_file, "From: %s\nTo: %s\nSubject: %s\n%s\n---\n", sender, receiver, subject, message);
    fclose(inbox_file);
    printf("DEBUG: Message written to inbox successfully.\n");

    // Respond with success
    send(client_socket, "OK\n", 3, 0);
    printf("DEBUG: Sent 'OK' response to client.\n");
}

void handleListCommand(int client_socket, const char *mail_spool_dir) {
    char username[81];
    char buffer[BUF];
    char user_inbox_path[256];
    FILE *inbox_file;
    int message_count = 0;

    // Read the username
    if (readline(client_socket, username, sizeof(username)) <= 0 || !isValidUsername(username)) {
        printf("DEBUG: Invalid or missing username received.\n");
        send(client_socket, "0\n", 2, 0); // Respond with "0" for no messages
        return;
    }

    printf("DEBUG: Username for LIST: %s\n", username);

    // Construct the path to the user's inbox file
    snprintf(user_inbox_path, sizeof(user_inbox_path), "%s/%s_inbox.txt", mail_spool_dir, username);
    inbox_file = fopen(user_inbox_path, "r");

    if (!inbox_file) {
        // User inbox not found or other error
        printf("DEBUG: Inbox file not found for user %s.\n", username);
        send(client_socket, "0\nThis user has not received any messages yet!\n\n", 60, 0);
        printf("DEBUG: Sending LIST response:\n0\nThis user has not received any messages yet!\n\n");
        return;
    }

    // Read subjects from the inbox file
    buffer[0] = '\0'; // Clear the buffer
    char line[BUF];
    size_t remaining_size = sizeof(buffer); // Track remaining space in buffer

    while (fgets(line, sizeof(line), inbox_file)) {
        // If the line starts with "Subject: ", extract the subject
        if (strncmp(line, "Subject: ", 9) == 0) {
            message_count++;
            size_t subject_length = strlen(line + 9); // Length of the subject
            if (subject_length + 1 > remaining_size) { // +1 for null terminator
                printf("DEBUG: Buffer overflow prevented while reading subjects.\n");
                break;
            }
            strncat(buffer, line + 9, remaining_size - 1);
            remaining_size -= subject_length;
        }
    }
    fclose(inbox_file);

    // Prepare response
    char response[BUF];
    snprintf(response, sizeof(response), "%d\n", message_count);
    size_t response_length = strlen(response);

    if (response_length + strlen(buffer) < sizeof(response)) {
        strncat(response, buffer, sizeof(response) - response_length - 1);
    } else {
        printf("DEBUG: Response buffer too small to include all subjects.\n");
    }

    // Send response to the client
    printf("DEBUG: Sending LIST response:\n%s", response);
    send(client_socket, response, strlen(response), 0);
}

void handleReadCommand(int client_socket, const char *mail_spool_dir) {
    char username[81], message_number_str[10];
    char user_inbox_path[256];
    char buffer[BUF];
    FILE *inbox_file;
    int message_number;

    // 1. Read Username
    if (readline(client_socket, username, sizeof(username)) <= 0 || !isValidUsername(username)) {
        printf("DEBUG: Invalid or missing username received.\n");
        send(client_socket, "ERR\nInvalid or missing username.\n", 40, 0);
        return;
    }
    printf("DEBUG: Username for READ: %s\n", username);

    // 2. Read Message Number
    if (readline(client_socket, message_number_str, sizeof(message_number_str)) <= 0 ||
        sscanf(message_number_str, "%d", &message_number) != 1 || message_number <= 0) {
        printf("DEBUG: Invalid or missing message number received.\n");
        send(client_socket, "ERR\nInvalid message number.\n", 30, 0);
        return;
    }
    printf("DEBUG: Message number for READ: %d\n", message_number);

    // 3. Construct Inbox Path
    snprintf(user_inbox_path, sizeof(user_inbox_path), "%s/%s_inbox.txt", mail_spool_dir, username);
    inbox_file = fopen(user_inbox_path, "r");

    if (!inbox_file) {
        printf("DEBUG: Inbox file not found for user %s.\n", username);
        char error_msg[BUF];
        snprintf(error_msg, sizeof(error_msg), "ERR\nInbox file not found for user %s.\n", username);
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // 4. Locate and Read the Specific Message
    int current_message = 1;
    int found = 0;
    buffer[0] = '\0';
    char line[BUF];
    int is_first_message = 1;

    while (fgets(line, sizeof(line), inbox_file)) {
        if (is_first_message) {
            // First message special handling
            if (current_message == message_number) {
                found = 1;
                break;
            }
            is_first_message = 0;
        } else if (strncmp(line, "---", 3) == 0) {
            // Message boundary detected
            current_message++;
        }
        if (current_message == message_number) {
            found = 1;
            break;
        }
    }

    if (!found || feof(inbox_file)) {
        printf("DEBUG: Message number %d not found for user %s.\n", message_number, username);
        char error_msg[BUF];
        snprintf(error_msg, sizeof(error_msg), "ERR\nMessage number %d is empty for user %s.\n",
                 message_number, username);
        send(client_socket, error_msg, strlen(error_msg), 0);
        fclose(inbox_file);
        return;
    }

    // 5. Extract the Message
    buffer[0] = '\0';
    while (fgets(line, sizeof(line), inbox_file)) {
        if (strncmp(line, "---", 3) == 0) {
            break;
        }
        strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
    }
    fclose(inbox_file);

    if (strlen(buffer) == 0) {
        printf("DEBUG: Message number %d is empty for user %s.\n", message_number, username);
        char error_msg[BUF];
        snprintf(error_msg, sizeof(error_msg), "ERR\nMessage number %d is empty for user %s.\n",
                 message_number, username);
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // 6. Combine "OK" and Message in One Send
    char response[BUF];
    size_t response_length = snprintf(response, sizeof(response), "OK\n");
    size_t buffer_length = strlen(buffer);

    if (response_length + buffer_length >= sizeof(response)) {
        // Truncate the message to fit into the response buffer
        strncat(response, buffer, sizeof(response) - response_length - 1);
    } else {
        strcat(response, buffer);
    }

    printf("DEBUG: Sent 'OK' response to client.\n");
    printf("DEBUG: Sending READ response:\n%s", response);
    send(client_socket, response, strlen(response), 0);
}

void handleDelCommand(int client_socket, const char *mail_spool_dir) {
    char username[81], message_number_str[10];
    char user_inbox_path[256];
    char buffer[BUF];
    FILE *inbox_file, *temp_file;
    int message_number;

    // 1. Read Username
    if (readline(client_socket, username, sizeof(username)) <= 0 || !isValidUsername(username)) {
        printf("DEBUG: Invalid or missing username received.\n");
        send(client_socket, "ERR\nInvalid or missing username.\n", 40, 0);
        return;
    }
    printf("DEBUG: Username for DEL: %s\n", username);

    // 2. Read Message Number
    if (readline(client_socket, message_number_str, sizeof(message_number_str)) <= 0 ||
        sscanf(message_number_str, "%d", &message_number) != 1 || message_number <= 0) {
        printf("DEBUG: Invalid or missing message number received.\n");
        send(client_socket, "ERR\nInvalid message number.\n", 30, 0);
        return;
    }
    printf("DEBUG: Message number for DEL: %d\n", message_number);

    // 3. Construct Inbox Path
    snprintf(user_inbox_path, sizeof(user_inbox_path), "%s/%s_inbox.txt", mail_spool_dir, username);

    inbox_file = fopen(user_inbox_path, "r");
    if (!inbox_file) {
        printf("DEBUG: Inbox file not found for user %s.\n", username);
        char error_msg[BUF];
        snprintf(error_msg, sizeof(error_msg), "ERR\nInbox file not found for user %s.\n", username);
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // 4. Create Temporary File
    snprintf(buffer, sizeof(buffer), "%s/temp_inbox.txt", mail_spool_dir);
    temp_file = fopen(buffer, "w");
    if (!temp_file) {
        perror("DEBUG: Failed to create temp file");
        fclose(inbox_file);
        send(client_socket, "ERR\nFailed to create temporary file.\n", 40, 0);
        return;
    }

    // 5. Locate and Remove the Specific Message
    int current_message = 1;
    int found = 0;
    char line[BUF];

    while (fgets(line, sizeof(line), inbox_file)) {
        if (strncmp(line, "---", 3) == 0) { // Message boundary
            current_message++;
        }

        if (current_message == message_number) {
            found = 1;
            continue; // Skip lines of the target message
        }

        fputs(line, temp_file); // Write lines of other messages to temp file
    }

    fclose(inbox_file);
    fclose(temp_file);

    if (!found) {
        printf("DEBUG: Message number %d not found for user %s.\n", message_number, username);
        remove(buffer); // Delete temp file
        char error_msg[BUF];
        snprintf(error_msg, sizeof(error_msg), "ERR\nMessage number %d not found for user %s.\n",
                 message_number, username);
        send(client_socket, error_msg, strlen(error_msg), 0);
        return;
    }

    // 6. Replace Original File with Temp File
    if (rename(buffer, user_inbox_path) != 0) {
        perror("DEBUG: Failed to replace inbox file");
        send(client_socket, "ERR\nFailed to update inbox file.\n", 35, 0);
        return;
    }

    printf("DEBUG: Message number %d deleted successfully for user %s.\n", message_number, username);
    char success_msg[BUF];
    snprintf(success_msg, sizeof(success_msg), "OK\nMessage number %d deleted successfully for user %s.\n",
             message_number, username);
    send(client_socket, success_msg, strlen(success_msg), 0);
}

void *clientCommunication(void *data, const char *mail_spool_dir) {
    int client_socket = *(int *)data; // Client socket
    char buffer[BUF];
    ssize_t size;

    ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n\n");
   if (send(client_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

    while (1) {
        // Receive data from the client
        size = recv(client_socket, buffer, BUF - 1, 0);
        if (size <= 0) {
            if (size == 0) {
                printf("Client disconnected\n");
            } else {
                perror("recv error");
            }
            break; // Exit on error or client disconnect
        }
        buffer[size] = '\0'; // Null-terminate the string
        printf("\nReceived from client: %s\n", buffer); // Print received data
        
        // Check command type (exact matching with strcmp)
        if (strcmp(buffer, "SEND") == 0) {
            // Process the SEND command
            printf("Receive SEND command\r\n");
            handleSendCommand(client_socket, mail_spool_dir);
        } else if (strcmp(buffer, "QUIT") == 0) {
            // Process the QUIT command (client disconnect)
            break;
        } else if (strcmp(buffer, "LIST") == 0) {
            // Process the LIST command
            printf("Receive LIST command\r\n");
            handleListCommand(client_socket, mail_spool_dir);
        } else if (strcmp(buffer, "READ") == 0) {
            // Process the READ command
            printf("Receive READ command\r\n");
            handleReadCommand(client_socket, mail_spool_dir);
        } else if (strcmp(buffer, "DEL") == 0) {
            // Process the DEL command
            printf("Receive DEL command\n");
            handleDelCommand(client_socket, mail_spool_dir);
        } else {
            // Unknown command
            printf("Unknown command received: %s\n", buffer);
            send(client_socket, "Unknown command\n", 16, 0);
        }
    }

    close(client_socket); // Close client socket
    return NULL;
}