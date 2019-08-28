#include "conc_net_thr_header.h"

void *thread_work(void *vargp);

char **ptr;

int main(int argc, char const *argv[]) {
    int i;
    pthread_t tid;
    const int n = 2;
    char *msg[n] = {"Hello World Eric", "Hello World Jeffrey"};

    ptr = msg;
    for (int i = 0; i < n; i++) {
        int ret = pthread_create(&tid, NULL, thread_work, (void *)i);
        err_check_pthread(ret, "pthread create");
    }

    // 主线程等待所有子线程返回
    pthread_exit(NULL);
    return 0;
}

void *thread_work(void *vargp) {
    int myid = (long)vargp;
    static int cnt = 0;
    printf("[%d]: %s (cnt=%d)\n", myid, ptr[myid], ++cnt);
    return NULL;
}
