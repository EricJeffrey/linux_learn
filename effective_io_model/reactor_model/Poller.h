#if !defined(POLLER_H)
#define POLLER_H

#include "Logger.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <exception>
#include <stdexcept>
#include <functional>
#include <set>
#include <vector>
#include <memory>

using std::function;
using std::pair;
using std::runtime_error;
using std::set;
using std::shared_ptr;
using std::vector;

class Poller;

typedef pair<vector<epoll_event>, int> EpollEvs2IntRet;
typedef shared_ptr<Poller> PtrPoller;

class Poller {
private:
    int listenSd, epFd;
    set<int> validConnSockSet;
    int connSockCount;

    inline epoll_event tempEpollEvent(int sd, int events = 0) noexcept {
        epoll_event tmp;
        tmp.data.fd = sd, tmp.events = events;
        return tmp;
    }

public:
    // init poller using epoll_create
    Poller() {
        connSockCount = 0;
        epFd = epoll_create1(0);
        if (epFd == -1)
            throw runtime_error("call to epoll_create1 failed");
    }
    // init epoll with listenSd as listening sockt
    // return created epFd, throw exception on error
    Poller(int listenSd) : Poller() {
        this->listenSd = listenSd;
        int ret = epollAdd(listenSd, EPOLLIN | EPOLLRDHUP | EPOLLHUP, false);
        if (ret == -1) {
            close(epFd);
            close(listenSd);
            throw runtime_error("call to epollAdd failed");
        }
    }

    ~Poller() {}

    int epollAdd(int sd, int events, bool isConnSock = true) noexcept {
        epoll_event tmpEvent = tempEpollEvent(sd, events);
        int ret = epoll_ctl(epFd, EPOLL_CTL_ADD, sd, &tmpEvent);
        if (ret == -1)
            loggerInstance()->sysError(errno, "call to epoll_ctl failed");
        if (isConnSock)
            validConnSockSet.insert(sd);
        return ret;
    }

    // Return a pair contain events and int value
    // Return <{}, -2> on Interrupted, <{}, -1> on error
    EpollEvs2IntRet epollWait(int timeOutMs = -1) noexcept {
        using std::make_pair;
        const int EVENT_SIZE = connSockCount + 1;
        vector<epoll_event> eventList(EVENT_SIZE);
        int ret = epoll_wait(epFd, eventList.data(), EVENT_SIZE, timeOutMs);
        if (ret == -1) {
            const int tmpErrno = errno;
            if (tmpErrno == EINTR) {
                loggerInstance()->sysError(tmpErrno, "call to epoll_wait interrupted by signal");
                return make_pair(eventList, -2);
            }
            loggerInstance()->sysError(tmpErrno, "call to epoll_wait failed");
        }
        return make_pair(eventList, ret);
    }

    // -1 on error
    int epollModify(int sd, int events) noexcept {
        epoll_event tmpEvent = tempEpollEvent(sd, events);
        tmpEvent.events = events, tmpEvent.data.fd = sd;
        int ret = epoll_ctl(epFd, EPOLL_CTL_MOD, sd, &tmpEvent);
        if (ret == -1)
            loggerInstance()->sysError(errno, "call to epoll_ctl failed");
        return 0;
    }

    // sd should not be listen socket, -1 on error
    int epollDelete(int sd) noexcept {
        epoll_event tmpEvent = tempEpollEvent(sd);
        int ret = epoll_ctl(epFd, EPOLL_CTL_DEL, sd, &tmpEvent);
        if (ret == -1)
            loggerInstance()->sysError(errno, "call  to epoll_ctl failed");
        validConnSockSet.erase(sd);
        return ret;
    }

    int closePoller() {
        const int ret = close(epFd);
        if (ret == -1)
            loggerInstance()->sysError(errno, "close poller failed");
        return ret;
    }
};

#endif // POLLER_H
