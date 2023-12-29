
#pragma once

// Contents of the header file
#define PRINT(x) {fprintf(stderr, "%d\n", x);} // DEBUG: print integers
#define PRINTE {fprintf(stderr, "e\n");} // DEBUG: print e
extern const int IOBUFFER_SIZE; // read/write buffers
extern const int ERRBUFFER_SIZE; // snprintf & f_print buffers to terminal

int get_argc(char* argv[]);

void* safe_malloc(size_t size);

void safe_signal(int signum, void(*handler)(int));