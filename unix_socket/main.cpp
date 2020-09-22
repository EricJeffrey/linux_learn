
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using std::cerr;
using std::cin;
using std::endl;
using std::runtime_error;
using std::string;
using std::thread;

/*
1. connect to
/run/libpod/socket/2a6b233aaf4ccfad82d4a851d685354b06312d8749dd150d37fca92db3a9a7a2/attach
2. monitor read on it
3. send whatever user type

podman容器，似乎通过上述AF_UNIX套接字可以向容器内的程序数据

 */

class Worker {
private:
    string sockPath;

    // throw runtime_error("call to [funcName] failed: [strerror(errno)]")
    inline void errorWrapper(const string &funcName) {
        throw runtime_error(string("Call to ") + funcName + " failed: " + string(strerror(errno)));
    }

    int connect() {
        int sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
        if (sd == -1)
            errorWrapper("socket");
        sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);
        // if (bind(sd, (sockaddr *)&addr, sizeof(sockaddr_un)) == -1)
        //     errorWrapper("bind");
        if (::connect(sd, (sockaddr *)&addr, sizeof(sockaddr_un)) == -1)
            errorWrapper("connect");
        return sd;
    }

    void readerJob(int sd) {
        const size_t bufSize = 1024;
        char buf[bufSize];
        while (true) {
            ssize_t readNum = read(sd, buf, bufSize);
            if (readNum == -1)
                errorWrapper("read");
            else {
                for (ssize_t i = 0; i < readNum; i++) {
                    if (buf[i] != 2)
                        cerr << buf[i];
                }
                cerr << endl;
            }
        }
    }

    void write2sock(int sd, char ch) {
        int writeNum = write(sd, &ch, sizeof(char));
        if (writeNum == -1)
            errorWrapper("write");
        else if (writeNum != 1 && errno == EINTR) {
            cerr << "-----interrupted by signal-----" << endl;
        }
    }

public:
    // sp: absolute unix socket path
    explicit Worker(const string &sp) : sockPath(sp) {}
    ~Worker() {}

    void start() {
        try {
            int sd = connect();
            cerr << "connected to target" << endl;
            thread(&Worker::readerJob, this, sd).detach();
            cerr << "reader started" << endl;
            while (cin) {
                char ch;
                cin >> ch;
                write2sock(sd, ch);
            }
        } catch (const std::exception &e) {
            cerr << e.what() << endl;
        }
    }
};

int main(int argc, char const *argv[]) {
    Worker("/run/libpod/socket/"
           "2a6b233aaf4ccfad82d4a851d685354b06312d8749dd150d37fca92db3a9a7a2/attach")
        .start();
    return 0;
}
