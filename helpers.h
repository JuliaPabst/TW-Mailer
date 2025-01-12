#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>
#include "ldap_functions.h"
#include <dirent.h>
#include <errno.h>
#include "ldap_functions.h"
#include "session_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>      
#include <sys/socket.h>
#include <sys/stat.h>


#define BUF 1024
#define MESSAGE_BUF 4096
#define MAX_ATTEMPTS 3
#define BLACKLIST_DURATION 60   // in seconds
#define BLACKLIST_FILE "blacklist.txt"

void trim(char *str);
int isValidUsername(const char *username);
void handleLdapLogin(int client_socket);
void handleSendCommand(int client_socket, const char *mail_spool_dir);
void handleListCommand(int client_socket, const char *mail_spool_dir);
void handleDelCommand(int client_socket, const char *mail_spool_dir);
void *clientCommunication(void *data, const char *mail_spool_dir);
int readline(int socket, char *buffer, size_t size);
int isBlackListed(const char *ip, time_t *remaining_time);
void addToBlackList(const char *ip);
void resetLoginAttempts(const char *ip);
void recordFailedAttempt(const char *ip);
int getFailedAttempts(const char *ip);


#endif // HELPERS_H
