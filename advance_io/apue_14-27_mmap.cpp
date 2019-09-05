#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define copyincr (1024 * 4)

#define errExit(A)              \
    do {                        \
        printf("error: %s", A); \
        exit(EXIT_FAILURE);     \
    } while (0)

#define logger(A)             \
    do {                      \
        printf("%s...\n", A); \
    } while (0)

int main(int argc, char const *argv[]) {
    int fdin, fdout;
    void *src, *dst;
    size_t copysz;
    struct stat sbuf;
    off_t fsz = 0;

    if (argc != 3)
        errExit("usage: a.sh <fromfile> <tofile>");
    logger("open argv1");
    if ((fdin = open(argv[1], O_RDONLY)) < 0)
        errExit("cannot open argv1 for reading");
    logger("open argv2");
    if ((fdout = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, S_IWUSR)) < 0)
        errExit("cannot open argv2 for read write");
    logger("execute fstat");
    if (fstat(fdin, &sbuf) < 0)
        errExit("fstat");
    logger("execute ftruncate");
    if (ftruncate(fdout, sbuf.st_size) < 0)
        errExit("ftruncate");
    while (fsz < sbuf.st_size) {
        logger("get map size");
        if ((sbuf.st_size - fsz) > copyincr)
            copysz = copyincr;
        else
            copysz = sbuf.st_size - fsz;
        logger("map src");
        if ((src = mmap(0, copysz, PROT_READ, MAP_SHARED, fdin, fsz)) == MAP_FAILED)
            errExit("mmap input");
        logger("map dst");
        if ((dst = mmap(0, copysz, PROT_READ | PROT_WRITE, MAP_SHARED, fdout, fsz)) == MAP_FAILED)
            errExit("mmap output");
        memcpy(dst, src, copysz);
        munmap(src, copysz);
        munmap(dst, copysz);
        fsz += copysz;
    }
    logger("done");
    exit(0);
}
