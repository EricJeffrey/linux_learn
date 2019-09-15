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

const char new_root_path[] = "./simple_container_root/";
const char prog_path[] = "/home/eric/coding/busybox";
const char prog_name[] = "busybox";
const char cmd_to_execute[] = "./busybox --install ./bin";
const int max_len = 256;
char nprog_path[max_len] = {};

#define logger(A) fprintf(STDIN_FILENO, "%s...\n", A)

#define md(A) mkdir(A, S_IRWXU | S_IRWXG)

// make needed dirs, this will move cwd to simp_contianer_root
void prepare_dirs() {
    logger("preparing dirs");
    md("/data");
    md("./simp_container_root/");
    chdir("./simp_container_root/");
    md("./new_root");
    chdir("./new_root");
    md("./bin");
    md("./data");
    md("./proc");
    md("./old_root");
    chdir("../"); // change to simp_container_root
}

void cpy_file(const char old_path[], const char new_path[]) {
    FILE *oldfp, *newfp;
    oldfp = fopen(old_path, "rb");
    newfp = fopen(new_path, "wb");
    while (!feof(oldfp)) {
        const int buf_size = 1024;
        char buf[buf_size] = {};
        int n = fread(buf, buf_size, 1, oldfp);
        fwrite(buf, n, 1, newfp);
    }
    fclose(oldfp);
    fclose(newfp);
    chmod(new_path, S_IRWXU | S_IRWXG);
}

// make root dir and move program to root/bin
void prepare_root() {
    logger("preparing root");
    prepare_dirs();
    // copy file
    cpy_file(prog_path, "./busybox");

    // stop here to test

    int slash_pos = strrchr(prog_path, '/') - prog_path;
    snprintf(nprog_path, max_len, "./bin%s", prog_path + slash_pos);

    logger("copying exe file");
    // copy file
    FILE *nfp = fopen(nprog_path, "wb");
    FILE *ofp = fopen(prog_path, "rb");
    const int buf_size = 2048;
    char buf[buf_size] = {};
    while (!feof(ofp)) {
        int nread = fread(buf, buf_size, 1, ofp);
        int nwrite = fwrite(buf, buf_size, 1, nfp);
    }
    fclose(nfp);
    fclose(ofp);
    // make executable
    chmod(nprog_path, S_IRWXG | S_IRWXU);
}

void config_container() {
}

void create_container() {
    logger("doing create_container");
    // make /data dir
    const char data_path[] = "/data";
    mkdir(data_path, S_IRWXU | S_IRWXG);
    chown(data_path, getuid(), getgid());
    int us_flag = CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS | CLONE_NEWUSER | CLONE_NEWUTS | CLONE_NEWCGROUP | CLONE_NEWIPC;
    // to new namespace
    logger("unshare and fork");

    unshare(us_flag);
    int ret = 0;
    if ((ret = fork()) > 0) { // parent
        waitpid(ret, NULL, 0);
        exit(0);
    } else if (ret == 0) { // child

        logger("child: sethostname and mkdir rootfs");

        const char nhostname[] = "mycontainer01";
        sethostname(nhostname, strlen(nhostname));
        // old new rootfs dir
        const char nrootfs[] = "./new_rootfs/";
        const char orootfs[] = "./old_rootfs/";
        mkdir(orootfs, S_IRWXG | S_IRWXU);
        mkdir(nrootfs, S_IRWXU | S_IRWXG);
        mkdir("./new_rootfs/data/", S_IRWXG | S_IRWXU);
        mount(nrootfs, nrootfs, NULL, MS_BIND, NULL);
        mount("/data", "./new_rootfs/data/", NULL, MS_BIND, NULL);
        logger("child: pivot root");
        // pivot_root
        syscall(SYS_pivot_root, nrootfs, orootfs);
        chdir(nrootfs);
        logger("child: mount procfs");
        // make proc fs
        mkdir("./proc/", S_IRWXU | S_IRWXG);
        mount(NULL, "./proc", "proc", 0, NULL);
        // run prog in child
        // execl(nprog_path, nprog_path, NULL);
        errExit("execl");
    } else if (ret < 0) {
        errExit("fork");
    }
}

int main(int argc, char const *argv[]) {
    logger("Running...");
    prepare_root();
    // create_container();
}
