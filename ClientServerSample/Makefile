CC=g++
CFLAGS=-g -Wall -Wextra -Werror -O -std=c++14 -pthread

BIN_DIR=bin
OBJ_DIR=obj

rebuild: clean all

all: $(BIN_DIR)/server $(BIN_DIR)/client

clean:
	rm -f $(BIN_DIR)/* $(OBJ_DIR)/*

# Compile myclient.c into myclient.o
$(OBJ_DIR)/myclient.o: myclient.c
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/myclient.o myclient.c -c

# Compile myserver.c into myserver.o
$(OBJ_DIR)/myserver.o: myserver.c
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/myserver.o myserver.c -c

# Compile helpers.c into helpers.o
$(OBJ_DIR)/helpers.o: helpers.c
	$(CC) $(CFLAGS) -o $(OBJ_DIR)/helpers.o helpers.c -c

# Link myserver.o and helpers.o to create the server executable
$(BIN_DIR)/server: $(OBJ_DIR)/myserver.o $(OBJ_DIR)/helpers.o
	$(CC) $(CFLAGS) -o $(BIN_DIR)/server $(OBJ_DIR)/myserver.o $(OBJ_DIR)/helpers.o

# Link myclient.o to create the client executable
$(BIN_DIR)/client: $(OBJ_DIR)/myclient.o
	$(CC) $(CFLAGS) -o $(BIN_DIR)/client $(OBJ_DIR)/myclient.o
