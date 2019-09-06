#include "pipe_header.h"

int main(int argc, char const *argv[]) {
    int n;
    int fd[2];
    pid_t pid;
    const int maxline = 100;
    char line[maxline];

    if (pipe(fd) < 0) 
        errExit("pipe");
    if ((pid = fork()) < 0) {
        errExit("fork");
    } else if (pid > 0) {
        close(fd[0]);
        write(fd[1], "hello world\n", 12);
    } else {
        close(fd[1]);
        n = read(fd[0], line, maxline);
        write(STDOUT_FILENO, line, n);
    }
    exit(0);
}
