#include "conc_net_thr_header.h"

#if !defined(_12_29)
#define _12_29

static int byte_cnt;
static sem_t mutex;

static void init_echo_cnt(void) {
    int ret = sem_init(&mutex, 0, 1);
    err_check_unix(ret, "sem_init");
    byte_cnt = 0;
}

void echo_cnt(int connfd) {
    int n;
    const int max_line = 1024;
    char buf[max_line];
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    int ret = pthread_once(&once, init_echo_cnt);
    err_check_unix(ret, "pthread_once");

    while ((n = read(connfd, buf, max_line)) != 0) {
        ret = sem_wait(&mutex);
        err_check_posix(ret, "sem_wait");

        byte_cnt += n;
        printf("server received %d (%d total) bytes on fd %d\n", n, byte_cnt, connfd);

        ret = sem_post(&mutex);
        err_check_posix(ret, "sem_post");

        ret = write(connfd, buf, n);
        err_check_unix(ret, "write");
    }
}

#endif // _12_29
