# Parent Makefile

# Compiler and flags
CC=g++
CFLAGS=-g -Wall -Wextra -Werror -O -std=c++14 -pthread

# Library flags for the LDAP project
LIBS=-lldap -llber

# Directories for both projects
LDAP_DIR=LDAPSample
SERVER_CLIENT_DIR=ClientServerSample

# Bin and Obj directories for both projects
LDAP_BIN_DIR=$(LDAP_DIR)/bin
LDAP_OBJ_DIR=$(LDAP_DIR)/obj

SERVER_BIN_DIR=$(SERVER_CLIENT_DIR)/bin
SERVER_OBJ_DIR=$(SERVER_CLIENT_DIR)/obj

.PHONY: all clean rebuild

# Rebuild everything
rebuild: clean all

# Build all projects
all: ldap_project client_server_project

# Clean all projects
clean:
	rm -f $(LDAP_BIN_DIR)/* $(LDAP_OBJ_DIR)/* $(SERVER_BIN_DIR)/* $(SERVER_OBJ_DIR)/*

# Targets for the LDAP project
ldap_project: $(LDAP_BIN_DIR)/ldapclient

$(LDAP_OBJ_DIR)/ldapclient.o: $(LDAP_DIR)/myldap.c
	$(CC) $(CFLAGS) -o $(LDAP_OBJ_DIR)/ldapclient.o $(LDAP_DIR)/myldap.c -c

$(LDAP_OBJ_DIR)/mypw.o: $(LDAP_DIR)/mypw.c
	$(CC) $(CFLAGS) -o $(LDAP_OBJ_DIR)/mypw.o $(LDAP_DIR)/mypw.c -c

$(LDAP_BIN_DIR)/ldapclient: $(LDAP_OBJ_DIR)/ldapclient.o $(LDAP_OBJ_DIR)/mypw.o
	$(CC) $(CFLAGS) -o $(LDAP_BIN_DIR)/ldapclient $(LDAP_OBJ_DIR)/mypw.o $(LDAP_OBJ_DIR)/ldapclient.o $(LIBS)

# Targets for the Client-Server project
client_server_project: $(SERVER_BIN_DIR)/server $(SERVER_BIN_DIR)/client

$(SERVER_OBJ_DIR)/myclient.o: $(SERVER_CLIENT_DIR)/myclient.c
	$(CC) $(CFLAGS) -o $(SERVER_OBJ_DIR)/myclient.o $(SERVER_CLIENT_DIR)/myclient.c -c

$(SERVER_OBJ_DIR)/myserver.o: $(SERVER_CLIENT_DIR)/myserver.c
	$(CC) $(CFLAGS) -o $(SERVER_OBJ_DIR)/myserver.o $(SERVER_CLIENT_DIR)/myserver.c -c

$(SERVER_OBJ_DIR)/helpers.o: $(SERVER_CLIENT_DIR)/helpers.c
	$(CC) $(CFLAGS) -o $(SERVER_OBJ_DIR)/helpers.o $(SERVER_CLIENT_DIR)/helpers.c -c

$(SERVER_BIN_DIR)/server: $(SERVER_OBJ_DIR)/myserver.o $(SERVER_OBJ_DIR)/helpers.o
	$(CC) $(CFLAGS) -o $(SERVER_BIN_DIR)/server $(SERVER_OBJ_DIR)/myserver.o $(SERVER_OBJ_DIR)/helpers.o

$(SERVER_BIN_DIR)/client: $(SERVER_OBJ_DIR)/myclient.o
	$(CC) $(CFLAGS) -o $(SERVER_BIN_DIR)/client $(SERVER_OBJ_DIR)/myclient.o
