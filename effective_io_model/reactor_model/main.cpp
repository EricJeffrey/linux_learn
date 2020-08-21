
#include "Reactor.h"
#include "ThreadPool.h"

void work() {
    int listenSd = Reactor::createServerSocket(2333, "0.0.0.0");
    if (listenSd == -1)
        loggerInstance()->sysError(errno, "create server socket");
    PtrPoller pollerPtr = make_shared<Poller>(Poller());
    Reactor reactor(pollerPtr, make_shared<DefaultEventHandler>(listenSd));
    try {
        reactor.start();
    } catch (const std::exception &e) {
        loggerInstance()->error("reactor start failed");
    }
}

int main(int argc, char const *argv[]) {
    return 0;
}
