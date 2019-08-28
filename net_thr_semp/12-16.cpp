#include "conc_net_thr_header.h"

void *thread_work(void *vargp);

volatile long cnt = 0;
sem_t mutex;

int main(int argc, char const *argv[]) {
    long niters;
    pthread_t tid1, tid2;
    sem_init(&mutex, 0, 1);

    niters = atoi(argv[1]);

    pthread_create(&tid1, NULL, thread_work, (void *)&niters);
    pthread_create(&tid2, NULL, thread_work, (void *)&niters);
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    if (cnt != 2 * niters) {
        printf("BOOM! cnt=%ld\n", cnt);
    } else {
        printf("OK cnt=%ld\n", cnt);
    }
    exit(0);
    return 0;
}

void *thread_work(void *vargp) {
    long i, niters = *((long *)vargp);

    for (int i = 0; i < niters; i++) {
        sem_wait(&mutex);
        cnt++;
        sem_post(&mutex);
    }
}
