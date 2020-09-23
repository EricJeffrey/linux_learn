
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

using std::cerr;
using std::cin;
using std::cout;
using std::endl;
using std::runtime_error;
using std::string;
using std::thread;

// throw runtime_error("call to [funcName]: strerror(errno)")
void errWrapper(const char *funcName) {
    throw std::runtime_error(string("Call to ") + funcName + " failed: " + strerror(errno));
}

int readFdFromSock(int unixSd) {
    const size_t bufSize = 1024;
    const size_t iovLen = 1;
    iovec iov[iovLen];
    char buf[bufSize];
    msghdr msg;

    memset(iov, 0, sizeof(iov));
    memset(buf, 0, sizeof(buf));
    memset(&msg, 0, sizeof(msg));

    msg.msg_name = nullptr;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = iovLen;

    ssize_t numRecv = recvmsg(unixSd, &msg, 0);
    if (numRecv == -1)
        errWrapper("recvmsg");
    else if (numRecv == 0)
        throw runtime_error("recvmsg returned 0");
    cmsghdr *cmsg;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            return *reinterpret_cast<int *>(CMSG_DATA(cmsg));
        }
    }
    throw runtime_error("no SCM_RIGHTS data");
}

void crun_console_test() {
    /*
    looks like we need to send tty to crun/runc using af_unix (instead of recv)
     */
    const string sockPath("/home/eric/coding/tmp/alpine_bundle/crun_console_test_sock");
    const string containerName("test_alpine");
    int sd = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);
    if (bind(sd, (sockaddr *)&addr, sizeof(addr)) == -1)
        errWrapper("bind");
    pid_t childPid = fork();
    if (childPid == -1)
        errWrapper("fork");
    else if (childPid == 0) {
        // child
        sleep(1);
        close(sd);
        int devNullFd = open("/dev/null", O_RDWR);
        if (devNullFd == -1)
            errWrapper("open /dev/null");
        if (dup2(devNullFd, STDIN_FILENO) == -1)
            errWrapper("dup2 stdin");
        int outFd = open("./out.log", O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IROTH);
        if (outFd == -1)
            errWrapper("open out.log");
        if (dup2(outFd, STDOUT_FILENO) == -1)
            errWrapper("dup2 stdout");
        if (dup2(outFd, STDERR_FILENO) == -1)
            errWrapper("dup2 stderr");
        execlp("/usr/bin/crun", "crun", "run", "-d", "--console-socket", sockPath.c_str(),
               containerName.c_str(), nullptr);
        errWrapper("execlp");
    } else {
        // parent
        if (listen(sd, 1024) == -1)
            errWrapper("listen");
        int consoleFd = -1;
        while (true) {
            sockaddr_in addr;
            socklen_t len;
            int csd = accept(sd, (sockaddr *)&addr, &len);
            if (csd == -1)
                errWrapper("accept");
            consoleFd = readFdFromSock(csd);
            close(csd);
            break;
        }
        cerr << "console fd: " << consoleFd << endl;
        thread(
            [](int fd) {
                while (true) {
                    char ch;
                    if (read(fd, &ch, sizeof(char)) == -1)
                        errWrapper("read");
                    cerr << ch;
                }
            },
            consoleFd)
            .detach();
        while (cin) {
            char ch;
            cin >> ch;
            if (write(consoleFd, &ch, sizeof(char)) == -1)
                errWrapper("write");
        }
        close(sd);
    }
}

int main(int argc, char *argv[]) { crun_console_test(); }
