#ifndef MYCLIENT_H
#define MYCLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Constants
#define BUF 1024

// Function Prototypes
void sendMessage(int socket, const char *message);
int isValidInput(const char *input, int maxLength);
void handleSendCommand(int create_socket);
void handleListCommand(int create_socket);
void handleReadCommand(int create_socket);
void handleDelCommand(int create_socket);

#endif // MYCLIENT_H
