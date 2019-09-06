#if !defined(PIPE_HEADER)
#define PIPE_HEADER

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#define errExit(A)                       \
    do {                                 \
        fprintf(stderr, "error: %s", A); \
    } while (0)

#endif // PIPE_HEADER
