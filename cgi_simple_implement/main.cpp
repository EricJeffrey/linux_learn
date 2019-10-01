/**
 * 主程序 - 脚本管理程序交互DEMO
 * 2019/9/30
 * EricJeffrey
*/
#include <condition_variable>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <wait.h>

using namespace std;

#define LOGGER(A) fprintf(stderr, "LOGGER: %s\n", A)

#define LOGGERF(A, ...) fprintf(stderr, A, __VA_ARGS__)

struct payload {
    string query_str;
    int out_fd;
    payload(string qs, int ofd) : query_str(qs), out_fd(ofd) {}
};

class payload_pool {
private:
    queue<payload> qpayload;
    mutex mutexq;
    condition_variable cond;

public:
    payload_pool() {}
    ~payload_pool() {}

    void push(payload load) {
        LOGGER("payload pool: push");
        mutexq.lock();
        qpayload.push(load);
        cond.notify_one();
        mutexq.unlock();
        LOGGER("payload pool: pushed");
    }
    payload front() {
        LOGGER("payload pool: front");
        unique_lock<mutex> lck(mutexq);
        while (qpayload.empty())
            cond.wait(lck);
        LOGGER("payload pool: wake up");
        payload tmpload = qpayload.front();
        qpayload.pop();
        LOGGER("payload pool: got");
        return tmpload;
    }
};

// 脚本管理程序
void manageScript(payload_pool *pool) {
    LOGGER("ms: script manager start");
    while (true) {
        payload load = pool->front();
        LOGGER("ms: payload got");
        int ret = fork();
        if (ret < 0)
            LOGGER("ms: fork error");
        if (ret == 0) {
            setenv("query_str", load.query_str.c_str(), 0);
            int tmpfd = load.out_fd;
            LOGGERF("LOG: load.outfd: %d\n", tmpfd);
            int ret = dup2(load.out_fd, STDOUT_FILENO);
            if (ret == -1)
                LOGGER(strerror(errno));
            LOGGER("ms: child: setenv and dup2, execl now");
            execl("/usr/bin/python3", "python3", "test.py", NULL);
            exit(EXIT_FAILURE);
        }
        waitpid(ret, NULL, 0);
        LOGGER("ms: child: execl done");
    }
}

int main(int argc, char const *argv[]) {
    payload_pool pool;
    thread t(manageScript, &pool);
    t.detach();
    LOGGER("main: thread created");

    int fd;
    if ((fd = open("bar.txt", O_RDWR | O_TRUNC)) == -1)
        fd = open("bar.txt", O_RDWR | O_CREAT, S_IRWXG | S_IRWXU);
    while (1) {
        string s;
        cin >> s;
        pool.push(payload(s, fd));
        LOGGER("main: payload pushed");
    }

    return 0;
}
