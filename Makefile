CC=gcc
CXX=g++
CFLAGS=-g -Wall -Wextra -Werror -O -std=c99 -pthread
LIBS=-lldap -llber

BIN_DIR=bin
OBJ_DIR=obj

rebuild: clean all

all: $(BIN_DIR)/server $(BIN_DIR)/client

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/*

# Compile myclient.c into myclient.o
$(OBJ_DIR)/myclient.o: myclient.c myclient.h
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/myclient.o -c myclient.c

# Compile myserver.c into myserver.o
$(OBJ_DIR)/myserver.o: myserver.c helpers.h ldap_functions.h session_manager.h
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/myserver.o -c myserver.c

# Compile helpers.c into helpers.o
$(OBJ_DIR)/helpers.o: helpers.c helpers.h
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/helpers.o -c helpers.c

# Compile ldap_functions.c into ldap_functions.o
$(OBJ_DIR)/ldap_functions.o: ldap_functions.c ldap_functions.h
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/ldap_functions.o -c ldap_functions.c

# Compile session_manager.c into session_manager.o
$(OBJ_DIR)/session_manager.o: session_manager.c session_manager.h
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/session_manager.o -c session_manager.c

# Link myserver.o, helpers.o, ldap_functions.o, and session_manager.o to create the server executable
$(BIN_DIR)/server: $(OBJ_DIR)/myserver.o $(OBJ_DIR)/helpers.o $(OBJ_DIR)/ldap_functions.o $(OBJ_DIR)/session_manager.o
	$(CC) $(CFLAGS) -o $(BIN_DIR)/server $(OBJ_DIR)/myserver.o $(OBJ_DIR)/helpers.o $(OBJ_DIR)/ldap_functions.o $(OBJ_DIR)/session_manager.o $(LIBS)

# Link myclient.o to create the client executable
$(BIN_DIR)/client: $(OBJ_DIR)/myclient.o
	$(CC) $(CFLAGS) -o $(BIN_DIR)/client $(OBJ_DIR)/myclient.o
