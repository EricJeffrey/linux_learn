/**
 * 条件变量example
 * 2019/9/29
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

const int maxn_thread = 6;
int varcond = 2;
pthread_mutex_t mutex;
pthread_t thrs[maxn_thread];
pthread_cond_t conds[maxn_thread];

void *threadWork(void *arg) {
    int x = *(int *)arg;
    free(arg);
    for (int i = 0; i < 2; i++) {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&(conds[x]), &mutex);
        printf("\nThread %d Wake Up\n", x);
        varcond -= 10;
        pthread_mutex_unlock(&mutex);
        printf("Thread %d Updated varcond...\n", x);
    }
    printf("Thread %d Exit...\n", x);
}

void init() {
    for (int i = 0; i < maxn_thread; i++) {
        pthread_cond_init(&(conds[i]), NULL);
        pthread_mutex_init(&mutex, NULL);
        int *x = (int *)malloc(sizeof(int));
        *x = i;
        pthread_create(&(thrs[i]), NULL, threadWork, (void *)x);
    }
}
void destroy() {
    for (int i = 0; i < maxn_thread; i++)
        pthread_cond_destroy(&(conds[i]));
    pthread_mutex_destroy(&mutex);
}

void updateState() {
    pthread_mutex_lock(&mutex);
    varcond += 10;
    pthread_mutex_unlock(&mutex);
}

int main(int argc, char const *argv[]) {
    init();
    int x;
    while (1) {
        printf("which to wake, -1 to exit:\n");
        scanf("%d", &x);
        if (x == -1)
            break;
        if (x >= 0 && x < 6) {
            updateState();
            pthread_cond_broadcast(&(conds[x]));
        }
    }
    for (int i = 0; i < maxn_thread; i++) {
        pthread_cancel(thrs[i]);
    }
    destroy();
    return 0;
}
