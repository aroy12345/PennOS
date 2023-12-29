#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

const int IOBUFFER_SIZE = 10000; // for read/write buffers
const int ERRBUFFER_SIZE = 1000; // for snprintf & f_print buffers to terminal

/**
 * Get the number of arguments in an array of strings.
 * @param argv The array of strings.
 * @return The number of arguments.
 */
int get_argc(char* argv[]) {
    int argc = 0;
    while (argv[argc] != NULL) {
        argc++;
    }
    return argc;
}

/**
 * Allocate memory using malloc.
 * @param size The size of the memory to allocate.
 * @return A pointer to the allocated memory.
 */
void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

/**
 * Set a signal handler using signal.
 * @param signum The signal number.
 * @param handler The signal handler function.
 */
void safe_signal(int signum, void(*handler)(int)) {
    // fprintf(stderr, "signaled\n");
    if (signal(signum, handler) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }
}