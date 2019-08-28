#include "../ns_in_opt_header.h"

struct child_args {
    char **argv;
    int pipe_fd[2];
};

static int verbose;

static void usage(char *pname);

static void update_map(char *mapping, char *map_file);

static int childFunc(void *arg);

static void proc_setgroups_write(pid_t child_pid, char const *str);

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

int main(int argc, char *argv[]) {
    int flags, opt, map_zero;
    pid_t child_pid;
    child_args args;
    char *uid_map, *gid_map;
    char map_path[PATH_MAX];
    const int MAP_BUF_SIZE = 100;
    char map_buf[MAP_BUF_SIZE];

    flags = verbose = map_zero = 0;
    gid_map = uid_map = NULL;
    while ((opt = getopt(argc, argv, "+imnpvzuUM:G:")) != -1) {
        switch (opt) {
        case 'i':
            flags |= CLONE_NEWIPC;
            break;
        case 'm':
            flags |= CLONE_NEWNS;
            break;
        case 'n':
            flags |= CLONE_NEWNET;
            break;
        case 'p':
            flags |= CLONE_NEWPID;
            break;
        case 'u':
            flags |= CLONE_NEWUTS;
            break;
        case 'U':
            flags |= CLONE_NEWUSER;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'M':
            uid_map = optarg;
            break;
        case 'z':
            map_zero = 1;
            break;
        case 'G':
            gid_map = optarg;
            break;
        default:
            usage(argv[0]);
            break;
        }
    }

    if (((uid_map != NULL || gid_map != NULL || map_zero) && !(flags & CLONE_NEWUSER)) || (map_zero && (uid_map != NULL || gid_map != NULL)))
        usage(argv[0]);

    args.argv = &argv[optind];
    /* We use a pipe to synchronize the parent and child, in order to
       ensure that the parent sets the UID and GID maps before the child
       calls execve(). This ensures that the child maintains its
       capabilities during the execve() in the common case where we
       want to map the child's effective user ID to 0 in the new user
       namespace. Without this synchronization, the child would lose
       its capabilities if it performed an execve() with nonzero
       user IDs (see the capabilities(7) man page for details of the
       transformation of a process's capabilities during execve()). */

    if (pipe(args.pipe_fd) == -1)
        errExit("pipe");

    child_pid = clone(childFunc, child_stack + STACK_SIZE, flags | SIGCHLD, &args);
    if (child_pid == -1)
        errExit("clone");

    if (verbose)
        printf("%s: PID of child created by clone() is %ld\n", argv[0], (long)child_pid);

    if (uid_map != NULL || map_zero) {
        snprintf(map_path, PATH_MAX, "/proc/%ld/uid_map", (long)child_pid);
        if (map_zero) {
            snprintf(map_buf, MAP_BUF_SIZE, "0 %ld 1", (long)getuid());
            uid_map = map_buf;
        }
        update_map(uid_map, map_path);
    }
    if (gid_map != NULL || map_zero) {
        proc_setgroups_write(child_pid, "deny");
        snprintf(map_path, PATH_MAX, "/proc/%ld/gid_map", (long)child_pid);
        if (map_zero) {
            snprintf(map_buf, MAP_BUF_SIZE, "0 %ld 1", (long)getpid());
            gid_map = map_buf;
        }
        update_map(gid_map, map_path);
    }
    /* Close the write end of the pipe, to signal to the child that we
       have updated the UID and GID maps */
    close(args.pipe_fd[1]);
    if (waitpid(child_pid, NULL, 0) == -1)
        errExit("waitpid");
    if (verbose)
        printf("%s: terminating\n", argv[0]);

    return 0;
}

#define fpe(str) fprintf(stderr, "    %s", str);
void usage(char *pname) {
    fprintf(stderr, "Usage: %s [options] cmd [arg...]\n\n", pname);
    fprintf(stderr, "Create a child process that executes a shell "
                    "command in a new user namespace,\n"
                    "and possibly also other new namespace(s).\n\n");
    fprintf(stderr, "Options can be:\n\n");
    fpe("-i          New IPC namespace\n");
    fpe("-m          New mount namespace\n");
    fpe("-n          New network namespace\n");
    fpe("-p          New PID namespace\n");
    fpe("-u          New UTS namespace\n");
    fpe("-U          New user namespace\n");
    fpe("-M uid_map  Specify UID map for user namespace\n");
    fpe("-G gid_map  Specify GID map for user namespace\n");
    fpe("-z          Map user's UID and GID to 0 in user namespace\n");
    fpe("            (equivalent to: -M '0 <uid> 1' -G '0 <gid> 1')\n");
    fpe("-v          Display verbose messages\n");
    fpe("\n");
    fpe("If -z, -M, or -G is specified, -U is required.\n");
    fpe("It is not permitted to specify both -z and either -M or -G.\n");
    fpe("\n");
    fpe("Map strings for -M and -G consist of records of the form:\n");
    fpe("\n");
    fpe("    ID-inside-ns   ID-outside-ns   len\n");
    fpe("\n");
    fpe("A map string can contain multiple records, separated"
        " by commas;\n");
    fpe("the commas are replaced by newlines before writing"
        " to map files.\n");

    exit(EXIT_FAILURE);
}
/* Update the mapping file 'map_file', with the value provided in
   'mapping', a string that defines a UID or GID mapping. A UID or
   GID mapping consists of one or more newline-delimited records
   of the form:

       ID_inside-ns    ID-outside-ns   length

   Requiring the user to supply a string that contains newlines is
   of course inconvenient for command-line use. Thus, we permit the
   use of commas to delimit records in this string, and replacethem
   with newlines before writing the string to the file. */
void update_map(char *mapping, char *map_file) {
    int fd, j;
    size_t map_len;
    map_len = strlen(mapping);
    for (int j = 0; j < map_len; j++) {
        if (mapping[j] == ',')
            mapping[j] = '\n';
    }
    fd = open(map_file, O_RDWR);
    if (fd == -1) {
        fprintf(stderr, "open %s: %s\n", map_file, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (write(fd, mapping, map_len) != map_len) {
        int tmp_errno = errno;
        fprintf(stderr, "write %s: %d, %s\n", map_file, tmp_errno, strerror(tmp_errno));
        exit(EXIT_FAILURE);
    }
    close(fd);
}

/* Linux 3.19 made a change in the handling of setgroups(2) and the
   'gid_map' file to address a security issue. The issue allowed
   *unprivileged* users to employ user namespaces in order to drop.
   The upshot of the 3.19 changes is that in order to update the
   'gid_maps' file, use of the setgroups() system call in this
   user namespace must first be disabled by writing "deny" to oneof
   the /proc/PID/setgroups files for this namespace.  That is the
   purpose of the following function. */
void proc_setgroups_write(pid_t child_pid, char const *str) {
    char setgroups_path[PATH_MAX];
    int fd;
    snprintf(setgroups_path, PATH_MAX, "/proc/%ld/setgroups", (long)child_pid);
    fd = open(setgroups_path, O_RDWR);
    if (fd == -1)
        errExit("open setgroups_path");
    if (write(fd, str, strlen(str)) == -1)
        fprintf(stderr, "ERROR: write %s: %s\n", setgroups_path, strerror(errno));
    close(fd);
}

int childFunc(void *arg) {
    child_args *args = (child_args *)arg;
    char ch;
    /* Close our descriptor for the write end
       of the pipe so that we see EOF when
       parent closes its descriptor */
    close(args->pipe_fd[1]);
    if (read(args->pipe_fd[0], &ch, 1) != 0) {
        fprintf(stderr, "Failure in child: read from pipe returned != 0\n");
        exit(EXIT_FAILURE);
    }
    close(args->pipe_fd[0]);

    printf("About to exec %s\n", args->argv[0]);
    execvp(args->argv[0], args->argv);
    errExit("execvp");
}
