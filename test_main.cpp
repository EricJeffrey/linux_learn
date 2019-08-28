#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wordexp.h>

#define errExit(A)                \
    do {                          \
        printf("error: %s\n", A); \
        exit(EXIT_FAILURE);       \
    } while (0)

int childFunc(void *arg) {
    printf("child: ok here\n");
    return 0;
}
#define STACK_SIZE (1024 * 1024)
static char sta[STACK_SIZE];

int main(int argc, char const *argv[]) {
    pid_t child;
    child = clone(childFunc, sta + STACK_SIZE, CLONE_NEWPID | CLONE_NEWUSER | SIGCHLD, NULL);
    if (child == -1)
        errExit("clone");
    if (waitpid(child, NULL, 0) == -1)
        errExit("wait");
    printf("parent: ok here\n");
    return 0;
}
