#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include "helpers.h"
#include "ldap_functions.h"
#include <pthread.h>

int abortRequested = 0;
int create_socket = -1;

// Struct for delivering data to the thread
typedef struct {
    int client_socket;
    const char *mail_spool_dir;
} ThreadData;

void *clientHandler(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    clientCommunication(&(data->client_socket), data->mail_spool_dir);
    free(data);
    pthread_exit(NULL);
}

// Signal handler for clean shutdown
void signalHandler(int sig) {
    printf("Signal %d received. Shutting down now...\n", sig);
    abortRequested = 1;
    close(create_socket);
}

int main(int argc, char **argv) {
    socklen_t addrlen;
    struct sockaddr_in address, cliaddress;
    int reuseValue = 1;
    int port;
    char *mail_spool_dir;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <mail-spool-directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    mail_spool_dir = argv[2];

    // Signal handler registration (Ctrl+C catch)
    if(signal(SIGINT, signalHandler) == SIG_ERR) {
        perror("Signal cannot be registered");
        return EXIT_FAILURE;
    }

    // Socket creation
    if((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket error");
        return EXIT_FAILURE;
    }

    // Set socket options
    if(setsockopt(create_socket, SOL_SOCKET, SO_REUSEADDR, &reuseValue, sizeof(reuseValue)) == -1) {
        perror("set socket options - reuseAddr");
        return EXIT_FAILURE;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket to address
    if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind error");
        return EXIT_FAILURE;
    }

    // Listen for incoming connections
    if (listen(create_socket, 5) == -1) {
        perror("listen error");
        return EXIT_FAILURE;
    }

    printf("Server running on port %d, mail spool directory: %s\n", port, mail_spool_dir);

    while (!abortRequested) {
        printf("Waiting for connections...\n");

        addrlen = sizeof(struct sockaddr_in);

        int *new_socket = malloc(sizeof(int));
        *new_socket = accept(create_socket, (struct sockaddr *)&cliaddress, &addrlen);
        
        if(*new_socket == -1) {
            perror("accept error");
            free(new_socket);
            continue;
        }

        printf("Client connected from %s:%d...\n",
               inet_ntoa(cliaddress.sin_addr),
               ntohs(cliaddress.sin_port));

        ThreadData *thread_data = malloc(sizeof(ThreadData));
        if(thread_data == NULL) {
            perror("Failed to allocate memory for thread data");
            close(*new_socket);
            free(new_socket);
            continue;
        }

        thread_data->client_socket = *new_socket;
        thread_data->mail_spool_dir = mail_spool_dir;

        pthread_t thread;
        if(pthread_create(&thread, NULL, clientHandler, (void *)thread_data) != 0) {
            perror("Failedd to create thread");
            close(*new_socket);
            free(thread_data);
            continue;
        }

        pthread_detach(thread);
    }

    if(create_socket != -1) {
        close(create_socket);
    }
    printf("Server shut down.\n");
    return EXIT_SUCCESS;
}