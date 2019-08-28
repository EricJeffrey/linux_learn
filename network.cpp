#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define err_arg "incorrect argument"
#define err_exit(A)               \
    {                             \
        printf("error: %s\n", A); \
        exit(0);                  \
    }

struct NetworkLearn {
    // 将命令行参数十六进制网络地址转换为点分十进制并输出
    void hex2dotdec(int argc, char const *argv[]) {
        if (argc != 2) {
            err_exit(err_arg);
        }
        // 解析参数值
        int argval = 0;
        const char *hex = argv[1];
        int len = strlen(hex);
        if (len != 10 || hex[0] != '0' || (hex[1] != 'x' && hex[1] != 'X')) {
            err_exit(err_arg);
        }
        // 每两位计算
        int d[4];
        for (int i = 2; i < 10; i += 2) {
            char a = hex[i], b = hex[i + 1];
            a = isdigit(a) ? a - '0' : (isupper(a) ? a + 32 : a) - 'a' + 10;
            b = isdigit(b) ? b - '0' : (isupper(a) ? a + 32 : a) - 'a' + 10;
            d[i / 2 - 1] = a * 16 + b;
        }

        // 输出
        for (int i = 0; i < 4; i++) {
            printf((i != 0 ? ".%d" : "%d"), d[i]);
        }
        putchar('\n');
    }

    // 将命令行的点分十进制网络地址转换为十六进制并输出
    void dotdec2hex(int argc, char const *argv[]) {
        if (argc != 2) {
            err_exit(err_arg);
        }
        // 解析点分十进制
        const char *dec = argv[1];
        int d[4] = {};
        for (int i = 0, j = 0; dec[i] != '\0'; i++) {
            if (!(dec[i] == '.' || isdigit(dec[i])))
                err_exit(err_arg);
            if (dec[i] != '.')
                d[j] = d[j] * 10 + (dec[i] - '0');
            else
                j++;
        }
        // 输出
        printf("0x");
        for (int i = 0; i < 4; i++) {
            printf("%02x", d[i]);
        }
        putchar('\n');
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
};

struct SimpleEchoServer {

    // 简单echo服务器客户端
    void echo_client_work() {
        int clientfd;
        const int maxn = 1024;
        char host[] = "127.0.0.1", port[] = "2333", buf[maxn];

        NetworkLearn nwl;
        clientfd = nwl.open_clinetfd(host, port);
        if (clientfd == -1) {
            err_exit("open_clientfd");
        }
        while (fgets(buf, maxn, stdin) != NULL) {
            write(clientfd, buf, strlen(buf));
            read(clientfd, buf, maxn);
            fputs(buf, stdout);
        }
        close(clientfd);
        exit(0);
    }

    // 简单echo服务器服务端
    void echo_server_work() {
        int listenfd, connfd;
        socklen_t clientlen;
        sockaddr_storage clientaddr;
        const int maxn = 1024;
        char client_hostname[maxn], client_port[maxn];

        NetworkLearn nwl;
        char port[] = "2333";
        listenfd = nwl.open_listenfd(port);
        while (1) {
            clientlen = sizeof(sockaddr_storage);
            connfd = accept(listenfd, (sockaddr *)&clientaddr, &clientlen);
            getnameinfo((sockaddr *)&clientaddr, clientlen, client_hostname, maxn, client_port, maxn, 0);

            printf("Connected to (%s, %s)\n", client_hostname, client_port);
            echo(connfd);
            close(connfd);
        }
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

    // 采用IO复用技术的echo服务器
    void io_mul_echo_server() {
        int listenfd, confd;
        socklen_t clientlen;
        sockaddr_storage clientaddr;
        fd_set read_set, ready_set;

        char port[] = "2333";
        NetworkLearn nwl;
        listenfd = nwl.open_listenfd(port);

        printf("io mul server start listen on listenfd: %d...\n", listenfd);

        FD_ZERO(&read_set);
        FD_SET(STDIN_FILENO, &read_set);
        FD_SET(listenfd, &read_set);

        while (1) {
            ready_set = read_set;
            int ret = select(listenfd + 1, &ready_set, NULL, NULL, NULL);
            printf("select return %d\n", ret);
            if (FD_ISSET(STDIN_FILENO, &ready_set)) {
                char buf[1024] = {};
                fgets(buf, 1024, stdin);
                printf("%s", buf);
            }
            if (FD_ISSET(listenfd, &ready_set)) {
                clientlen = sizeof(sockaddr_storage);
                confd = accept(listenfd, (sockaddr *)&clientaddr, &clientlen);
                echo(confd);
                close(confd);
            }
        }
    }

    // 采用IO复用与事件驱动的echo服务器
    void io_mul_event_drive_server() {
        struct pool {
            int maxfd;
            fd_set read_set, ready_set;
            int nready;
        };
    }
};

int main(int argc, char const *argv[]) {
    int x;
    printf("0 - client, 1 - server, 2 - iomul server: ");
    scanf("%d", &x);
    SimpleEchoServer ses;
    switch (x) {
    case 0:
        ses.echo_client_work();
        break;
    case 1:
        ses.echo_server_work();
        break;
    case 2:
        ses.io_mul_echo_server();
        break;
    default:
        break;
    }
    return 0;
}
