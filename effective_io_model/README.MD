使用poll实现简单服务器

# IO模型 - Poll

Linux系统提供了`select, poll, epoll`三种更加高效的IO模型，这里使用一个例子简单介绍一下`poll`在套接字上的使用。

传统的阻塞读写方式需要给每个连接分配一个线程，读取到一个请求后就响应，之后再读取，若客户端没有发送请求数据，则读取操作会阻塞。这样的方式显然浪费了系统资源，并且连接过多时线程数的增加也会给系统带来较大负担。

基于事件的IO模型允许服务端使用事件触发的方式，在客户端发送请求时触发读取`read`操作，并在写操作`write`不会阻塞的时候写数据。这种IO模型允许使用更少的线程处理更多的客户端连接，能够降低系统负担，提高应用的服务效率。

## 如何使用

简单明了:

```c
/* ... 套接字创建 ... */
int sd = socket(AF_INET, SOCK_STREAM, 0);
bind(...);
listen(...); // sd 上的POLLIN事件表示可以accept
//---------------------------------------------

// pollfd: {fd, events, revents} -> {文件描述符，感兴趣事件(bitmask)，返回的事件}
struct pollfd *fds = (struct pollfd *)(calloc(100, sizeof(pollfd)));
// 设置监听的套接字，关心的事件
fds[0].fd = sd, fds[0].events = POLLIN;
// 需要监听的pollfd数目
nfds_t nfds = 1;
// 超时值: -1 表示一直阻塞
int timeout = -1;
// 返回值: -1 错误，0 没有事件发送，>0 发生事件的描述符个数，事件设置在revents中
ret = poll(fds, nfds, timeout);

// 事件的值有 POLLIN POLLOUT POLLHUP 等，参考poll(2)
```

![套接字上的poll的事件指示符](./select_and_poll_indications_for_sockets.jpg)

## TCP服务器响应流程

对于TCP服务器来说，`bind+listen+accept`然后处理客户端的连接是必不可少的，不过在使用`poll`的时候，`accept`与客户端的`read+write`都可以在事件触发后执行，客户端连接需要设置为非阻塞的，避免read和write的阻塞，大致流程如下:

1. `socket() + bind() + listen()`创建套接字`sd`


2. 将`sd`加入到poll的描述符集`fds`中，并且监听上面的`POLLIN`事件

3. 调用`poll()`等待描述符集中的事件

   1. 若`fds[0].revents & POLLIN`，则表示客户端请求建立连接
      
      - 调用`accept`接收请求得到新连接`childSd`，设置新连接时非阻塞的`fcntl(childSd, F_SETFL, O_NONBLOCK)`
      
      - 将`childSd`加入到poll的描述符集中，监听其上的`POLLIN`事件: `fds[i].events = POLLIN`

   2. 若其他套接字`tmpSd`上有`POLLIN`事件，表示客户端发送请求数据
      
      - 读取数据，若读取完则监听`tmpSd`上的读和写事件: `fds[j].events = POLLIN | POLLOUT`
        
        > 读取遇到`EAGAIN | EWOULDBLOCK`，表示会阻塞，需要停止读等待下一次读事件
      
      - 否则，记录这次读取的数据，下一个读事件时继续执行读操作

   3. 若其他套接字`tmpSd`上有`POLLOUT`事件，表示客户端可写
      
      - 写入数据，若写入完，则清除`tmpSd`上的写事件
        
        > 同样，写如遇到`EAGAIN | EWOULDBLOCK`，表示会阻塞，需要停止写等待下一次写事件
      
      - 否则，下次写事件继续写

由于套接字上写事件一般都是可行的，所以初始不监听`POLLOUT`事件，否则poll会不停报道套接字上可写。

## 例子

```cpp
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

const char resp[] = "HTTP/1.1 200\r\n\
Content-Type: application/json\r\n\
Content-Length: 13\r\n\
Date: Thu, 13 Aug 2020 08:02:00 GMT\r\n\
Keep-Alive: timeout=60\r\n\
Connection: keep-alive\r\n\r\n\
[HELLO WORLD]\r\n\r\n";

void workPollTest() {
    // ----------------------------------- 创建套接字
    const int port = 2333;
    int sd, ret;
    sd = socket(AF_INET, SOCK_STREAM, 0);
    fprintf(stderr, "created socket\n");
    if (sd == -1)
        errExit();
    int opt = 1;
    // 重用地址
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
    // ----------------------------------- 套接字创建完毕

    // -----------------------------------初始化监听列表
    // number of poll fds
    int currentFdNum = 1;
    pollfd *fds = static_cast<pollfd *>(calloc(100, sizeof(pollfd)));
    fds[0].fd = sd, fds[0].events = POLLIN;
    nfds_t nfds = 1;
    int timeout = -1;

    fprintf(stderr, "polling\n");
    while (1) {
        // ----------------------------------- 执行poll操作
        ret = poll(fds, nfds, timeout);
        fprintf(stderr, "poll returned with ret value: %d\n", ret);
        if (ret == -1)
            errExit();
        else if (ret == 0) {
            fprintf(stderr, "return no data\n");
        } else { // ret > 0
            // got accept
            fprintf(stderr, "checking fds\n");
            // ----------------------------------- 检查是否有新客户端建立连接
            if (fds[0].revents & POLLIN) {
                sockaddr_in childAddr;
                socklen_t childAddrLen;
                int childSd = accept(sd, (sockaddr *)&childAddr, &(childAddrLen));
                if (childSd == -1)
                    errExit();
                fprintf(stderr, "child got\n");
                // set non_block
                int flags = fcntl(childSd, F_GETFL);
                // ----------------------------------- accept并设置为非阻塞
                if (fcntl(childSd, F_SETFL, flags | O_NONBLOCK) == -1)
                    errExit();
                fprintf(stderr, "child set nonblock\n");
                // add child to list
                // ----------------------------------- 假如到poll的描述符集，关心POLLIN事件
                fds[currentFdNum].fd = childSd, fds[currentFdNum].events = (POLLIN | POLLRDHUP);
                nfds++, currentFdNum++;
                fprintf(stderr, "child: %d pushed to poll list\n", currentFdNum - 1);
            }
            // child read & write
            // ----------------------------------- 检查其他描述符的事件
            for (int i = 1; i < currentFdNum; i++) {
                if (fds[i].revents & (POLLHUP | POLLRDHUP | POLLNVAL)) {
                    // ----------------------------------- 客户端描述符关闭
                    // ----------------------------------- 设置events=0, fd=-1，不再关心
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
                        // ----------------------------------- 读取请求数据
                        ret = read(fds[i].fd, buffer, 1024);
                        fprintf(stderr, "read on: %d returned with value: %d\n", i, ret);
                        if (ret == 0) {
                            fprintf(stderr, "read returned 0(EOF) on: %d, breaking\n", i);
                            break;
                        }
                        if (ret == -1) {
                            const int tmpErrno = errno;
                            // ----------------------------------- 会阻塞，这里认为读取完毕
                            // ----------------------------------- 实际需要检查读取数据是否完毕
                            if (tmpErrno == EWOULDBLOCK || tmpErrno == EAGAIN) {
                                fprintf(stderr, "read would block, stop reading\n");
                                // read is over
                                // http pipe line? need to put resp into a queue
                                // ----------------------------------- 可以监听写事件了 POLLOUT
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
                    // ----------------------------------- 写事件，把请求返回
                    ret = write(fds[i].fd, resp, sizeof(resp));
                    fprintf(stderr, "write on: %d returned with value: %d\n", i, ret);
                    // ----------------------------------- 这里需要处理 EAGAIN EWOULDBLOCK
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

```

# IO模型 - Epoll

简单介绍`epoll`在网络套接字上的使用。

首先是epoll的使用，主要步骤
1. 创建epoll实例 - `epoll_create1(0)`
2. 调用`epoll_ctl`添加、修改或者删除监听套接字。需要使用 `epoll_event`对象
    ```cpp
    struct epoll_event tmpEvent;
    tmpEvent.events = EPOLLIN, tmpEvent.data.fd = fd;
    ```
3. 使用`epoll_wait`监听

那么如果要使用epoll监听TCP连接，可以按照如下步骤：

1. `socket() + bind() + setsockopt() + listen()`得到套接字sd
2. 使用`epoll_create`创建epoll实例，得到`epFd`
3. 设置`tempEvent === {data.fd = fd, events = EPOLLIN}`，使用`epoll_ctl(epFd, EPOLL_CTL_ADD, sd, &tempEvent)`将sd假如到监听队列中
4. 创建一个`epoll_event`数组eventList，调用`epoll_wait`监听队列，根据返回值ret
    1. -1：出错
    2.  0：无事发生
    3.  \>0：则`eventList`前ret个对象为事件以及对应的描述符
    4. 对于 sd 上的EPOLLIN，表示可以accept
        1. `accept`得到childSd，设置其为非阻塞`O_NONBLOCK`
        2. 使用`epoll_ctl(epFd, EPOLL_CTL_ADD, childSd, tempEvent)`加入到epoll中，事件为`EPOLLIN`
        3. 之所以不监听`EPOLLOUT`是因为套接字的写操作总是可行，故会一直触发事件。应当在读取到请求后才触发可写的监听
    6. 对于其他 sd
        1. 首先判断是否 `(EPOLLHUP | EPOLLRDHUP)`，表示出错
        2. 否则，若`EPOLLIN`，则读取数据
            - 若读取到`EOF`（返回值为0），则表示连接已断开
            - `EWOULDBLOCK or EAGAIN`，表示没有数据可读，设置套接字的`EPOLLOUT`事件监听
        3. 否则，若`EPOLLOUT`，则写入数据，并取消套接字的`EPOLLOUT`事件监听

上述读写以及epoll_wait都有可能呗信号打断，需要处理一下。

代码实例:
```cpp
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
                        ret = fcntl(childSd, F_SETFL, O_NONBLOCK);
                        if (ret == -1)
                            errExit();
                        fprintf(stderr, "child sd set to NON BLOCK\n");
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
                        char buffer[1024] = {};
                        while (1) {
                            ret = read(eventList[i].data.fd, buffer, 1024);
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

```
