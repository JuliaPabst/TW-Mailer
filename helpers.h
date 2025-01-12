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

void trim(char *str);
void signalHandler(int sig);
int isValidUsername(const char *username);
void handleLdapLogin(int client_socket);
void handleSendCommand(int client_socket, const char *mail_spool_dir);
void handleListCommand(int client_socket, const char *mail_spool_dir);
void handleDelCommand(int client_socket, const char *mail_spool_dir);
void *clientCommunication(void *data, const char *mail_spool_dir);


#endif // HELPERS_H
