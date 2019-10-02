#include <algorithm>
#include <condition_variable>

#include <mutex>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

using namespace std;

#define LOGGER(A) fprintf(stderr, "LOG - %s\n", A)

class task {
private:
    int arg;

public:
    task(int a) : arg(a) {}
    void run() {
        LOGGER("run: exec task now, sleep");
        for (int i = 0; i < 5; i++) {
            sleep(arg);
            printf("run: %ds have elapsed...\n", arg);
        }
        LOGGER("run: exec task over");
    }
};

class thread_pool {
private:
    const int tot_thr;
    int ava_thr_cnt;

    vector<thread> threads;
    condition_variable cond_var;
    mutex mutex_cond, mutex_task;
    queue<task> q_task;

    void notify_one() {
        cond_var.notify_one();
    }

public:
    thread_pool(int tot = 16) : tot_thr(tot) {
        ava_thr_cnt = tot;
        for (int i = 0; i < tot_thr; i++) {
            threads.push_back(thread([this]() -> void {
                LOGGER("thread: init");
                while (true) {
                    unique_lock<mutex> lck(this->mutex_cond);
                    LOGGER("thread: wait for task");
                    while (this->q_task.empty())
                        this->cond_var.wait(lck);
                    if (!this->q_task.empty()) {
                        LOGGER("thread: task got, exec now");

                        this->mutex_task.lock();
                        lck.unlock();
                        // 获得任务
                        task tmptask = this->q_task.front();
                        this->q_task.pop();
                        this->ava_thr_cnt -= 1;
                        // 解锁，执行任务
                        this->mutex_task.unlock();

                        tmptask.run();

                        this->mutex_task.lock();
                        this->ava_thr_cnt += 1;
                        this->mutex_task.unlock();
                    }
                }
            }));
        }
    }
    // 提交任务，加入队列并通知线程
    int commit_task(task t) {
        LOGGER("main: commit task");
        mutex_task.lock();
        q_task.push(t);
        mutex_task.unlock();
        notify_one();
    }
};

int main(int argc, char const *argv[]) {
    const int maxn = 4;
    thread_pool pool(maxn);
    LOGGER("main: thread pool created");

    vector<int> times_to_sleep = {1, 3, 5, 7, 2, 4, 6, 8};
    for (auto v : times_to_sleep) {
        pool.commit_task(task(v));
    }
    getchar();
    // int x;
    // while (true) {
    //     printf("input time to sleep: \n");
    //     scanf("%d", &x);
    //     if (x == -1)
    //         break;
    //     LOGGER("main: commit task");
    //     pool.commit_task(task(x));
    // }
    terminate();
    return 0;
}
