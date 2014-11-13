#include <inc/lib.h>

void
usage(void)
{
    cprintf("usage: touch FILE...\n");
    exit();
}

void
touch(char *path)
{
    int fd;

    if ((fd = open(path, O_CREAT)) < 0) {
        cprintf("open %s: %e\n", path, fd);
        exit();
    }

    close(fd);
}

void
umain(int argc, char **argv)
{
    int i;

    binaryname = "touch";

    if (argc < 2) {
        cprintf("missing FILE operand\n");
        usage();
    }

    for (i = 1; i < argc; i++) {
        touch(argv[i]);
    }
}
