#include <pty.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <exception>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

/*
crun
    run 不需要手动添加终端
    exec

创建UNIX套接字
打开伪终端
执行crun，设置UNIX套接字路径
将master通过套接字发送给crun
设置为raw模式
转发输入、输出
退出时 - 重置终端模式

 */

using std::string;
typedef std::pair<int, string> PairIntStr;

// return "Call to [funcName] failed: strerror(errno)"
inline string wrapErrnoMsg(const string &funcName) {
    return "Call to " + funcName + " failed: " + strerror(errno);
}

// set tty to raw mode
// throw on error
int ttySetRaw(int fd, struct termios *prevTermios) {
    using std::runtime_error;
    termios t;
    if (tcgetattr(fd, &t) == -1)
        throw runtime_error(wrapErrnoMsg("tcgetattr"));
    if (prevTermios != nullptr)
        *prevTermios = t;
    t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
    t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | PARMRK);
    t.c_oflag &= ~(OPOST);
    t.c_cc[VMIN] = 1;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSAFLUSH, &t) == -1)
        throw runtime_error(wrapErrnoMsg("tcsetattr"));
    return 0;
}

// open pseudo terminal master, return fd of master and name of slave
// throw on error
PairIntStr ptyMasterOpen() {
    using std::make_pair;
    using std::runtime_error;

    int masterFd, savedErrno;
    masterFd = savedErrno = 0;
    masterFd = posix_openpt(O_RDWR | O_NOCTTY);

    auto saveAndClose = [&savedErrno, &masterFd]() {
        savedErrno = errno;
        close(masterFd);
        errno = savedErrno;
    };
    if (masterFd == -1)
        throw runtime_error(wrapErrnoMsg("posix_openpt"));
    if (grantpt(masterFd) == -1) {
        saveAndClose();
        throw runtime_error(wrapErrnoMsg("grantpt"));
    }
    if (unlockpt(masterFd) == -1) {
        saveAndClose();
        throw runtime_error(wrapErrnoMsg("unlockpt"));
    }
    char *p = ptsname(masterFd);
    if (p == nullptr) {
        saveAndClose();
        throw runtime_error(wrapErrnoMsg("ptsname"));
    }
    return make_pair(masterFd, string(p));
}

int ptyMasterOpen(char *slaveName, size_t snLen) {
    int masterFd, savedErrno;
    char *p;
    masterFd = posix_openpt(O_RDWR | O_NOCTTY);
    if (masterFd == -1)
        return -1;
    if (grantpt(masterFd) == -1) {
        savedErrno = errno;
        close(masterFd);
        errno = savedErrno;
        return -1;
    }
    if (unlockpt(masterFd) == -1) {
        savedErrno = errno;
        close(masterFd);
        errno = savedErrno;
        return -1;
    }
    p = ptsname(masterFd);
    if (p == NULL) {
        savedErrno = errno;
        close(masterFd);
        errno = savedErrno;
        return -1;
    }
    if (strlen(p) < snLen)
        strncpy(slaveName, p, snLen);
    else {
        close(masterFd);
        errno = EOVERFLOW;
        return -1;
    }
    return masterFd;
}

void errExit(const char *msg) {
    perror(msg);
    exit(1);
}

struct termios ttyOrig;
void ttyRest() {
    if (tcsetattr(STDIN_FILENO, TCSANOW, &ttyOrig) == -1)
        errExit("tcsetattr");
}

void work() {
    const int buf_size = 1024;
    const int max_sname = 1000;
    if (tcgetattr(STDIN_FILENO, &ttyOrig) == -1)
        errExit("tcgetattr");
    winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0)
        errExit("ioctl-TIOCGWINSZ");
    int masterFd;
    char slaveName[max_sname];
    pid_t childPid = forkpty(&masterFd, slaveName, &ttyOrig, &ws);
    if (childPid == -1)
        errExit("forkPty");
    if (childPid == 0) {
        char *shell = getenv("SHELL");
        const char *shellx = (shell == NULL || (*shell) == '\0') ? "/bin/sh" : shell;
        execlp(shellx, shellx, NULL);
        errExit("execlp");
    }
    // parent
    int scriptFd = open("./ttypescript", O_RDWR | O_CREAT | O_TRUNC | S_IRUSR | S_IWUSR | S_IRGRP |
                                             S_IWGRP | S_IROTH | S_IWOTH);
    if (scriptFd == -1)
        errExit("open");
    ttySetRaw(STDIN_FILENO, &ttyOrig);
    if (atexit(ttyRest) != 0)
        errExit("atexit");

    while (true) {
        fd_set infds;
        FD_ZERO(&infds);
        FD_SET(STDIN_FILENO, &infds);
        FD_SET(masterFd, &infds);
        if (select(masterFd + 1, &infds, NULL, NULL, NULL) == -1)
            errExit("select");
        if (FD_ISSET(STDIN_FILENO, &infds)) {
            char buf[buf_size];
            int numread = read(STDIN_FILENO, buf, buf_size);
            if (numread <= 0)
                exit(0);
            if (write(masterFd, buf, numread) != numread)
                fprintf(stderr, "partial/failed write (masterFd)\n");
        }
        if (FD_ISSET(masterFd, &infds)) {
            char buf[buf_size];
            int numread = read(masterFd, buf, buf_size);
            if (numread <= 0)
                exit(0);

            int ret = write(STDOUT_FILENO, buf, numread);
            if (ret < 0)
                errExit("write");
            else if (ret != numread)
                fprintf(stderr, "partial/failed write (STDOUT_FILENO)\n");
            ret = write(scriptFd, buf, numread);
            if (ret < 0)
                errExit("write");
            else if (ret != numread)
                fprintf(stderr, "partial/failed write (scriptFd)\n");
        }
    }
}

int main() {
    try {
        int scriptFd = open("./ttypescript",
                            O_RDWR | O_CREAT | O_TRUNC | S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH);
        sleep(1);
        close(scriptFd);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}
