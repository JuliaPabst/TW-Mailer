#include "helpers.h"
#include <dirent.h>
#include <errno.h>
#include "ldap_functions.h"
#include "session_manager.h"
#include <arpa/inet.h>
#include <time.h>


typedef struct {
    char ip[INET_ADDRSTRLEN];
    int attempts;
} LoginAttempt;

LoginAttempt loginAttempts[MAX_ATTEMPTS] = {0};

void getCurrentTimeString(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%a %d.%m.%Y %H:%M:%S", tm_info);
}

int readline(int socket, char *buffer, size_t size) {
    size_t i = 0;
    char c;

    while(i < size - 1) { // Reserve space for null terminator
        ssize_t bytes = recv(socket, &c, 1, 0);
        if(bytes == -1) {
            perror("recv error");
            return -1; // Return error
        } else if(bytes == 0) {
            // Connection closed by the client
            if(i == 0) {
                return -1; // No data read
            }
            break;
        }

        if(c == '\0' || c == '\n') {
            break; // End of line
        }

        buffer[i++] = c; // Store character in buffer
    }

    buffer[i] = '\0'; // Null-terminate the string
    printf("DEBUG: Received line: '%s'\n", buffer); // Add debug statement
    return(int)i; // Return the number of characters read
}

int isValidUsername(const char *username) {
    if(strlen(username) > 8) return 0;
    for(size_t i = 0; i < strlen(username); ++i) {
        if(!isalnum(username[i])) return 0;
    }
    return 1;
}

int isBlackListed(const char *ip, time_t *remaining_time) {
    FILE *file = fopen(BLACKLIST_FILE, "r");
    if(!file) {
        perror("Failed to open blacklist file");
        return 0;
    }

    char line[256];
    time_t current_time = time(NULL);
    int blacklisted = 0;
    time_t latest_blacklist_time = 0; // Track the latest blacklist time

    printf("DEBUG: Checking blacklist for IP: %s\n", ip);

    while(fgets(line, sizeof(line), file)) {
        char blacklisted_ip[INET_ADDRSTRLEN];
        time_t blacklist_time;

        // Correctly parse the line
        if(sscanf(line, "%*s %*s %*s - blocked IP: %15s %ld", blacklisted_ip, &blacklist_time) == 2) {

            if(strcmp(ip, blacklisted_ip) == 0) {
                // Update the latest blacklist time
                if(blacklist_time > latest_blacklist_time) {
                    latest_blacklist_time = blacklist_time;
                }
            }
        } else {
            printf("DEBUG: Failed to parse line: %s\n", line);
        }
    }

    fclose(file);

    if(latest_blacklist_time > 0) {
        double time_diff = difftime(current_time, latest_blacklist_time);
        if(time_diff < BLACKLIST_DURATION) {
            *remaining_time = BLACKLIST_DURATION - time_diff;
            blacklisted = 1;
            printf("DEBUG: IP %s is blacklisted with remaining time: %.0f seconds\n", ip, (double)*remaining_time);
        } else {
            printf("DEBUG: Blacklist duration expired for IP: %s\n", ip);
        }
    } else {
        printf("DEBUG: No matching entry found for IP: %s\n", ip);
    }

    return blacklisted;
}


void addToBlackList(const char *ip) {
    FILE *file = fopen(BLACKLIST_FILE, "a");
    if(!file) {
        perror("Failed to open blacklist file");
        return;
    }

    char time_str[64];
    getCurrentTimeString(time_str, sizeof(time_str));

    fprintf(file, "%s - blocked IP: %s %ld\n", time_str, ip, time(NULL));
    fclose(file);
}

void resetLoginAttempts(const char *ip) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(strcmp(loginAttempts[i].ip, ip) == 0) {
            loginAttempts[i].attempts = 0;
            break;
        }
    }
}


void recordFailedAttempt(const char *ip) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(strcmp(loginAttempts[i].ip, ip) == 0) {
            loginAttempts[i].attempts++;
            return;
        }

        if(loginAttempts[i].ip[0] == '\0') {
            strncpy(loginAttempts[i].ip, ip, INET_ADDRSTRLEN);
            loginAttempts[i].attempts = 1;
            return;
        }
    }
}

int getFailedAttempts(const char *ip) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(strcmp(loginAttempts[i].ip, ip) == 0) {
            return loginAttempts[i].attempts;
        }
    }

    return 0;
}

void handleLdapLogin(int client_socket) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(client_socket, (struct sockaddr *)&addr, &addr_len);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, client_ip, sizeof(client_ip));

    printf("DEBUG: Received LOGIN command from IP: %s\n", client_ip);

    char buffer[256];
    time_t remaining_time = 0;
    if(isBlackListed(client_ip, &remaining_time)) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                 "ERR\nYour IP is blocked for %.0f seconds.\n", (double)remaining_time);
        printf("DEBUG: IP %s is blacklisted for %.0f seconds.\n", client_ip, (double)remaining_time);
        send(client_socket, error_msg, strlen(error_msg), 0);
        recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        return;
    }

    char username[256];
    recv(client_socket, username, sizeof(username) - 1, 0);
    username[strcspn(username, "\n")] = 0;
    printf("DEBUG: Received Username: %s\n", username);

    char password[256];
    recv(client_socket, password, sizeof(password) - 1, 0);
    password[strcspn(password, "\n")] = 0;
    printf("DEBUG: Received Password\n");

    char *retrievedUsername = ldapFind(username, password);
    if(!retrievedUsername || strcmp(retrievedUsername, "FAILED") == 0) {
        recordFailedAttempt(client_ip);
        int attempts_left = MAX_ATTEMPTS - getFailedAttempts(client_ip);

        if(attempts_left <= 0) {
            addToBlackList(client_ip);
            printf("DEBUG: 3 Failed attempts! IP %s is blacklisted\n", client_ip);
            send(client_socket, "ERR\nToo many failed attempts. Try again in 1 minute.\n", 55, 0);
            resetLoginAttempts(client_ip);
        } else {
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg),
                     "ERR\nInvalid credentials.\nYou have %d more attempt%s left.\n",
                     attempts_left, (attempts_left == 1) ? "" : "s");
            printf("DEBUG: Invalid credentials. %d attempts left for IP %s\n", attempts_left, client_ip);
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
        return;
    }

    resetLoginAttempts(client_ip);
    addSession(client_socket, retrievedUsername);
    printf("Retrieved Username: %s\n", retrievedUsername);
    send(client_socket, "OK\nLogin successful.\n", 22, 0);

    if(retrievedUsername != NULL) {
        free(retrievedUsername);
    }
}

void handleSendCommand(int client_socket, const char *mail_spool_dir) {
    const char *sender = getSessionUsername(client_socket);
    char receiver[81], subject[81], message[BUF * 4];  // larger buffer for message
    char buffer[BUF];
    char inbox_path[512];
    char user_dir[256];
    FILE *inbox_file;

    // Read Receiver
    if(readline(client_socket, receiver, sizeof(receiver)) <= 0 || !isValidUsername(receiver)) {
        send(client_socket, "ERR Invalid receiver\n", 22, 0);
        return;
    }

    // Read Subject
    if(readline(client_socket, subject, sizeof(subject)) <= 0 || strlen(subject) > 80) {
        send(client_socket, "ERR Invalid subject\n", 21, 0);
        return;
    }

    // Read Message (including empty lines)
    message[0] = '\0';
    while(1){
        int bytes_read = readline(client_socket, buffer, sizeof(buffer));
        if(bytes_read < 0) {
            send(client_socket, "ERR Reading message failed\n", 28, 0);
            return;
        }

        // End of message
        if(strcmp(buffer, ".") == 0) {
            break;
        }

        // Correctly handle empty lines
        strcat(message, buffer);
        strcat(message, "\n");
    }

    // Create user directory if not exists
    snprintf(user_dir, sizeof(user_dir), "%s/%s", mail_spool_dir, receiver);
    if(mkdir(user_dir, 0755) == -1 && errno != EEXIST) {
        perror("Error creating user directory");
        send(client_socket, "ERR Could not create user directory\n", 37, 0);
        return;
    }

    // Save the message
    snprintf(inbox_path, sizeof(inbox_path), "%s/inbox.txt", user_dir);
    inbox_file = fopen(inbox_path, "a");
    if(!inbox_file) {
        perror("Error opening inbox file");
        send(client_socket, "ERR Could not open inbox\n", 25, 0);
        return;
    }
    fprintf(inbox_file, "From: %s\nTo: %s\nSubject: %s\n%s\n", sender, receiver, subject, message);

    // Process attachments
    if(readline(client_socket, buffer, sizeof(buffer)) > 0 && strcmp(buffer, "ATTACHMENT_START") == 0) {
        // Create attachment directory if not exists
        char attachment_dir[512];
        snprintf(attachment_dir, sizeof(attachment_dir), "%s/attachments", user_dir);
        if(mkdir(attachment_dir, 0755) == -1 && errno != EEXIST) {
            send(client_socket, "ERR Unable to create attachments directory\n", 45, 0);
            return;
        }

        // Read and save attachments
        while(1) {
            char attachment_name[256];
            if(readline(client_socket, attachment_name, sizeof(attachment_name)) <= 0) {
                fclose(inbox_file);
                send(client_socket, "ERR Missing attachment name\n", 29, 0);
                return;
            }

            char attachment_path[1024];
            snprintf(attachment_path, sizeof(attachment_path), "%s/%s", attachment_dir, attachment_name);
            FILE *attachment_file = fopen(attachment_path, "wb");
            if(!attachment_file) {
                perror("Error opening attachment file");
                fclose(inbox_file);
                send(client_socket, "ERR Could not save attachment\n", 31, 0);
                return;
            }

            // receive data for attachments
            while(1) {
                ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
                if(bytes_received < 0) {
                    perror("Error receiving file data");
                    fclose(attachment_file);
                    fclose(inbox_file);
                    return;
                } else if (bytes_received == 0) {
                    printf("DEBUG: Client disconnected unexpectedly\n");
                    fclose(attachment_file);
                    break;
                }

                // check if "ATTACHMENT_END" is given
                size_t bytes_to_write = bytes_received;
                for(size_t i = 0; i < (size_t)(bytes_received - 14); ++i) {
                    if(memcmp(buffer + i, "ATTACHMENT_END", 14) == 0) {
                        bytes_to_write = i;
                        break;
                    }
                }

                // write in received data
                if(bytes_to_write > 0) {
                    fwrite(buffer, 1, bytes_to_write, attachment_file);
                }

                // if marker was found, break loop
                if((size_t)bytes_to_write < (size_t)bytes_received) {
                    printf("DEBUG: Attachment end marker found\n");
                    break;
                }
            }
            fclose(attachment_file);
            fprintf(inbox_file, "Attachment: %s\n", attachment_name);
            printf("DEBUG: Message and attachments saved successfully.\n");
            break;
        }
    }
    fprintf(inbox_file, "---\n");
    fclose(inbox_file);
    send(client_socket, "OK\n", 3, 0); // Always send a final response to the client
}

void handleListCommand(int client_socket, const char *mail_spool_dir) {
    const char *username = getSessionUsername(client_socket);
    char response[BUF * 2];
    char user_inbox_path[256];
    char attachment_path[256];
    FILE *inbox_file;
    int message_count = 0;
    size_t response_length = 0;

    printf("DEBUG: Username for LIST: %s\n", username);

    // Construct the path to the user's inbox file
    snprintf(user_inbox_path, sizeof(user_inbox_path), "%s/%s/inbox.txt", mail_spool_dir, username);
    snprintf(attachment_path, sizeof(attachment_path), "%s/%s/attachments", mail_spool_dir, username);
    
    inbox_file = fopen(user_inbox_path, "r");
    if(!inbox_file) {
        // User inbox not found or other error
        printf("DEBUG: Inbox file not found for user %s.\n", username);
        send(client_socket, "0\nThis user has not received any messages yet!\n\n", 60, 0);
        printf("DEBUG: Sending LIST response:\n0\nThis user has not received any messages yet!\n\n");
        return;
    }

    // parse messages to display correctly
    char line[BUF];
    response_length += snprintf(response + response_length, sizeof(response) - response_length, "List of Messages\n");

    while(fgets(line, sizeof(line), inbox_file)) {
        if(strncmp(line, "From: ", 6) == 0) {
            response_length += snprintf(response + response_length, sizeof(response) - response_length, "\nMessage %d:\n%s", ++message_count, line);
        } else if(strncmp(line, "To: ", 4) == 0 || strncmp(line, "Subject: ", 9) == 0) {
            response_length += snprintf(response + response_length, sizeof(response) - response_length, "%s", line);
        } else if(strncmp(line, "Attachment: ", 12) == 0) {
            response_length += snprintf(response + response_length, sizeof(response) - response_length, "  %s\n", line);
            printf("\n");
        }
    }
    fclose(inbox_file);

    if(message_count == 0) {
        response_length += snprintf(response + response_length, sizeof(response) - response_length, "No messages found.\n");
    }

    // list attachments
    DIR *dir = opendir(attachment_path);
    if(dir) {
        struct dirent *entry;
        response_length += snprintf(response + response_length, sizeof(response) - response_length, "\nAttachment summary:\n");

        while((entry = readdir(dir))) {
            if(entry->d_name[0] != '.') { // ignore '.' and '..'
                response_length += snprintf(response + response_length, sizeof(response) - response_length, "- %s\n", entry->d_name);
            }
        }
        closedir(dir);
    } else {
        response_length += snprintf(response + response_length, sizeof(response) - response_length, "\nNo attachments found.\n");
    }

    // always send final response to server
    send(client_socket, response, strlen(response), 0);
}

void handleReadCommand(int client_socket, const char *mail_spool_dir) {
    const char *username = getSessionUsername(client_socket);
    char message_number_str[10];
    char user_inbox_path[256];
    char buffer[BUF];
    FILE *inbox_file;
    int message_number;

    printf("DEBUG: Username for READ: %s\n", username);

    // 2. Read Message Number
    if(readline(client_socket, message_number_str, sizeof(message_number_str)) <= 0 ||
        sscanf(message_number_str, "%d", &message_number) != 1 || message_number <= 0) {
        printf("DEBUG: Invalid or missing message number received.\n");
        send(client_socket, "ERR\nInvalid message number.\n", 30, 0);
        return;
    }
    printf("DEBUG: Message number for READ: %d\n", message_number);

    // 3. Construct Inbox Path
    snprintf(user_inbox_path, sizeof(user_inbox_path), "%s/%s/inbox.txt", mail_spool_dir, username);
    inbox_file = fopen(user_inbox_path, "r");

    if(!inbox_file) {
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

    while(fgets(line, sizeof(line), inbox_file)) {
        if (is_first_message) {
            // First message special handling
            if(current_message == message_number) {
                found = 1;
                break;
            }
            is_first_message = 0;
        } else if(strncmp(line, "---", 3) == 0) {
            // Message boundary detected
            current_message++;
        }
        if(current_message == message_number) {
            found = 1;
            break;
        }
    }

    if(!found || feof(inbox_file)) {
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
    while(fgets(line, sizeof(line), inbox_file)) {
        if(strncmp(line, "---", 3) == 0) {
            break;
        }
        strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
    }
    fclose(inbox_file);

    if(strlen(buffer) == 0) {
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

    if(response_length + buffer_length >= sizeof(response)) {
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
    const char *username = getSessionUsername(client_socket);
    char message_number_str[10];
    char user_inbox_path[256];
    char buffer[BUF];
    FILE *inbox_file, *temp_file;
    int message_number;

    printf("DEBUG: Username for DEL: %s\n", username);

    // 1. Read Message Number
    memset(message_number_str, 0, sizeof(message_number_str));
    if(readline(client_socket, message_number_str, sizeof(message_number_str)) <= 0 ||
        sscanf(message_number_str, "%d", &message_number) != 1 || message_number <= 0) {
        printf("DEBUG: Invalid or missing message number received.\n");
        send(client_socket, "ERR\nInvalid message number.\n", 30, 0);
        return;
    }
    printf("DEBUG: Message number for DEL: %d\n", message_number);

    // 2. Construct Inbox Path
    snprintf(user_inbox_path, sizeof(user_inbox_path), "%s/%s/inbox.txt", mail_spool_dir, username);
    inbox_file = fopen(user_inbox_path, "r");
    if(!inbox_file) {
        printf("DEBUG: Inbox file not found for user %s.\n", username);
        send(client_socket, "ERR\nInbox file not found.\n", 28, 0);
        return;
    }

    // 3. Count total messages
    int total_messages = 0;
    char line[BUF];
    while(fgets(line, sizeof(line), inbox_file)) {
        if(strncmp(line, "---", 3) == 0) {
            total_messages++;
        }
    }
    rewind(inbox_file);  // Set file pointer back to the beginning

    // 4. Validate message number
    if(message_number > total_messages) {
        printf("DEBUG: Message number %d does not exist. Total messages: %d\n", message_number, total_messages);
        send(client_socket, "ERR\nMessage number does not exist.\n", 36, 0);
        fclose(inbox_file);
        return;
    }

    // 5. Create Temporary File
    snprintf(buffer, sizeof(buffer), "%s/temp_inbox.txt", mail_spool_dir);
    temp_file = fopen(buffer, "w");
    if(!temp_file) {
        perror("DEBUG: Failed to create temp file");
        fclose(inbox_file);
        send(client_socket, "ERR\nFailed to create temporary file.\n", 38, 0);
        return;
    }

    // 6. Locate and Remove the Specific Message
    int current_message = 1;
    int found = 0;
    int deleting = 0;

    while(fgets(line, sizeof(line), inbox_file)) {
        if(strncmp(line, "From: ", 6) == 0) {
            if(current_message == message_number) {
                found = 1;
                deleting = 1;  // Start deleting this message
                continue;      // Skip "From:" line
            }
        }

        if(strncmp(line, "---", 3) == 0) {
            if(deleting) {
                deleting = 0;  // Stop deleting after the separator
                current_message++;
                continue;      // Skip separator
            } else {
                current_message++;
            }
        }

        if(!deleting) {
            fputs(line, temp_file);  // Write only non-deleted messages
        }
    }

    fclose(inbox_file);
    fclose(temp_file);

    // 7. Handle case if message was not found
    if(!found) {
        printf("DEBUG: Message number %d not found for user %s.\n", message_number, username);
        remove(buffer);
        send(client_socket, "ERR\nMessage number not found.\n", 32, 0);
        return;
    }

    // 8. Replace Original File with Temp File
    if(rename(buffer, user_inbox_path) != 0) {
        perror("DEBUG: Failed to replace inbox file");
        send(client_socket, "ERR\nFailed to update inbox file.\n", 34, 0);
        return;
    }

    printf("DEBUG: Message number %d deleted successfully for user %s.\n", message_number, username);
    send(client_socket, "OK\nMessage deleted successfully.\n", 33, 0);
}


void *clientCommunication(void *data, const char *mail_spool_dir) {
    int client_socket = *(int *)data; // Client socket
    char buffer[BUF];
    ssize_t size;

    ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n\n");
   if(send(client_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

    while(1) {
        // Receive data from the client
        size = recv(client_socket, buffer, BUF - 1, 0);
        if(size <= 0) {
            if(size == 0) {
                printf("Client disconnected\n");
            } else {
                perror("recv error");
            }
            break; // Exit on error or client disconnect
        }
        buffer[size] = '\0'; // Null-terminate the string
        printf("\nReceived from client: %s\n", buffer); // Print received data
        
        // Check command type (exact matching with strcmp)
        if(strcmp(buffer, "LOGIN") == 0) {
            // Process the LOGIN command
            printf("Receive LOGIN command\r\n");
            handleLdapLogin(client_socket);
        } else if(strcmp(buffer, "SEND") == 0) {
            // Process the SEND command
            printf("Receive SEND command\r\n");
            handleSendCommand(client_socket, mail_spool_dir);
        } else if(strcmp(buffer, "QUIT") == 0) {
            // Process the QUIT command (client disconnect)
            break;
        } else if(strcmp(buffer, "LIST") == 0) {
            // Process the LIST command
            printf("Receive LIST command\r\n");
            handleListCommand(client_socket, mail_spool_dir);
        } else if(strcmp(buffer, "READ") == 0) {
            // Process the READ command
            printf("Receive READ command\r\n");
            handleReadCommand(client_socket, mail_spool_dir);
        } else if(strcmp(buffer, "DEL") == 0) {
            // Process the DEL command
            printf("Receive DEL command\n");
            handleDelCommand(client_socket, mail_spool_dir);
        } else {
            // Unknown command
            printf("Unknown command received: %s\n", buffer);
            send(client_socket, "Unknown command\n", 16, 0);
        }
    }

    removeSession(client_socket); //Remove the session

    close(client_socket); // Close client socket
    return NULL;
}