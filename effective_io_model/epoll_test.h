#if !defined(EPOLL_TEST_H)
#define EPOLL_TEST_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// extern int epoll_ctl (int __epfd, int __op, int __fd, struct epoll_event *__event) __THROW;

const char resp[] = "HTTP/1.1 200\r\n\
Content-Type: application/json\r\n\
Content-Length: 13\r\n\
Date: Thu, 13 Aug 2020 08:02:00 GMT\r\n\
Keep-Alive: timeout=60\r\n\
Connection: keep-alive\r\n\r\n\
[HELLO WORLD]\r\n\r\n";
void errExit() {
    fprintf(stderr, "ERROR! errno: %s\n", strerror(errno));
    exit(1);
}

static epoll_event tempEvent;

void setTempEvent(int fd, int events) {
    memset(&tempEvent, 0, sizeof(tempEvent));
    tempEvent.data.fd = fd, tempEvent.events = events;
}

void workEpollTest() {
    // freopen("./foo.txt", "wt", stderr);
    const int port = 2333;
    int sd, ret;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    fprintf(stderr, "created socket\n");
    if (sd == -1)
        errExit();
    int opt = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1)
        errExit();
    fprintf(stderr, "socket reuse set\n");
    sockaddr_in addr;
    addr.sin_family = AF_INET, addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sd, (sockaddr *)&addr, sizeof(addr)) == -1)
        errExit();
    fprintf(stderr, "socket binded\n");
    if (listen(sd, 1024) == -1)
        errExit();
    fprintf(stderr, "socket listen start\n");

    // ----------------------------------------------- epoll创建实例
    int epFd = epoll_create1(0);
    if (epFd == -1)
        errExit();
    fprintf(stderr, "epoll instance created\n");
    setTempEvent(sd, EPOLLIN);
    ret = epoll_ctl(epFd, EPOLL_CTL_ADD, sd, &tempEvent);
    if (ret == -1)
        errExit();
    fprintf(stderr, "listen socket add to epoll\n");
    // ----------------------------------------------- 开始接收连接
    epoll_event *eventList = new epoll_event[100];
    while (true) {
        ret = epoll_wait(epFd, eventList, 100, -1);
        fprintf(stderr, "epoll returned with value: %d\n", ret);
        if (ret == -1) {
            if (errno == EINTR)
                fprintf(stderr, "epoll interrupted by signal, ignoring\n");
            else
                errExit();
        } else if (ret == 0) {
            fprintf(stderr, "no events on socket\n");
        } else {
            // ----------------------------------------------- 检查有事件的描述符
            fprintf(stderr, "checking fds\n");
            for (int i = 0; i < ret; i++) {
                if (eventList[i].data.fd == sd) {
                    if (eventList[i].events & EPOLLIN) {
                        sockaddr_in addr;
                        socklen_t addrLen;
                        int childSd = accept(sd, (sockaddr *)&addr, &addrLen);
                        fprintf(stderr, "client connection got, childSd: %d\n", childSd);
                        if (childSd == -1)
                            errExit();
                        // ret = fcntl(childSd, F_SETFL, O_NONBLOCK);
                        // if (ret == -1)
                        //     errExit();
                        // fprintf(stderr, "child sd set to NON BLOCK\n");
                        setTempEvent(childSd, EPOLLIN);
                        ret = epoll_ctl(epFd, EPOLL_CTL_ADD, childSd, &tempEvent);
                        if (ret == -1)
                            errExit();
                        fprintf(stderr, "childSd add to epoll list\n");
                    } else {
                        fprintf(stderr, "Unknown events on sd!\n");
                        close(epFd);
                        errExit();
                    }
                } else {
                    if (eventList[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
                        // close
                        fprintf(stderr, "events hup on sd, closing\n");
                        close(eventList[i].data.fd);
                        epoll_ctl(epFd, EPOLL_CTL_DEL, eventList[i].data.fd, nullptr);
                    } else if (eventList[i].events & EPOLLIN) {
                        const static int buf_sz = 505;
                        char buffer[buf_sz] = {};
                        while (1) {
                            ret = read(eventList[i].data.fd, buffer, buf_sz);
                            // 阻塞模式下每一次read读取定长数据
                            fprintf(stderr, "read on: %d returned with value: %d\n",
                                    eventList[i].data.fd, ret);
                            if (ret == 0) {
                                fprintf(stderr, "read return EOF, connection closed\n");
                                close(eventList[i].data.fd);
                                epoll_ctl(epFd, EPOLL_CTL_DEL, eventList[i].data.fd, nullptr);
                                break;
                            }
                            if (ret == -1) {
                                const int tmpErrno = errno;
                                if (tmpErrno == EWOULDBLOCK || tmpErrno == EAGAIN) {
                                    fprintf(stderr, "read would block, stop reading\n");
                                    // read is over
                                    // http pipe line? need to put resp into a queue
                                    setTempEvent(eventList[i].data.fd,
                                                 EPOLLIN | EPOLLOUT | EPOLLHUP);
                                    epoll_ctl(epFd, EPOLL_CTL_MOD, eventList[i].data.fd,
                                              &tempEvent);
                                    break;
                                } else {
                                    errExit();
                                }
                            }
                            break;
                        }
                    } else if (eventList[i].events & EPOLLOUT) {
                        ret = write(eventList[i].data.fd, resp, sizeof(resp));
                        fprintf(stderr, "write on: %d returned with value: %d\n", i, ret);
                        if (ret == -1)
                            // may be interrupted by signal
                            errExit();
                        setTempEvent(eventList[i].data.fd, EPOLLIN);
                        epoll_ctl(epFd, EPOLL_CTL_MOD, eventList[i].data.fd, &tempEvent);
                    }
                }
            }
        }
    }
}

#endif // EPOLL_TEST_H
