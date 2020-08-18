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

typedef function<int(Poller *, int)> HandlerType;
typedef pair<vector<epoll_event>, int> PairEpollEv2Int;

/* 
class handler {
    int handleEvent();

    int handleReadEvent();
    int handleWriteEvent();
    ...
}
 */

class Poller {
private:
    int listenSd, epFd;
    set<int> validConnSockSet;
    int connSockCount;
    HandlerType reader2, writer2, acceptor2;

    inline epoll_event tempEpollEvent(int sd, int events = 0) noexcept {
        epoll_event tmp;
        tmp.data.fd = sd, tmp.events = events;
        return tmp;
    }

    int epollAdd(int sd, int events, bool isConnSock = true) noexcept {
        epoll_event tmpEvent = tempEpollEvent(sd, events);
        int ret = epoll_ctl(epFd, EPOLL_CTL_ADD, sd, &tmpEvent);
        if (ret == -1)
            loggerInstance()->sysError(errno, "call to epoll_ctl failed");
        if (isConnSock)
            validConnSockSet.insert(sd);
        return ret;
    }
    // return a shared_ptr point to returned events, return nullptr on error
    PairEpollEv2Int epollWait(int timeOutMs = -1) noexcept {
        using std::make_pair;
        const int EVENT_SIZE = connSockCount + 1;
        vector<epoll_event> eventList(EVENT_SIZE);
        int ret = epoll_wait(epFd, eventList.data(), EVENT_SIZE, timeOutMs);
        if (ret == -1)
            loggerInstance()->sysError(errno, "call to epoll_wait failed");
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
    // sd should not be listenSd, -1 on error
    int epollDelete(int sd) noexcept {
        epoll_event tmpEvent = tempEpollEvent(sd);
        int ret = epoll_ctl(epFd, EPOLL_CTL_DEL, sd, &tmpEvent);
        if (ret == -1)
            loggerInstance()->sysError(errno, "call  to epoll_ctl failed");
        validConnSockSet.erase(sd);
        return ret;
    }

    // todo let EventHandler handle these job
    void dispatchEvent(const vector<epoll_event> &eventList) {
        for (auto &&ev : eventList) {
            const int sd = ev.data.fd;
            if (ev.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                loggerInstance()->error({(ev.events & EPOLLERR ? "EPOLLERR" : "EPOLL(RD)HUP"),
                                         "on sock:", to_string(sd)});
                close(sd);
                if (sd == listenSd)
                    throw runtime_error("Unexpected HUP/ERR on listening socket!");
                else {
                    if (epollDelete(sd) == -1)
                        throw runtime_error("call to epollDelete failed");
                }
            } else if (sd == listenSd) {
                acceptor2(this, sd);
            } else {
                switch (ev.events) {
                    case EPOLLIN:
                        reader2(this, sd);
                        break;
                    case EPOLLOUT:
                        writer2(this, sd);
                        break;
                    default:
                        loggerInstance()->warn(
                            {"unsupported event:", to_string(ev.events), ", will be ignored"});
                        break;
                }
            }
        }
    }

public:
    // init epoll with listenSd as listening sockt, return created epFd, throw exception on error
    Poller(int listenSd) : listenSd(listenSd) {
        epFd = epoll_create1(0);
        if (epFd == -1)
            throw runtime_error("call to epoll_create1 failed");
        int ret = epollAdd(listenSd, EPOLLIN | EPOLLRDHUP | EPOLLHUP, false);
        if (ret == -1) {
            close(epFd);
            close(listenSd);
            throw runtime_error("call to epollAdd failed");
        }
    }
    ~Poller() {}
    // call epoll_wait and dispatch event
    void startPoll() {
        while (true) {
            PairEpollEv2Int ret = epollWait();
            if (ret.second == -1) {
                close(epFd);
                throw runtime_error("call to epollWait failed");
            } else {
                dispatchEvent(ret.first);
            }
        }
    }
};

#endif // POLLER_H
