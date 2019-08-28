#include "12-29.cpp"
#include "conc_net_thr_header.h"

#define NTHREADS 4
#define SUBFIZE 16

void echo_cnt(int connfd);
void *thread_work(void *vargp);

sbuf_t sbuf;

int main(int argc, char const *argv[]) {
    int ret = 0;
    int i, listenfd, connfd;
    sockaddr_storage clientaddr;
    socklen_t clientlen;
    pthread_t tid;

    char port[] = "2333";
    listenfd = open_listenfd(port);
    err_check_unix(listenfd, "open_listenfd");

    sbuf.init(NTHREADS);
    for (int i = 0; i < NTHREADS; i++) {
        ret = pthread_create(&tid, NULL, thread_work, (void *)i);
        err_check_posix(ret, "pthread_create");
    }

    while (1) {
        clientlen = sizeof(sockaddr_storage);
        connfd = accept(listenfd, (sockaddr *)&clientaddr, &clientlen);
        err_check_unix(connfd, "accept");
        sbuf.insert(connfd);
    }

    return 0;
}

void *thread_work(void *vargp) {
    int ret = pthread_detach(pthread_self());
    err_check_posix(ret, "pthread_detach");
    while (1) {
        int connfd = sbuf.remove();
        echo_cnt(connfd);
        ret = close(connfd);
        err_check_unix(ret, "close");
    }
}
