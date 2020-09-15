#include <cstring>

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include <netlink/netlink.h>
#include <netlink/cache.h>
#include <netlink/route/link.h>
#include <netlink/socket.h>

#include <errno.h>

void errExit(const char *msg, int errCode = 0) {
    printf("Call to ");
    printf(msg);
    printf("failed");
    if (errCode != 0)
        printf(": %s", nl_geterror(errCode));
    printf("\n");
    exit(1);
}

void addVeth() {
    nl_sock *sock = nl_socket_alloc();
    if (sock == nullptr)
        errExit("nl_socket_alloc");
    rtnl_link *link = rtnl_link_alloc();
    if (link == nullptr)
        errExit("rtnl_link_alloc");
    //
}

void listLinks() {
    nl_sock *sock = nl_socket_alloc();
    if (sock == nullptr) {
        printf("Call to nl_socket_alloc failed\n");
        return;
    }
    int ret = 0;
    ret = nl_connect(sock, NETLINK_ROUTE);
    if (ret < 0) {
        printf("Call to nl_connect failed: %s\n", nl_geterror(ret));
        return;
    }
    nl_cache *cache;
    ret = rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache);
    if (ret < 0) {
        printf("Call to rtnl_link_alloc_cache failed: %s\n", nl_geterror(ret));
        return;
    }
    // rtnl_link *link = rtnl_link_get(cache, 0);
    // if (link == nullptr) {
    //     perror("Call to rtnl_link_get failed");
    //     return;
    // }
    const int buf_size = 1024;
    char buf[buf_size] = {};
    for (size_t i = 0; i < 10000; i++) {
        memset(buf, 0, buf_size);
        rtnl_link *link = rtnl_link_get(cache, i);
        if (link == nullptr) {
            printf("Call to rtnl_link_get failed");
            return;
        }
        char *retPtr = rtnl_link_i2name(cache, 0, buf, buf_size);
        if (retPtr == nullptr) {
            // printf("%ld: null\n", i);
        } else {
            printf("%ld: %s\n", i, buf);
        }
    }

    // rtnl_link_put(link);
    nl_cache_put(cache);
    nl_socket_free(sock);
}

// void addVlan() {
//     nl_sock *sock = nl_socket_alloc();
//     rtnl_link *link;
//     int master_index;
//     nl_cache *cache;
// }

int main(int argc, char const *argv[]) {
    listLinks();
    return 0;
}
