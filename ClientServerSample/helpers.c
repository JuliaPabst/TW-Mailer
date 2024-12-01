#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>      
#include <sys/socket.h>
#include "helpers.h"
#include <dirent.h>

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
    if (readline(client_socket, receiver, sizeof(receiver)) <= 0  || !isValidUsername(receiver)){
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
        if (readline(client_socket, buffer, sizeof(buffer)) <= 0) {
            printf("DEBUG: Error or disconnection while reading message lines.\n");
            send(client_socket, "Missing content\n", 4, 0);
            return;
        }
        printf("DEBUG: Message line received: %s\n", buffer); // Debug message line
        if (strcmp(buffer, ".") == 0) {
            printf("DEBUG: End of message detected.\n");
            break; // End of message
        }

        if (strlen(message) + strlen(buffer) + 2 >= BUF) { // +2 for newline and null terminator
            printf("DEBUG: Message buffer overflow detected.\n");
            send(client_socket, "ERR\n", 4, 0);
            return;
        }

        strcat(message, buffer);
        strcat(message, "\n");
    }

    printf("DEBUG: Complete message:\n%s\n", message); // Debug full message

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
        send(client_socket, "This user has not received any messages yet!\n", 60, 0);
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


void *clientCommunication(void *data, const char *mail_spool_dir) {
    int client_socket = *(int *)data; // Client socket
    char buffer[BUF];
    ssize_t size;

    ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
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
        printf("Received from client: %s\n", buffer); // Print received data
        
        // Check command type
        if (strncmp(buffer, "SEND", 4) == 0) {
            // Process the SEND command
            printf("Receive Send command\r\n");
            handleSendCommand(client_socket, mail_spool_dir);
        } else if (strcmp(buffer, "QUIT") == 0) {
            // Process the QUIT command (client disconnect)
            break;
        } else if (strncmp(buffer, "LIST", 4) == 0) {
        // Process the LIST command
        printf("Receive LIST command\r\n");
        handleListCommand(client_socket, mail_spool_dir);
        } else {
            // Unknown command
            send(client_socket, "Unknown command\n", 25, 0);
        }  

    }

    close(client_socket); // Close client socket
    return NULL;
}