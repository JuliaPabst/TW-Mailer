#include "session_manager.h"
#include <string.h>
#include <stdio.h>

Session sessions[MAX_CLIENTS] = {0}; // initialize session
pthread_mutex_t sessions_lock = PTHREAD_MUTEX_INITIALIZER;

void addSession(int client_socket, const char *username) {
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sessions[i].client_socket == 0) { // free slot
            sessions[i].client_socket = client_socket;
            strncpy(sessions[i].username, username, USERNAME_MAX_LENGTH - 1);
            sessions[i].username[USERNAME_MAX_LENGTH - 1] = '\0';
            pthread_mutex_unlock(&sessions_lock);
            return;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    fprintf(stderr, "ERROR: No free session slots available.\n");
}

const char *getSessionUsername(int client_socket) {
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sessions[i].client_socket == client_socket) {
            pthread_mutex_unlock(&sessions_lock);
            return sessions[i].username;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    return NULL;
}

void removeSession(int client_socket) {
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sessions[i].client_socket == client_socket) {
            sessions[i].client_socket = 0;
            sessions[i].username[0] = '\0';
            pthread_mutex_unlock(&sessions_lock);
            return;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
}
