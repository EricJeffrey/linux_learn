/**
 * 线程池功能：提交任务
 * 从标准输入读取数据，每次提交一个任务 
 * 
 * 数据结构
 *      线程 - 执行任务
 *      可用线程数 - 
 *      条件变量 - 通知线程池任务到达
 *      互斥量 - 线程池任务专用
 *      互斥量 - 更新可用线程数
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define LOGGER(A) fprintf(stderr, "%s\n", A)

typedef void (*runnable)(int);

struct exeunit {
    runnable run;
    int arg;
};

struct threadpool {
    int avathrcnt;
    pthread_t thrs;
    pthread_mutex_t cntmutex, taskmutex;
    pthread_cond_t taskcond;
    exeunit unit;
};

void *thrfunc(void *arg) {
    while (1) {
        LOGGER("thread: lock taskmutex");

        threadpool *pool = (threadpool *)arg;
        pthread_mutex_lock(&(pool->taskmutex));
        while (pool->unit.run == NULL)
            pthread_cond_wait(&(pool->taskcond), &(pool->taskmutex));

        LOGGER("thread: wake up and lock cntmutex");

        pthread_mutex_lock(&(pool->cntmutex));
        pool->avathrcnt -= 1;
        pthread_mutex_unlock(&(pool->cntmutex));

        LOGGER("thread: unlock cntmutex and running job");

        pool->unit.run(pool->unit.arg);

        LOGGER("thread: job over, unlock taskmutex and lock cntmutex");

        pthread_mutex_unlock(&(pool->taskmutex));

        pthread_mutex_lock(&(pool->cntmutex));
        pool->avathrcnt += 1;
        pool->unit.run = NULL;
        pool->unit.arg = 1;
        pthread_mutex_unlock(&(pool->cntmutex));

        LOGGER("thread: unlock cntmutex");
    }
}

void init(threadpool *poll) {
    poll->unit.run = NULL;
    poll->unit.arg = 1;
    poll->avathrcnt = 1;
    pthread_mutex_init(&poll->cntmutex, NULL);
    pthread_mutex_init(&poll->taskmutex, NULL);
    pthread_cond_init(&poll->taskcond, NULL);
    pthread_create(&poll->thrs, NULL, thrfunc, (void *)poll);
}

// -------------------------------------------------------

void child_job(int x) {
    LOGGER("job: running job, sleep now");
    sleep(x);
    printf("job: I have slept for a while\n");
    LOGGER("job: over");
}

void commit_task(threadpool *pool, int sleep_time) {
    pthread_mutex_lock(&pool->taskmutex);
    pool->unit.run = child_job;
    pool->unit.arg = sleep_time;
    pthread_mutex_unlock(&pool->taskmutex);

    pthread_cond_signal(&(pool->taskcond));
}

int main() {
    threadpool pool;
    init(&pool);
    int x;
    while (true) {
        printf("sleep time:\n");
        scanf("%d", &x);
        if (x == -1)
            break;
        commit_task(&pool, x);
    }
}
