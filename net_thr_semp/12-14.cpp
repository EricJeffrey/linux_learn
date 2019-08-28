#include "conc_net_thr_header.h"

void echo(int connfd);
void *thread_work(void *vargp);

int main(int argc, char const *argv[]) {
    int listenfd, *connfdp;
    socklen_t clientlen;
    sockaddr_storage clientaddr;
    pthread_t tid;

    char port[] = "2333";
    listenfd = open_listenfd(port);
    err_check_minus_1(listenfd, "open listen fd");

    while (1) {
        clientlen = sizeof(sockaddr_storage);
        connfdp = (int *)malloc(sizeof(int));
        err_check_minus_1(*connfdp, "malloc");
        *connfdp = accept(listenfd, (sockaddr *)&clientaddr, &clientlen);
        err_check_minus_1(*connfdp, "accept");

        int ret = pthread_create(&tid, NULL, thread_work, connfdp);
        err_check_pthread(ret, "pthread create");
    }

    return 0;
}

void *thread_work(void *vargp) {
    int connfd = *(int *)vargp;
    int ret = pthread_detach(pthread_self());
    err_check_pthread(ret, "pthread detach");

    free(vargp);
    echo(connfd);
    ret = close(connfd);
    err_check_minus_1(ret, "close");
    return NULL;
}
