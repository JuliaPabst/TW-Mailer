#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>      
#include <sys/socket.h>
#include "helpers.h"

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
    char sender[9], receiver[9], subject[81], message[BUF];
    char buffer[BUF];
    char inbox_path[256];
    FILE *inbox_file;

    // Read Sender
    if (readline(client_socket, sender, sizeof(sender)) <= 0 || !isValidUsername(sender)) {
        printf("DEBUG: Invalid or missing sender received.\n");
        send(client_socket, "ERR\n", 4, 0);
        return;
    }

    printf("DEBUG: Sender: %s\n", sender); // Debug sender

    // Read Receiver
    if (readline(client_socket, receiver, sizeof(receiver)) <= 0 || !isValidUsername(receiver)) {
        printf("DEBUG: Invalid or missing receiver received.\n");
        send(client_socket, "ERR\n", 4, 0);
        return;
    }
    printf("DEBUG: Receiver: %s\n", receiver); // Debug receiver

    // Read Subject
    if (readline(client_socket, subject, sizeof(subject)) <= 0 || strlen(subject) > 80) {
        printf("DEBUG: Invalid or missing subject received.\n");
        send(client_socket, "ERR\n", 4, 0);
        return;
    }
    printf("DEBUG: Subject: %s\n", subject); // Debug subject

    // Read Message
    message[0] = '\0'; // Clear the message buffer
    printf("DEBUG: Starting to read message lines.\n");

    while (1) {
        if (readline(client_socket, buffer, sizeof(buffer)) <= 0) {
            printf("DEBUG: Error or disconnection while reading message lines.\n");
            send(client_socket, "ERR\n", 4, 0);
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
        send(client_socket, "ERR\n", 4, 0);
        return;
    }
    fprintf(inbox_file, "From: %s\nTo: %s\nSubject: %s\n%s\n---\n", sender, receiver, subject, message);
    fclose(inbox_file);
    printf("DEBUG: Message written to inbox successfully.\n");

    // Respond with success
    send(client_socket, "OK\n", 3, 0);
    printf("DEBUG: Sent 'OK' response to client.\n");
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
        } else {
            // Unknown command
            send(client_socket, "ERR\n", 4, 0);
        }
    }

    close(client_socket); // Close client socket
    return NULL;
}