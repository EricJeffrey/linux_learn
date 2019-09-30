/**
 * 主程序 - 脚本管理程序交互DEMO
 * 2019/9/30
 * EricJeffrey
*/
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using namespace std;

class payload {
private:
    string query_str;
    int out_fd;

public:
    payload(string qstr, int ofd) : query_str(qstr), out_fd(ofd) {}
};

class payload_pool {
private:
    queue<payload> q;
    mutex mutex_pool, mutex_q;
    condition_variable cond_var;

public:
    void add(payload load) {
        mutex_q.lock();
        q.push(load);
        mutex_q.unlock();
    }
    payload fetch(){
        mutex_q.lock();
        payload ret = q.front();
        q.pop();
        mutex_q.unlock();
    }
};
