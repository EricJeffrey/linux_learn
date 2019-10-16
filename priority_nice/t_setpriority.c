#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

int getmyprio() {
    int curNice = getpriority(PRIO_PROCESS, 0);
    if (curNice == -1 && errno != 0) {
        fprintf(stderr, "getpriority error!\nerrno: %d, strerror: %s\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return curNice;
}

int main(int argc, char const *argv[]) {
    __pid_t pid = 2782;
    errno = 0;
    int currNice = getmyprio();
    printf("current nice: %d\n", currNice);
    printf("current RLIMIT_NICE: %d\n", RLIMIT_NICE);
    int target = -1;
    printf("set my nice to %d\n", target);
    int ret = setpriority(PRIO_PROCESS, 0, target);
    if (ret == -1) {
        fprintf(stderr, "setpriority failed\n");
        exit(EXIT_FAILURE);
    }
    printf("current nice: %d\n", getmyprio());
    return 0;
}
