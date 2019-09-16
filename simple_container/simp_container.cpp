#include "/home/eric/coding/my_lib/common.h"
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syscall.h>
#include <unistd.h>
#include <wordexp.h>

#define logger(A)                      \
    do {                               \
        fprintf(stdout, "%s...\n", A); \
        fflush(stdout);                \
    } while (0)

#define md(A) mkdir(A, S_IRWXU | S_IRWXG)

// copy binary file old_path to new_path
void cpy_file(const char old_path[], const char new_path[]) {
    FILE *oldfp, *newfp;
    oldfp = fopen(old_path, "rb");
    newfp = fopen(new_path, "wb");
    while (!feof(oldfp)) {
        const int buf_size = 4096;
        char buf[buf_size] = {};
        int n = fread(buf, 1, buf_size, oldfp);
        fwrite(buf, 1, n, newfp);
    }
    fclose(oldfp);
    fflush(newfp);
    fclose(newfp);
    chmod(new_path, S_IRWXU | S_IRWXG);
}

// make needed dirs, this will move cwd to simp_contianer_root
void prepare_dirs() {
    logger("preparing dirs");
    // md("/data");
    // chown("/data", 1000, 1000);
    md("./simp_container_root/");
    chdir("./simp_container_root/");
    md("./new_root");
    md("./new_root/bin");
    md("./new_root/data");
    md("./new_root/proc");
    md("./new_root/old_root");
}

// execute busybox inside container
void exec_cmd() {
    int ret = 0;
    if ((ret = fork()) > 0) { // parent
        waitpid(ret, NULL, 0);
        execl("/bin/sh", "sh", NULL);
        errExit("execl");
    }
    if (ret == 0) { // child
        execl("/bin/busybox", "busybox", "--install", "/bin/", NULL);
        errExit("execl");
    }
    errExit("fork");
}

// unshare and fork
void do_unshare() {
    logger("doing unshare");
    int ret = 0;
    int us_flags = CLONE_NEWCGROUP | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUSER | CLONE_NEWUTS;
    ret = unshare(us_flags);
    if (ret < 0)
        errExit("unshare");
    if ((ret = fork()) < 0)
        errExit("fork");
    if (ret == 0) { // child
        const char hostname[] = "container01";
        logger("setting hostname");
        sethostname(hostname, strlen(hostname));
        logger("chdir to new root");
        mount("./new_root", "./new_root", NULL, MS_BIND, NULL);
        chdir("./new_root");
        logger("pivot root");
        syscall(SYS_pivot_root, "./", "./old_root");
        logger("doing mount and umount");
        mount("none", "/proc", "proc", 0, NULL);
        umount("/old_root");
        logger("set env and exec busybox sh");
        setenv("hostname", "container01", 0);
        // setenv("PS1", "root@$container01:$(pwd)# ", 0);
        exec_cmd();
    }
    if (ret > 0) { // parent
        waitpid(ret, NULL, 0);
        exit(0);
    }
}

// make root dir and move program to root/bin
void create_container() {
    logger("preparing root");
    prepare_dirs();
    // copy file
    logger("copying file");
    cpy_file("/home/eric/coding/busybox", "./new_root/bin/busybox");
    do_unshare();
    errExit("do unshare");

    // stop here to test
    // logger("execl busybox sh");
    // int ret = execl("./busybox", "busybox", "sh", NULL);
    // errExit("execl");
    // exit(0);
}

int main(int argc, char const *argv[]) {
    logger("Running...");
    create_container();
    // create_container();
}
