#include "../ns_in_opt_header.h"

static int childFunc(void *arg);

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

int main(int argc, char *argv[]) {
    pid_t pid;

    pid = clone(childFunc, child_stack + STACK_SIZE, CLONE_NEWUSER | SIGCHLD, argv[1]);
    if (pid == -1)
        errExit("clone");
    if (waitpid(pid, NULL, 0) == -1)
        errExit("waitpid");
    return 0;
}

int childFunc(void *arg) {
    cap_t caps;
    for (;;) {
        printf("eUID = %ld; eGID = %ld; \n", (long)geteuid(), (long)getegid());
        caps = cap_get_proc();
        printf("capabilityies: %s\n", cap_to_text(caps, NULL));
        if (arg == NULL)
            break;
        sleep(5);
    }
    return 0;
}
