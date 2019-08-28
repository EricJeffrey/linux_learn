#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#if !defined(NETWORK_HEADER)
#define NETWORK_HEADER

// 若A为-1，则输出B并退出
#define err_check_minus_1(A, B)       \
    do {                              \
        if (A == -1) {                \
            printf("error: %s\n", B); \
            exit(EXIT_FAILURE);       \
        }                             \
    } while (0)

// 线程错误检查，若A不为0，则调用perror(B)并退出
#define err_check_pthread(A, B) \
    do {                        \
        if (A != 0) {           \
            errno = A;          \
            perror(B);          \
            exit(EXIT_FAILURE); \
        }                       \
    } while (0)

// 输出错误信息A并退出
#define err_exit(A)               \
    {                             \
        printf("error: %s\n", A); \
        exit(0);                  \
    }

// 返回-1的unix错误检查
#define err_check_unix(ret, msg) \
    do {                         \
        if (ret == -1) {         \
            unix_error(msg);     \
        }                        \
    } while (0)

// 成功时返回0的posix错误检查
#define err_check_posix(ret, msg)  \
    do {                           \
        if (ret != 0) {            \
            posix_error(ret, msg); \
        }                          \
    } while (0)

// 返回错误代码的gai错误检查
#define err_check_gai(ret, msg)  \
    do {                         \
        if (ret != 0) {          \
            gai_error(ret, msg); \
        }                        \
    } while (0)

// 使用errno的unix错误处理
void unix_error(char const *msg) {
    fprintf(stderr, "unix error! %s: %s\n", msg, strerror(errno));
    exit(0);
}
// 使用返回代码code的posix错误处理
void posix_error(int code, char const *msg) {
    fprintf(stderr, "posix error! %s: %s\n", msg, strerror(code));
    exit(0);
}
// 使用返回代码code的getaddrinfo错误处理函数
void gai_error(int code, char const *msg) {
    fprintf(stderr, "gai error! %s: %s\n", msg, gai_strerror(code));
    exit(0);
}
// 一般错误处理
void app_error(char const *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(0);
}

// 监听端口 port 的连接
int open_listenfd(char *port) {
    addrinfo hints, *listp, *p;
    int listenfd, optval = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;
    getaddrinfo(NULL, port, &hints, &listp);

    for (p = listp; p; p = p->ai_next) {
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            continue;
        }
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(listenfd);
    }

    freeaddrinfo(listp);
    if (!p) {
        return -1;
    }
    if (listen(listenfd, 1024) < 0) {
        close(listenfd);
        return -1;
    }
    return listenfd;
}

// 服务器echo函数
void echo(int connfd) {
    size_t n;
    const int maxn = 1024;
    char buf[maxn];

    while ((n = read(connfd, buf, maxn)) != 0) {
        printf("server received %d bytes\n", (int)n);
        write(connfd, buf, n);
    }
}

// 检索 hostname 与 port 指向的网络地址并建立连接，返回 connect 的clientfd 或者 -1
int open_clinetfd(char *hostname, char *port) {
    int clientfd;
    addrinfo hints, *listp, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;
    int ret = getaddrinfo(hostname, port, &hints, &listp);
    if (ret != 0) {
        err_exit(gai_strerror(ret));
    }

    for (p = listp; p; p = p->ai_next) {
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            continue;
        }
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) {
            break;
        }
        close(clientfd);
    }

    freeaddrinfo(listp);
    if (!p)
        return -1;
    else
        return clientfd;
}

// 简单echo服务器客户端
void echo_client_work() {
    int clientfd;
    const int maxn = 1024;
    char host[] = "127.0.0.1", port[] = "2333", buf[maxn];

    clientfd = open_clinetfd(host, port);
    err_check_minus_1(clientfd, "open clientfd");

    while (fgets(buf, maxn, stdin) != NULL) {
        int ret = write(clientfd, buf, strlen(buf));
        err_check_minus_1(ret, "write");
        ret = read(clientfd, buf, maxn);
        err_check_minus_1(ret, "write");
        fputs(buf, stdout);
    }
    close(clientfd);
    exit(0);
}

// 生产者消费者模型
struct sbuf_t {
    int *buf;
    int n;
    int front, rear;
    // mutex-同步buf，slots-同步空槽，items-同步非空槽
    sem_t mutex, slots, items;

    // 初始化pv模型
    void init(int n) {
        buf = (int *)calloc(n, sizeof(int));
        this->n = n;
        front = rear = 0;
        sem_init(&mutex, 0, 1);
        sem_init(&slots, 0, n);
        sem_init(&items, 0, 0);
    }

    // 释放内存buf
    void destruct() {
        free(buf);
    }

    // 向产品槽中增加一个产品
    void insert(int item) {
        sem_wait(&slots);
        sem_wait(&mutex);
        buf[(++rear) % n] = item;
        sem_post(&mutex);
        sem_post(&items);
    }

    // 从槽中取出一个模型
    int remove() {
        int item;
        sem_wait(&items);
        sem_wait(&mutex);
        item = buf[(++front) % n];
        sem_post(&mutex);
        sem_post(&slots);
        return item;
    }
};

#endif // NETWORK_HEADER
