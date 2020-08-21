#if !defined(THREAD_POOL_H)
#define THREAD_POOL_H

#include <queue>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <memory>
#include <stdexcept>
#include <exception>
#include <thread>

using std::condition_variable;
using std::exception;
using std::lock_guard;
using std::make_shared;
using std::mutex;
using std::queue;
using std::runtime_error;
using std::shared_ptr;
using std::thread;
using std::unique_lock;
using std::vector;

class Task {
public:
    Task() {}
    ~Task() {}
    virtual void run() = 0;
};

typedef shared_ptr<Task> PtrTask;

class ThreadPool {
private:
    const size_t MAX_THREAD_NUM = 10;
    size_t threadNum, idleThreadNum;
    mutex idleThrNumMutex;
    bool keepRunning;

    queue<PtrTask> taskQueue;
    mutex taskQueueMutex;
    // notify thread
    condition_variable taskQueueCondVar;

    inline void incIdleThrNumBy1(bool positive) {
        lock_guard<mutex> idleThrNumLock(idleThrNumMutex);
        idleThreadNum += (positive ? 1 : -1);
    }

    // Wait on taskQueueCondVar then run task
    void threadJob() {
        loggerInstance()->debug("executing threadJob");
        while (keepRunning) {
            incIdleThrNumBy1(true);
            unique_lock<mutex> uTaskLock(taskQueueMutex);
            taskQueueCondVar.wait(uTaskLock, [this]() { return !this->taskQueue.empty(); });
            incIdleThrNumBy1(false);

            PtrTask task = this->taskQueue.front();
            this->taskQueue.pop();
            uTaskLock.unlock();
            task->run();
        }
    }

    // create a new thread watiing on condition var of task queue
    void createNewThread() {
        loggerInstance()->debug("creating new thread");
        thread tmpThread = thread(&ThreadPool::threadJob, this);
        tmpThread.detach();
        ++threadNum;
    }

public:
    ThreadPool(size_t initSize = 0) {
        loggerInstance()->debug("ThreadPool Construct");
        threadNum = idleThreadNum = 0;
        keepRunning = true;
        for (size_t i = 0; i < initSize; i++)
            createNewThread();
    }
    ~ThreadPool() {}

    // Won't create if we have idle thread
    // If all thread busy, just enqueue
    void addTask(PtrTask task) {
        loggerInstance()->debug({"pool -> addTask, idleThreadNum:", to_string(idleThreadNum), "threadNum:", to_string(threadNum)});
        if (idleThreadNum == 0 && threadNum < MAX_THREAD_NUM) {
            createNewThread();
        }
        {
            lock_guard<mutex> lock(taskQueueMutex);
            taskQueue.push(task);
        }
        taskQueueCondVar.notify_one();
    }
    void stopRunning() { keepRunning = false; }
};

#endif // THREAD_POOL_H
