#include "wrappers.h"
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

#define MD(A) Mkdir(A, S_IRWXU | S_IRWXG)

// 将 old_path 指向的文件复制到 new_path，出错时返回 -1
void CopyFile(const char old_path[], const char new_path[]) {
    LOGGER_DEBUG_SIMP("COPY FILE");
    FILE *oldfp, *newfp;
    if ((oldfp = fopen(old_path, "rb")) == NULL)
        errExit("FOPEN ERROR, PATH: %s", old_path);
    if ((newfp = fopen(new_path, "wb")) == NULL)
        errExit("FOPEN ERROR, PATH: %s", new_path);

    int n;
    const int buf_size = 4096;
    char buf[buf_size] = {};
    while (!feof(oldfp)) {
        if ((n = fread(buf, 1, buf_size, oldfp)) < 0) {
            if (ferror(oldfp))
                errExit("FREAD ERROR, PATH: %s", old_path);
            break;
        }
        if (fwrite(buf, 1, n, newfp) != n)
            errExit("FWRITE ERROR, PATH: %s", new_path);
    }

    fclose(oldfp);
    fflush(newfp);
    fclose(newfp);
    Chmod(new_path, S_IRWXU | S_IRWXG);
}
// make needed dirs, this will move cwd to simp_contianer_root
void prepare_dirs() {
    LOGGER_DEBUG_SIMP("PREPARE DIRS");

    MD("../simp_container_root/");
    Chdir("../simp_container_root/");
    MD("./new_root");
    MD("./new_root/bin");
    MD("./new_root/proc");
    MD("./new_root/old_root");
}

// execute busybox inside container
void exec_cmd() {
    LOGGER_DEBUG_SIMP("EXEC CMD");

    int ret = Fork();
    if (ret > 0) { // parent
        Waitpid(ret, NULL, 0);
        Execl("/bin/sh", "sh", NULL);
    }
    if (ret == 0) // child
        Execl("/bin/busybox", "busybox", "--install", "/bin/", NULL);
}

// new namespace init
int new_ns_init(void *arg) {
    LOGGER_DEBUG_SIMP("NAMESPACE INIT");

    // sync with map_user_grp
    int *pipe_fd = (int *)arg;
    Close(pipe_fd[1]);
    char ch;
    // block untile fd[1] is closed
    Read(pipe_fd[0], &ch, 1);
    LOGGER_DEBUG_SIMP("PIPE CLOSED");

    const char hostname[] = "container01";
    Sethostname(hostname, strlen(hostname));

    Mount("./new_root", "./new_root", NULL, MS_BIND, NULL);
    Chdir("./new_root");

    Syscall(SYS_pivot_root, "./", "./old_root");

    Mount("none", "/proc", "proc", 0, NULL);
    // Umount("/old_root");

    exec_cmd();
}

// update uid/gid map file
void update_map(const char *map_file_path, char const *content) {
    LOGGER_DEBUG_SIMP("UPDATE MAP");

    int fd_uid = Open(map_file_path, O_RDWR);
    int len = strlen(content);
    int ret = Write(fd_uid, content, len);
    Close(fd_uid);
}

// map uid and gid, 0 1000 10
void map_ugid(pid_t child_pid) {
    LOGGER_DEBUG_SIMP("MAP UID GID");

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

// make root dir and move program to root/bin
void create_container() {
    LOGGER_DEBUG_SIMP("CREATE CONTAINER");

    prepare_dirs();

    CopyFile("/home/eric/coding/busybox", "./new_root/bin/busybox");

    int pipe_fd[2];
    Pipe(pipe_fd);

    int ns_flag = 0;
    pid_t child_pid;

    LOGGER_DEBUG_SIMP("CLONE CHILD");
    ns_flag |= CLONE_NEWCGROUP;
    ns_flag |= CLONE_NEWIPC;
    ns_flag |= CLONE_NEWNET;
    ns_flag |= CLONE_NEWNS;
    ns_flag |= CLONE_NEWPID;
    ns_flag |= CLONE_NEWUTS;
    ns_flag |= CLONE_NEWUSER;
    ns_flag |= SIGCHLD;
    child_pid = Clone(new_ns_init, child_stack + STACK_SIZE, ns_flag, pipe_fd);

    map_ugid(child_pid);
    Close(pipe_fd[1]);

    LOGGER_DEBUG_FORMAT("CLONE OVER, PARENT -- MY_PID: %ld", long(getpid()));
    LOGGER_DEBUG_FORMAT("CLONE OVER, PARENT -- CHILD_PID: %ld", long(child_pid));
    Waitpid(child_pid, NULL, 0);
    LOGGER_DEBUG_SIMP("SHOULD NOT COME HERE BEFORE END");
    kill(0, SIGKILL);
    exit(0);
}

int main(int argc, char const *argv[]) {
    LOGGER_SET_LV(LOG_LV_VERBOSE);
    create_container();
    return 0;
}
