#include "pipe_header.h"

#define DEF_PAGER "/bin/more"
#define VERBOSE 1
#define log(A)                    \
    do {                          \
        if (VERBOSE) {            \
            printf("%s...\n", A); \
        }                         \
    } while (0)

int main(int argc, char *argv[]) {
    int n;
    int fd[2];
    pid_t pid;
    char *pager, *argv0;
    const int maxline = 100;
    char line[maxline];
    FILE *fp;

    if (argc != 2)
        errExit("usage: a.sh <pathname>");
    log("opening file");
    if ((fp = fopen(argv[1], "r")) == NULL)
        errExit("can't open argv[1]");
    log("create pipe");
    if (pipe(fd) < 0)
        errExit("pipe");
    log("create child process");
    if ((pid = fork()) < 0) {
        errExit("fork");
    } else if (pid > 0) {
        // parent
        close(fd[0]);
        log("parent: reading file");
        while (fgets(line, maxline, fp) != NULL) {
            n = strlen(line);
            if (write(fd[1], line, n) != n)
                errExit("write");
        }
        if (ferror(fp))
            errExit("fgets error");
        close(fd[1]);
        if (waitpid(pid, NULL, 0) < 0)
            errExit("waitpid");
        log("parent: exiting");
        exit(0);
    } else {
        // child
        close(fd[1]);
        log("child: performing dup");
        if (fd[0] != STDIN_FILENO) {
            if (dup2(fd[0], STDIN_FILENO) != STDIN_FILENO) {
                errExit("dup2");
            }
            close(fd[0]);
        }
        if ((pager = getenv("PAGER")) == NULL)
            pager = DEF_PAGER;
        if ((argv0 = strrchr(pager, '/')) != NULL)
            argv0++;
        else
            argv0 = pager;
        log("child: executing pager");
        if (execl(pager, argv0, (char *)0) < 0)
            errExit("execl");
    }
    exit(0);

    return 0;
}
