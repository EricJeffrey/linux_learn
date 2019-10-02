
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define errExit(A)                                                                         \
    do {                                                                                   \
        fprintf(stderr, "error! %s.\nerrno: %d, strerr: %s\n", A, errno, strerror(errno)); \
        exit(EXIT_FAILURE);                                                                \
    } while (0)

// !!! BUGGY CODE !!!
// 代码有bug，仅展示如何使用 epoll

int main(int argc, char const *argv[]) {
    int fdfoo, fdbar;
    fdfoo = open("p", O_RDONLY);
    if (fdfoo == -1)
        errExit("open p");
    fdbar = open("q", O_RDONLY);
    if (fdbar == -1)
        errExit("open q");
    int epfd;
    epfd = epoll_create(1);
    if (epfd == -1)
        errExit("epoll create");

    epoll_event ev;
    int ret = 0;
    ev.data.fd = fdfoo;
    ev.events = EPOLLIN;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fdfoo, &ev);
    if (ret == -1)
        errExit("epoll ctl p");

    ev.data.fd = fdbar;
    ev.events = EPOLLIN;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fdbar, &ev);
    if (ret == -1)
        errExit("epoll ctl q");

    int cnt = 2;
    const int max_events = 5;
    epoll_event evlist[max_events];
    while (cnt > 0) {
        printf("main: epoll wait");
        ret = epoll_wait(epfd, evlist, max_events, -1);
        if (ret == -1) {
            if (errno == EINTR)
                continue;
            else
                errExit("epoll wait");
        }
        const int buf_size = 1024;
        char buf[buf_size] = {};
        for (int i = 0; i < ret; i++) {
            epoll_event tmpev = evlist[i];
            if (tmpev.events & EPOLLIN) {
                ret = read(tmpev.data.fd, buf, buf_size);
                if (ret == -1)
                    errExit("read");
                printf("main: read %d bytes: %.*s\n", ret, ret, buf);
            } else if (tmpev.events & (EPOLLHUP | EPOLLERR)) {
                printf("main: closing fd %d\n", tmpev.data.fd);
                if (close(tmpev.data.fd) == -1)
                    errExit("close");
                cnt -= 1;
            }
        }
    }
    printf("All file descriptors closed; bye\n");
    return 0;
}
