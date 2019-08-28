
#include "ns_in_opt_header.h"

static void usage(char *pname);
static int childFunc(void *arg);

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];
static int do_mount = 0;

int main(int argc, char *argv[]) {
    int flags, opt, verbose;
    pid_t child_pid;

    flags = verbose = 0;

    /* Parse command-line options. The initial '+' character in
       the final getopt() argument prevents GNU-style permutation
       of command-line options. That's useful, since sometimes
       the 'command' to be executed by this program itself
       has command-line options. We don't want getopt() to treat
       those as options to this program. */
    while ((opt = getopt(argc, argv, "+imMnpuUv")) != -1) {
        switch (opt) {
        case 'i':
            flags |= CLONE_NEWIPC;
            break;
        case 'M':
            do_mount = 1;
        case 'm':
            flags |= CLONE_NEWNS;
            break;
        case 'n':
            flags |= CLONE_NEWNET;
            break;
        case 'p':
            flags |= CLONE_NEWPID;
            break;
        case 'u':
            flags |= CLONE_NEWUTS;
            break;
        case 'U':
            flags |= CLONE_NEWUSER;
            break;
        case 'v':
            verbose = 1;
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    child_pid = clone(childFunc, child_stack + STACK_SIZE, flags | SIGCHLD, &argv[optind]);
    if (child_pid == -1)
        errExit("clone");

    if (verbose)
        printf("%s: PID of child created by clone() is %ld\n", argv[0], (long)child_pid);

    if (waitpid(child_pid, NULL, 0) == -1)
        errExit("waitpid");

    if (verbose)
        printf("%s: terminating\n", argv[0]);

    exit(EXIT_SUCCESS);
}

static int childFunc(void *arg) {
    char **argv = (char **)arg;
    if (do_mount) {
        mount("proc", "/proc", "proc", 0, NULL);
    }
    execvp(argv[0], &argv[0]);
    errExit("execvp");

    // pid_t child_pid;
    // if ((child_pid = fork()) == 0) {
    //     execvp(argv[0], &argv[0]);
    //     errExit("execvp");
    // }
    // int ret = waitpid(child_pid, NULL, 0);
    // if (do_mount)
    //     umount("proc");

    // if (ret == -1)
    //     errExit("waitpid");
    return 0;
}
static void usage(char *pname) {
    fprintf(stderr, "usage: %s [options] cmd [arg...]\n", pname);
    fprintf(stderr, "options can be: \n");
    fprintf(stderr, "   -i new IPC namespace\n");
    fprintf(stderr, "   -m new mount namespace\n");
    fprintf(stderr, "   -M mount proc to /proc in child\n");
    fprintf(stderr, "   -n new network namespace\n");
    fprintf(stderr, "   -p new PID namespace\n");
    fprintf(stderr, "   -u new UTS namespace\n");
    fprintf(stderr, "   -U new user namespace\n");
    fprintf(stderr, "   -v Display verbose message\n");
    exit(EXIT_FAILURE);
}