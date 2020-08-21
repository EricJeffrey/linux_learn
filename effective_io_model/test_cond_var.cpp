/*

1. 验证 条件变量 condition_variable 先notify再wait，是否会起作用

-- 使用 wait(mutex, pred) 总会在 pred 满足的时候触发
-- 而 wait(mutex) 在之后没有 notify 的时候会一直等待，之前的 notify 会消失

2. 如果两个线程都采用 wait(mutex, pred) 呢

-- wait 保证了访问 pred 时上锁

*/
#include <iostream>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <condition_variable>
#include <queue>

using namespace std;

queue<int> taskQueue;
mutex taskQueueMutex;
condition_variable taskQueueCondVar;

int main() {
    thread([](int x) {
        cerr << "thread " << x << " sleeping" << endl;
        sleep(2);
        cerr << "thread " << x << " wake up" << endl;
        unique_lock<mutex> threadTaskQULock(taskQueueMutex);
        taskQueueCondVar.wait(threadTaskQULock, []() { return !taskQueue.empty(); });
        // taskQueueCondVar.wait(threadTaskQULock);
        cerr << "thread " << x << " wait returned" << endl;
        taskQueue.pop();
        cerr << "thread " << x << " task done" << endl;
        threadTaskQULock.unlock();
    }, 1).detach();
    thread([](int x) {
        cerr << "thread " << x << " sleeping" << endl;
        sleep(2);
        cerr << "thread " << x << " wake up" << endl;
        unique_lock<mutex> threadTaskQULock(taskQueueMutex);
        taskQueueCondVar.wait(threadTaskQULock, []() { return !taskQueue.empty(); });
        // taskQueueCondVar.wait(threadTaskQULock);
        cerr << "thread " << x << " wait returned" << endl;
        taskQueue.pop();
        cerr << "thread " << x << " task done" << endl;
        threadTaskQULock.unlock();
    }, 2).detach();
    cerr << "Hello World" << endl;
    {
        lock_guard<mutex> tmpTaskQLockGuard(taskQueueMutex);
        taskQueue.push(1);
        taskQueue.push(1);
    }
    taskQueueCondVar.notify_one();
    cout << "main sleeping" << endl;
    sleep(5);
    cout << "main wake up" << endl;
    exit(0);
    return 0;
}
