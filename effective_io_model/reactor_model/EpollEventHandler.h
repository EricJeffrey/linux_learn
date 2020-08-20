#if !defined(EPOLL_EVENT_HANDLER_H)
#define EPOLL_EVENT_HANDLER_H

#include "Logger.h"
#include "Poller.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <stdexcept>
#include <exception>

using std::runtime_error;

class EpollEventHandler {
private:
public:
    EpollEventHandler() {}
    ~EpollEventHandler() {}
    virtual int handleEvent(int fd, int events, PtrPoller poller) {}
};

class DefaultServerEvHandler : EpollEventHandler {
private:
    int listenSd;

public:
    DefaultServerEvHandler(int listenSd) : listenSd(listenSd) {}
    ~DefaultServerEvHandler() {}

    int handleEvent(int fd, int events, PtrPoller poller) override {
        if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
            loggerInstance()->error(
                {(events & EPOLLERR ? "EPOLLERR" : "EPOLL(RD)HUP"), "on sock:", to_string(fd)});
            close(fd);
            if (fd == listenSd)
                throw runtime_error("Unexpected HUP/ERR on listening socket!");
            else {
                if (poller->epollDelete(fd) == -1)
                    throw runtime_error("call to epollDelete failed");
            }
        } else if (fd == listenSd) {
            handleAccept(fd);
        } else {
            switch (events) {
                case EPOLLIN:
                    handleRead(fd);
                    break;
                case EPOLLOUT:
                    handleWrite(fd);
                    break;
                default:
                    loggerInstance()->warn(
                        {"unsupported event:", to_string(events), ", will be ignored"});
                    break;
            }
        }
    }

    virtual int handleAccept(int fd) {}

    virtual int handleRead(int fd) {}

    virtual int handleWrite(int fd) {}
};

#endif // EPOLL_EVENT_HANDLER_H
