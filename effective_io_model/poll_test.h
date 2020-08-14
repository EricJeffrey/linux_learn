#if !defined(POLL_TEST_H)
#define POLL_TEST_H

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <errno.h>
#include <cstring>
#include <vector>
#include <initializer_list>

using std::initializer_list;
using std::vector;

/*

accept - POLLIN

对于客户端连接
    1. 只接受读事件 - POLLIN
    2. 读取到一个请求后，可以读和写 - POLLIN | POLLOUT
    3. 写完一个响应
        1. 如果有待写响应 - 不变
        2. 没有待写则，只允许读 - POLLIN

*/

void errExit() {
    fprintf(stderr, "ERROR! error %s\n", strerror(errno));
    exit(1);
}

const char resp[] = "HTTP/1.1 200\r\n\
Content-Type: application/json\r\n\
Content-Length: 13\r\n\
Date: Thu, 13 Aug 2020 08:02:00 GMT\r\n\
Keep-Alive: timeout=60\r\n\
Connection: keep-alive\r\n\r\n\
[HELLO WORLD]\r\n\r\n";

void workPollTest() {
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
    fprintf(stderr, "socket opt set\n");
    sockaddr_in addr;
    addr.sin_family = AF_INET, addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t addrLen = sizeof(addr);
    if (bind(sd, (sockaddr *)&addr, sizeof(addr)) == -1)
        errExit();
    fprintf(stderr, "socket binded\n");
    if (listen(sd, 1024) == -1)
        errExit();
    fprintf(stderr, "socket listen start\n");

    // poll: poll (struct pollfd *__fds, nfds_t __nfds, int __timeout)

    // number of poll fds
    int currentFdNum = 1;
    pollfd *fds = static_cast<pollfd *>(calloc(100, sizeof(pollfd)));
    fds[0].fd = sd, fds[0].events = POLLIN;
    nfds_t nfds = 1;
    int timeout = -1;

    fprintf(stderr, "polling\n");
    while (1) {
        ret = poll(fds, nfds, timeout);
        fprintf(stderr, "poll returned with ret value: %d\n", ret);
        if (ret == -1)
            errExit();
        else if (ret == 0) {
            fprintf(stderr, "return no data\n");
        } else { // ret > 0
            // got accept
            fprintf(stderr, "checking fds\n");
            if (fds[0].revents & POLLIN) {
                sockaddr_in childAddr;
                socklen_t childAddrLen;
                int childSd = accept(sd, (sockaddr *)&childAddr, &(childAddrLen));
                if (childSd == -1)
                    errExit();
                fprintf(stderr, "child got\n");
                // set non_block
                int flags = fcntl(childSd, F_GETFL);
                if (fcntl(childSd, F_SETFL, flags | O_NONBLOCK) == -1)
                    errExit();
                fprintf(stderr, "child set nonblock\n");
                // add child to list
                fds[currentFdNum].fd = childSd, fds[currentFdNum].events = (POLLIN | POLLRDHUP);
                nfds++, currentFdNum++;
                fprintf(stderr, "child: %d pushed to poll list\n", currentFdNum - 1);
            }
            // child read & write
            for (int i = 1; i < currentFdNum; i++) {
                if (fds[i].revents & (POLLHUP | POLLRDHUP | POLLNVAL)) {
                    // set not interested
                    fprintf(stderr, "child: %d shutdown\n", i);
                    close(fds[i].fd);
                    fds[i].events = 0;
                    fds[i].fd = -1;
                    continue;
                }
                //  read
                if (fds[i].revents & POLLIN) {
                    char buffer[1024] = {};
                    while (1) {
                        ret = read(fds[i].fd, buffer, 1024);
                        fprintf(stderr, "read on: %d returned with value: %d\n", i, ret);
                        if (ret == 0) {
                            fprintf(stderr, "read returned 0(EOF) on: %d, breaking\n", i);
                            break;
                        }
                        if (ret == -1) {
                            const int tmpErrno = errno;
                            if (tmpErrno == EWOULDBLOCK || tmpErrno == EAGAIN) {
                                fprintf(stderr, "read would block, stop reading\n");
                                // read is over
                                // http pipe line? need to put resp into a queue
                                fds[i].events |= POLLOUT;
                                break;
                            } else {
                                errExit();
                            }
                        }
                    }
                }
                // write
                if (fds[i].revents & POLLOUT) {
                    ret = write(fds[i].fd, resp, sizeof(resp));
                    fprintf(stderr, "write on: %d returned with value: %d\n", i, ret);
                    if (ret == -1) {
                        errExit();
                    }
                    fds[i].events &= !(POLLOUT);
                }
            }
        }
    }
}

#endif // POLL_TEST_H
