#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <pthread.h>

#define MAX_CLIENTS 100
#define USERNAME_MAX_LENGTH 81

typedef struct {
    int client_socket;
    char username[USERNAME_MAX_LENGTH];
} Session;

// Array zur Speicherung von Sessions
extern Session sessions[MAX_CLIENTS];
extern pthread_mutex_t sessions_lock;

// Funktionen zur Verwaltung von Sessions
void addSession(int client_socket, const char *username);
const char *getSessionUsername(int client_socket);
void removeSession(int client_socket);

#endif // SESSION_MANAGER_H
