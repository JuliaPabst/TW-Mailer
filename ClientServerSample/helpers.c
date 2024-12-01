#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void signalHandler(int sig) {
    // Suppress unused parameter warning
    (void)sig;

    printf("Signal received: %d\n", sig);
    fflush(stdout);

    // Handle cleanup or graceful shutdown
    exit(0);
}

void *clientCommunication(void *data) {
    // Suppress unused parameter warning
    (void)data;

    printf("Handling client communication...\n");
    fflush(stdout);

    // Returning NULL as a placeholder
    return NULL;
}
