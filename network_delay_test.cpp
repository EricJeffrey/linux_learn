#include <chrono>
#include <iostream>

#include <arpa/inet.h>
#include <cctype>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int port = 8000;

void showTime(const char *msg)
{
    auto now = chrono::steady_clock::now();
    cerr << msg << ": " << std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() << endl;
}

void checkThrow(int v, const char *what)
{
    if (v == -1)
        throw runtime_error(what);
}

void server()
{
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    checkThrow(sd, "socket");
    int v = 1;
    checkThrow(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)), "setsockopt");
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    checkThrow(bind(sd, (sockaddr *)&addr, sizeof(addr)), "bind");
    checkThrow(listen(sd, 1024), "listen");
    while (true)
    {
        sockaddr_in clientAddr;
        memset(&clientAddr, 0, sizeof(clientAddr));
        socklen_t len;
        memset(&len, 0, sizeof(len));
        int clientSd = accept(sd, (sockaddr *)&clientAddr, &len);
        checkThrow(clientSd, "accept");
        for (size_t i = 0; i < 10; i++)
        {
            char buf[1024] = {};
            int tmp = read(clientSd, buf, 1023);
            checkThrow(tmp, "read");
            showTime("server receive");
            tmp = write(clientSd, buf, 1023);
            checkThrow(tmp, "write");
            showTime("server sent");
        }
        close(clientSd);
        break;
    }
    close(sd);
}

void client()
{
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    checkThrow(sd, "socket");
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    checkThrow(inet_pton(AF_INET, "192.168.31.2", &addr.sin_addr), "inetpton");
    checkThrow(connect(sd, (sockaddr *)&addr, sizeof(addr)), "connect");
    for (size_t i = 0; i < 10; i++)
    {
        char buf[1024] = "hello world this is just a test msg\n"
                         "int sd = socket(AF_INET, SOCK_STREAM, 0);\n"
                         "checkThrow(sd, \"socket\");\n"
                         "int v = 1;\n"
                         "checkThrow(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)), \"setsockopt\");\n"
                         "sockaddr_in addr;\n"
                         "memset(&addr, 0, sizeof(addr));\n"
                         "addr.sin_addr.s_addr = INADDR_ANY;\n"
                         "addr.sin_family = AF_INET;\n"
                         "addr.sin_port = htons(port);\n"
                         "checkThrow(bind(sd, (sockaddr *)&addr, sizeof(addr)), \"bind\");\n"
                         "checkThrow(listen(sd, 1024), \"listen\");\n"
                         "while (true)";
        int tmp = write(sd, buf, sizeof(buf));
        checkThrow(tmp, "write");
        showTime("client send");
        tmp = read(sd, buf, 1023);
        checkThrow(tmp, "read");
        showTime("client receive");
    }
    close(sd);
}

int main(int argc, char const *argv[])
{
    if (argc != 2)
    {
        cout << argv[0] << " client|server" << endl;
        return 0;
    }
    if (argv[1] == string("client"))
    {
        client();
    }
    else if (argv[1] == string("server"))
    {
        server();
    }
    return 0;
}
