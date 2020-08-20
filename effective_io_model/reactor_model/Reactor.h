#if !defined(REACTOR_H)
#define REACTOR_H

#include "Logger.h"
#include "Poller.h"
#include "EpollEventHandler.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory>
#include <exception>

using std::make_shared;
using std::shared_ptr;

typedef shared_ptr<Poller> PtrPoller;
typedef shared_ptr<EpollEventHandler> PtrEventHandler;

class Reactor {
private:
    PtrPoller pollerPtr;
    PtrEventHandler handlerPtr;

public:
    Reactor(PtrPoller poller, PtrEventHandler handler) {
        this->pollerPtr = poller;
        this->handlerPtr = handler;
    }
    ~Reactor() {}

    // Create a traditional server socket listen on 0.0.0.0:port
    // Call setsockopt(...SO_REUSEADDR) if setSockReuse is true
    // Throw on Error
    static int createServerSocket(int port, const string &host, bool setSockReuse = true) {
        int sd, ret;
        sd = socket(AF_INET, SOCK_STREAM, 0);
        if (sd == -1) {
            loggerInstance()->sysError(errno, "call to socket failed");
            throw runtime_error("call to socket failed");
        }
        if (setSockReuse) {
            int opt = 1;
            if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1) {
                loggerInstance()->sysError(errno, "setsockopt failed");
                throw runtime_error("call to setsockopt failed");
            }
        }
        sockaddr_in addr;
        addr.sin_family = AF_INET, addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sd, (sockaddr *)&addr, sizeof(addr)) == -1) {
            loggerInstance()->sysError(errno, "bind failed");
            throw runtime_error("call to bind failed");
        }
        if (listen(sd, 1024) == -1) {
            loggerInstance()->sysError(errno, "listen failed");
            throw runtime_error("call to listen failed");
        }
        return sd;
    }

    // Call epoll_wait to listen event
    // Throw on error
    void startEventListener() {
        while (true) {
            EvListPairIntRet ret = pollerPtr->epollWait();
            if (ret.second == -1) {
                pollerPtr->closePoller();
                throw runtime_error("call to epollWait failed");
            } else {
                for (auto &&ev : ret.first)
                    handlerPtr->handleEvent(ev.data.fd, ev.events, pollerPtr);
            }
        }
    }
};

#endif // REACTOR_H
