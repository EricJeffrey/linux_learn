#if !defined(MY_BUF_IO)
#define MY_BUF_IO

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 8192
#define IO_ERROR
#define my_buf_io_log(A)

#if !defined(my_buf_io_log)
#define my_buf_io_log(A)                    \
    {                                       \
        write(STDOUT_FILENO, "...", 3);     \
        write(STDOUT_FILENO, A, sizeof(A)); \
        write(STDOUT_FILENO, "...\n", 4);   \
    }
#endif // my_buf_io_log

// 两个整数的最小值
int imin(int a, int b) {
    return a <= b ? a : b;
}

struct my_buf_io {
    int read_fd, write_fd;
    char read_buf[BUF_SIZE], write_buf[BUF_SIZE];
    int read_pos, write_pos;
    int read_size, write_size;
    char log_on;
};

// 初始化 bufio 内部的输入、输出缓冲区
void buf_io_init(my_buf_io *bufio, int read_fd, int write_fd) {
    memset(bufio->read_buf, 0, sizeof(bufio->read_buf));
    memset(bufio->write_buf, 0, sizeof(bufio->write_buf));
    bufio->read_size = bufio->write_size = 0;
    bufio->read_pos = bufio->write_pos = 0;
    bufio->read_fd = read_fd;
    bufio->write_fd = write_fd;
    bufio->log_on = 0;
}

// 清空 bufio 内部的输入缓冲区并再次读入数据，若出错，返回-1，若EOF，返回0，否则返回读取的字节数
int buf_io_fillread(my_buf_io *bufio) {
    my_buf_io_log("buf io fill read");
    memset(bufio->read_buf, 0, sizeof(bufio->read_buf));
    bufio->read_size = bufio->read_pos = 0;
    int ret = 0;
    while (1) {
        ret = read(bufio->read_fd, bufio->read_buf, BUF_SIZE);
        if (ret != -1 || errno != EINTR) {
            bufio->read_size = ret > 0 ? ret : 0;
            break;
        }
    }
    return ret;
}

// 读取最多 n 个字节数据到 buf 中，返回：实际读取的字节数 错误-1 0表示文件尾EOF
int buf_io_readn(my_buf_io *bufio, char *buf, int n) {
    my_buf_io_log("buf io readn");
    if (n <= 0)
        return 0;
    if (bufio->read_size >= n) {
        my_buf_io_log("bufio-readsize >= n");
        memcpy(buf, bufio->read_buf + bufio->read_pos, n);
        bufio->read_size -= n;
        bufio->read_pos += n;
        return n;
    } else {
        my_buf_io_log("bufio-readsize < n");
        int left_n = n, buf_pos = 0, read_cnt = 0;
        char eof = 0;
        while (left_n > 0) {
            int toread_n = imin(left_n, bufio->read_size);
            memcpy(buf + buf_pos, bufio->read_buf + bufio->read_pos, toread_n);
            left_n -= toread_n;
            buf_pos += toread_n;
            read_cnt += toread_n;
            if (eof || left_n == 0) {
                bufio->read_pos += toread_n;
                bufio->read_size -= toread_n;
                break;
            }
            // 填充 bufio 读缓冲区
            int ret = buf_io_fillread(bufio);
            if (ret == -1) {
                return -1;
            } else if (ret == 0) {
                eof = 1;
            }
        }
        if (left_n > 0) {
            int toread_n = imin(left_n, bufio->read_size);
            memcpy(buf + buf_pos, bufio->read_buf + bufio->read_pos, toread_n);
        }

        return read_cnt;
    }
}

// 读取以 \r\n 结尾的一行字符串，最多 line_len 个字符，到 line 中，返回： 读取的字节数，含回车符 错误-1 文件尾0
int buf_io_readline(my_buf_io *bufio, char *line, int line_len) {
    int read_cnt = 0;
    char in_r = 0;
    for (int i = 0, j = 0; i < line_len; i++, j++) {
        if (bufio->read_size <= 0) {
            my_buf_io_log("bufio readsize <= 0");
            int ret = buf_io_fillread(bufio);
            if (ret > 0) {
                j = 0;
                line[i] = bufio->read_buf[bufio->read_pos];
                bufio->read_pos++;
                bufio->read_size--;
                read_cnt++;
            } else if (ret == 0) {
                // eof
                break;
            } else {
                // error
                return -1;
            }
        } else {
            line[i] = bufio->read_buf[bufio->read_pos];
            bufio->read_pos++;
            bufio->read_size--;
            read_cnt++;
        }
        if (in_r) {
            if (line[i] == '\n') {
                my_buf_io_log("\\r\\n meet");
                break;
            } else {
                my_buf_io_log("\\r no \\n meet");
                in_r = 0;
            }
        }
        if (line[i] == '\r') {
            my_buf_io_log("\\r meet");
            in_r = 1;
        }
    }
    return read_cnt;
}

// 测试编写的缓冲IO函数
void test() {
    my_buf_io bufio;
    int fd = open("./foo.txt", O_RDWR);
    buf_io_init(&bufio, fd, STDOUT_FILENO);
    const int maxn = 1024;
    char buf[maxn] = {};
    int x;
    while (1) {
        write(STDOUT_FILENO, "input x: ", 9);
        char tmp[4] = {};
        read(STDIN_FILENO, tmp, sizeof(tmp));
        x = atoi(tmp);
        if (x == 0) {
            break;
        }
        memset(buf, 0, sizeof(buf));
        int ret;
        // ret = buf_io_readn(&bufio, buf, maxn - x);
        ret = buf_io_readline(&bufio, buf, maxn - x);
        char msg[maxn + 15] = {};
        int len = sprintf(msg, "return: %d\n%s\n", ret, buf);
        write(STDOUT_FILENO, msg, len);
    }
    close(fd);
}

#endif // MY_BUF_IO

int main(int argc, char const *argv[]) {
    test();
    return 0;
}
