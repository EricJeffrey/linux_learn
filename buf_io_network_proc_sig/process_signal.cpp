/**
 * Linux 进程控制 信号 学习代码
 * 每一小部分代码都包含在 [ifndef] 语句
 * 头文件 [headers.h] 包含了必须的头文件及宏
 * EricJeffrey 2019/7/23
 */

#include "headers.h"


// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(SIG_SAMPLE_A)
#define SIG_SAMPLE_A

/**
 * Sample show parent send signal SIGKILL 
 * to kill his child.
 */
int work() {
    pid_t pid;
    if ((pid = fork()) == 0) {
        printf("Child: %d will pause here.\n", pid);
        pause();
        printf("Control should never reach here!\n");
        exit(0);
    }
    sleep(5);
    kill(pid, SIGKILL);
    exit(0);
}

#endif // SIG_SAMPLE_A

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(SIGINT_SAMPLE_A)
#define SIGINT_SAMPLE_A

// signal handler
void signal_handler(int sig) {
    printf("Caught SIGINT\n");
    exit(0);
}
/**
 * Capture signal SIG_INT and use sig_handler handle it
 */
int work() {
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        printf("ERROR: %d, %s", errno, strerror(errno));
        exit(0);
    }
    pause();
}

#endif // SIGINT_SAMPLE_A

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(SNOOZE_SAMPLE_A)
#define SNOOZE_SAMPLE_A

// empty SIG_INT handler, do nothing is ok
void signal_handler(int sig) {}
// sleep for spec seconds, but can be canceld by Ctrl+C
void snooze(int sec) {
    int ret = sleep(sec);
    ret = sec - ret;
    printf("Slept %d of %d sec.\n", ret, sec);
}
// snooze for sec
int work(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    if (argc != 2) {
        printf("sleep for given seconds\nUSAGE: \n snooze sec\n");
        exit(0);
    }
    int sec = atoi(argv[1]);
    snooze(sec);
    return 0;
}

#endif // SNOOZE_SAMPLE_A

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(SIGNAL_SAMPLE_B)
#define SIGNAL_SAMPLE_B

// ok code, reap all possible children at one time
void signal_handler_ok(int sig) {
    int olderrorno = errno;
    while (waitpid(-1, NULL, 0) > 0) {
        const char s[] = "Handler reaped child\n";
        write(STDOUT_FILENO, s, strlen(s));
    }
    if (errno != ECHILD) {
        const char s[] = "waitpid error\n";
        write(STDOUT_FILENO, s, strlen(s));
        exit(0);
    }
    sleep(1);
    errno = olderrorno;
}
// buggy code
void signal_handler(int sig) {
    int olderrno = errno;
    if ((waitpid(-1, NULL, 0)) < 0) {
        const char s[] = "signal_error\n";
        write(STDOUT_FILENO, s, strlen(s));
        exit(0);
    }
    const char s[] = "Handler reaped child\n";
    write(STDOUT_FILENO, s, strlen(s));
    sleep(1);
    errno = olderrno;
}
// buggy code, parent reap child by SIGCHLD
void work() {
    int i, n;
    const int MAX_BUF = 1024 * 1024;
    char buf[MAX_BUF];
    if (signal(SIGCHLD, signal_handler_ok) == SIG_ERR) {
        const char s[] = "SIGNAL_ERROR";
        write(STDOUT_FILENO, s, strlen(s));
        exit(0);
    }
    for (int i = 0; i < 3; i++) {
        if (fork() == 0) {
            printf("Hello from child %d\n", (int)getpid());
            sleep(1);
            exit(0);
        }
    }
    if ((n == read(STDIN_FILENO, buf, sizeof(buf))) < 0) {
        const char s[] = "read error";
        write(STDOUT_FILENO, s, strlen(s));
        exit(0);
    }
    printf("Parent processing input\n");
    sleep(1);
    exit(0);
}

#endif // SIGNAL_SAMPLE_B

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(PRAC_8_8)
#define PRAC_8_8

volatile long counter = 2;
// write a long, use write
void tmp_writel(long x) {
    char s[13] = {};
    int i = 0;
    while (x > 0) {
        s[i] = x % 10 + '0';
        x = x / 10;
        i = i + 1;
    }
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char tmp = s[j];
        s[j] = s[k];
        s[k] = tmp;
    }
    write(STDOUT_FILENO, s, i);
}
// SIG_USR1 handler
void handler(int sig) {
    tmp_writel(getpid());
    sigset_t mask, prev_mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);
    tmp_writel(--counter);
    write(STDOUT_FILENO, "\n", 1);
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    _exit(0);
}
// practice from 深入理解计算机系统 8.8
void work() {
    pid_t pid;
    sigset_t mask, prev_mask;
    printf("%ld", counter);
    fflush(stdout);

    signal(SIGUSR1, handler);
    if ((pid = fork()) == 0) {
        const char s[] = "Hello from child: ";
        write(STDOUT_FILENO, s, sizeof(s));
        tmp_writel(getpid());
        write(STDOUT_FILENO, "\n", 1);
        while (1) {
        }
    }
    sleep(1);
    kill(pid, SIGUSR1);
    waitpid(-1, NULL, 0);

    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev_mask);
    printf("%ld", ++counter);
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    exit(0);
}

#endif // PRAC_8_8

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(SIGNAL_SAMPLE_C)
#define SIGNAL_SAMPLE_C

void handler(int sig) {
    printf("Got signal\n");
}
// 显式阻塞信号后，新信号是否会被丢弃？ - 第一个不会丢弃，会阻塞
void work() {
    sigset_t mask, prev;
    printf("Now send first SIGUSR1\n");
    // send signal SIGUSR1
    signal(SIGUSR1, handler);
    kill(0, SIGUSR1);
    sleep(1);
    // block all signal
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &prev);
    printf("Signal blocked\n");
    // send another signal
    printf("Now send second SIGUSR1\n");
    kill(0, SIGUSR1);
    printf("Unblock Signal\n");
    sigprocmask(SIG_SETMASK, &prev, NULL);
    sleep(1);
    printf("Now send third signal\n");
    // send third signal
    kill(0, SIGUSR1);
    printf("Finish...\n");
}

#endif // SIGNAL_SAMPLE_C

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(SETJMP_SAMPLE_A)
#define SETJMP_SAMPLE_A

jmp_buf buf;
int error1 = 0, error2 = 1;
void foo(), bar();
void foo() {
    printf("in foo\n");
    if (error1)
        longjmp(buf, 1);
    bar();
}
void bar() {
    printf("in bar\n");
    if (error2)
        longjmp(buf, 2);
}
// set jmp sample
void work() {
    switch (setjmp(buf)) {
    case 0:
        foo();
        break;
    case 1:
        printf("error1 condition in foo\n");
        break;
    case 2:
        printf("error2 condition in bar\n");
        break;
    }
    exit(0);
}

#endif // SETJMP_SAMPLE_A

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(SIG_SUSPEND_SAMPLE_A)
#define SIG_SUSPEND_SAMPLE_A

void handler(int sig) {
    printf("SIGCHLD GOT\n");
}
// use sigsuspend to hang up parent until a signal got
// use kill -s SIGKILL pid to kill process, SIGKILL cannot be ignored and blocked
void work(char *argv[]) {
    printf("now run date...\n");
    sigset_t mask;
    sigemptyset(&mask);
    signal(SIGCHLD, handler);
    if (fork() == 0) {
        sleep(2);
        execv("/bin/date", argv);
    }
    sigsuspend(&mask);
    int a, b;
    printf("child exit, do my job, input a b:\n");
    scanf("%d %d", &a, &b);
    printf("a + b = %d\n", a + b);
}

#endif // SIG_SUSPEND_SAMPLE_A

void handler(int sig) {
    printf("signal SIGUSR1 got.\n");
}

int main(int argc, char *argv[]) {
    printf("here we go...\n");
    signal(SIGUSR1, handler);

    sigset_t mask, prev;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &prev);

    pid_t pid = fork();
    if (pid == 0) {
        printf("child: I will do something.\n");
        kill(getppid(), SIGUSR1);
        printf("child: I will do some other things and exit.\n");
        exit(0);
    }

    sigsuspend(&prev);
    printf("parent: I will do something after child send SIGUSR1\n");
    sigprocmask(SIG_SETMASK, &prev, NULL);

    wait(NULL); // recycle child
    return 0;
}
