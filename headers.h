/**
 * Linux Process Namespace学习代码头文件
 * EricJeffrey 2019/7/23
 */

#include <dirent.h>
#include <errno.h>
#include <memory.h>
#include <sched.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define errExit(msg)        \
    do {                    \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    } while (0)

#define NS_SAMPLE_A
#define SIG_SAMPLE_A
#define SNOOZE_SAMPLE_A
#define SIGINT_SAMPLE_A
#define SIGNAL_SAMPLE_B
#define SIGNAL_SAMPLE_C
#define PRAC_8_8
#define SETJMP_SAMPLE_A
#define SIG_SUSPEND_SAMPLE_A
#define NS_RUNNING_PROC
#define PID_NS_LS_TEST_A
