#include "/home/eric/coding/my_lib/common.h"
#include <dirent.h>
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

const int STACK_SIZE = 1024 * 1024;
char child_stack[STACK_SIZE];

#define LOG_ON 1

#define LOGGER(A)                          \
    do {                                   \
        if (LOG_ON) {                      \
            fprintf(stderr, "%s...\n", A); \
            fflush(stderr);                \
        }                                  \
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
    LOGGER("PREPARE DIRS");

    md("./simp_container_root/");
    chdir("./simp_container_root/");
    md("./new_root");
    md("./new_root/bin");
    md("./new_root/proc");
    md("./new_root/old_root");
}

// execute busybox inside container
void exec_cmd() {
    LOGGER("EXEC CMD");

    int ret = fork();
    if (ret > 0) { // parent
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

// new namespace init
int new_ns_init(void *arg) {
    // sync with map_user_grp
    int *pipe_fd = (int *)arg;
    close(pipe_fd[1]);
    char ch;
    // block untile fd[1] is closed
    if (read(pipe_fd[0], &ch, 1) != 0)
        errExit("PIPE READ ERROR!");

    LOGGER("NAMESPACE INIT");

    const char hostname[] = "container01";

    LOGGER("SET HOSTNAME");
    sethostname(hostname, strlen(hostname));

    LOGGER("chdir to new root");
    mount("./new_root", "./new_root", NULL, MS_BIND, NULL);
    chdir("./new_root");

    LOGGER("pivot root");
    syscall(SYS_pivot_root, "./", "./old_root");

    LOGGER("doing mount and umount");
    mount("none", "/proc", "proc", 0, NULL);
    umount("/old_root");

    LOGGER("exec busybox sh");
    exec_cmd();
}

// update uid/gid map file
void update_map(char *map_file_path, char const *content) {
    int fd_uid = open(map_file_path, O_RDWR);
    int len = strlen(content);
    if (fd_uid < 0)
        errExit("OPEN %s ERROR!", map_file_path);
    int ret = write(fd_uid, content, len);
    if (ret < 0)
        errExit("WRITE %s ERROR!", map_file_path);
    close(fd_uid);
}

// map uid and gid, 0 1000 10
void map_ugid(pid_t child_pid) {
    LOGGER("MAP UID GID");

    const int buf_size = 30;
    char buf[] = "0 1000 1";
    char id_path[buf_size] = {};

    snprintf(id_path, buf_size, "/proc/%ld/uid_map", (long)child_pid);
    update_map(id_path, "0 1000 1");

    // write deny to /proc/$$/setgroups
    char setgrp_path[buf_size] = {};
    snprintf(setgrp_path, buf_size, "/proc/%ld/setgroups", (long)child_pid);
    update_map(setgrp_path, "deny");

    memset(id_path, 0, strlen(id_path));
    snprintf(id_path, buf_size, "/proc/%ld/gid_map", (long)child_pid);
    update_map(id_path, buf);
}

// unshare and fork
int do_clone(int pipe_fd[2]) {
    LOGGER("DO CLONE");

    int ns_flag = 0;
    // ns_flag |= CLONE_NEWCGROUP;
    ns_flag |= CLONE_NEWIPC;
    ns_flag |= CLONE_NEWNET;
    ns_flag |= CLONE_NEWNS;
    ns_flag |= CLONE_NEWPID;
    ns_flag |= CLONE_NEWUTS;
    ns_flag |= CLONE_NEWUSER;
    ns_flag |= SIGCHLD;

    int ret = clone(new_ns_init, child_stack + STACK_SIZE, ns_flag, pipe_fd);
    if (ret < 0)
        errExit("CLONE ERROR!");
    return ret;
}

// make root dir and move program to root/bin
void create_container() {
    LOGGER("CREATE CONTAINER");

    prepare_dirs();

    cpy_file("/home/eric/coding/busybox", "./new_root/bin/busybox");

    int pipe_fd[2];
    pipe(pipe_fd);

    pid_t child_pid;
    child_pid = do_clone(pipe_fd);

    map_ugid(child_pid);
    close(pipe_fd[1]);
    LOGGER("PARENT: CLOSE PIPE");

    if (child_pid > 0) { // parent
        waitpid(child_pid, NULL, 0);
        exit(0);
    }

    errExit("CREATE CONTAINER ERROR");
}

int main(int argc, char const *argv[]) {
    create_container();
    return 0;
}
