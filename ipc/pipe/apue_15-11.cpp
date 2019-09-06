#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define PAGER "${PAGER:-more}"
#define errExit(A)                                                                 \
    do {                                                                           \
        printf("error: %s, errno: %d, strerror: %s\n", A, errno, strerror(errno)); \
        exit(EXIT_FAILURE);                                                        \
    } while (0)

int main(int argc, char *argv[]) {
    const int maxline = 1024;
    char line[maxline];
    FILE *fpin, *fpout;

    if (argc != 2)
        errExit("usage: a.out <pathname>");
    if ((fpin = fopen(argv[1], "r")) == NULL)
        errExit("can not open argv1");

    if ((fpout = popen(PAGER, "w")) == NULL)
        errExit("popen");

    while (fgets(line, maxline, fpin) != NULL)
        if (fputs(line, fpout) == EOF)
            errExit("fputs");
    if (ferror(fpin))
        errExit("fgets");
    if (pclose(fpout) == -1)
        errExit("pclose");

    exit(0);
}
