/**
 * Linux 命名空间namespaces 学习代码
 * 每一小部分代码都包含在 [ifndef] 语句
 * 头文件 [headers.h] 包含了必须的头文件及宏
 * EricJeffrey 2019/7/26
 */

#include "headers.h"

// $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$
#if !defined(NS_SAMPLE_A)
#define NS_SAMPLE_A

// child process
static int childFunc(void *arg) {
    struct utsname uts;

    if (sethostname((char *)arg, strlen((char *)arg)) == -1)
        errExit("sethostname");

    if (uname(&uts) == -1)
        errExit("uname");

    printf("uts.nodename in child: %s\n", uts.nodename);

    sleep(20);
    return 0;
}
/**
 * create a child process that executes in a separate UTS namespace.  The child
 * changes the hostname in its UTS namespace.  Both parent and child
 * then display the system hostname, making it possible to see that the
 * hostname differs in the UTS namespaces of the parent and child
 */
int work(int argc, char const *argv[]) {
    char *stack;
    char *stackTop;
    pid_t pid;
    struct utsname uts;
    const int STACK_SIZE = 1024 * 1024;
    stack = (char *)malloc(STACK_SIZE);
    if (stack == NULL)
        errExit("malloc");
    stackTop = stack + STACK_SIZE;

    pid = clone(childFunc, stackTop, CLONE_NEWUTS | SIGCHLD, (void *)argv[1]);
    if (pid == -1)
        errExit("clone");
    printf("clone() returned %ld\n", (long)pid);

    sleep(1);

    if (uname(&uts) == -1)
        errExit("uname");
    printf("uts.nodename in parent: %s\n", uts.nodename);

    if (waitpid(pid, NULL, 0) == -1)
        errExit("waitpid");
    printf("child has terminated\n");

    exit(EXIT_SUCCESS);

    return 0;
}

#endif // NS_SAMPLE_A

#if !defined(NS_RUNNING_PROC)
#define NS_RUNNING_PROC

// 在子进程中打印子进程的PID及其父进程的PID
int child_func(void *arg) {
    printf("child pid in child process (pid): %d\n", getpid());
    printf("parent pid in child process (ppid): %d\n", getppid());
    exit(0);
}
// 子进程设置新的PID命名空间，并分别在子进程与父进程中打印两个进程的PID
void work() {
    const int STACK_SIZE = 1024 * 1024;
    char *stack = (char *)malloc(STACK_SIZE);
    char *stack_top = stack + STACK_SIZE;
    printf("clone child...\n");
    pid_t pid = clone(child_func, stack_top, CLONE_NEWPID | CLONE_NEWUTS, NULL);
    sleep(2);
    printf("child pid in parent process (pid): %ld\n", (long)pid);
    printf("parent pid in parent process (ppid): %ld\n", (long)getpid());
    exit(0);
}

#endif // NS_RUNNING_PROC

#if !defined(PID_NS_LS_TEST_A)
#define PID_NS_LS_TEST_A

int child_func(void *arg) {
    printf("child: run ps -ef:\n");
    sleep(1);
    mount("proc", "/proc", "proc", 0, NULL);
    sleep(1);
    execl("/bin/ps", "/bin/ps", "-ef", (char *)NULL);
}
//clone a child in NEW PID and MOUNT namespace then run ps -ef
void work() {
    pid_t pid;
    printf("parent: run ps -ef:\n");
    if ((pid = fork()) == 0) {
        execl("/bin/ps", "/bin/ps", "-ef", (char *)NULL);
    }
    sleep(1);

    const int STACK_SIZE = 1024 * 1024;
    char *stack = (char *)malloc(STACK_SIZE);
    char *stack_top = stack + STACK_SIZE;
    pid = clone(child_func, stack_top, CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, NULL);
    printf("parent: child %d created.\n", pid);
    pid = waitpid(pid, NULL, 0);
    printf("parent: child %d reaped.\n", pid);
}

#endif // PID_NS_LS_TEST_A

int main(int argc, char const *argv[]) {
    printf("hello world!\n"); // only one process
    fork();
    printf("hello world2!\n"); // tow processes now
    fork();
    printf("hello world3!\n"); // four processes now
    // sleep(2);
    return 0;
}
