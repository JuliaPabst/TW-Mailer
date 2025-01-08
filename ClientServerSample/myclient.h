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
#include <termios.h> // For terminal control (e.g., disabling echo for password input)

// Constants
#define BUF 1024

// Function Prototypes
void sendMessage(int socket, const char *message);
int isValidInput(const char *input, int maxLength);
char *handleLoginCommand(int create_socket, char *username);
void handleSendCommand(int create_socket, char *username);
void handleListCommand(int create_socket,char *username);
void handleReadCommand(int create_socket, char *username);
void handleDelCommand(int create_socket, char *username);


#endif // MYCLIENT_H
