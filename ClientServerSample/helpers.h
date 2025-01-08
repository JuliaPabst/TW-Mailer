#ifndef HELPERS_H
#define HELPERS_H

#include <stdio.h>
#include "ldap_functions.h"

#define BUF 1024
#define MESSAGE_BUF 4096

void signalHandler(int sig);
int isValidUsername(const char *username);
void handleSendCommand(int client_socket, const char *mail_spool_dir);
void handleListCommand(int client_socket, const char *mail_spool_dir);
void handleDelCommand(int client_socket, const char *mail_spool_dir);
void *clientCommunication(void *data, const char *mail_spool_dir);
void handleLdapLogin(int client_socket);

#endif // HELPERS_H