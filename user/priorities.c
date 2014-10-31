// Fork a binary tree of processes and display their structure.

#include <inc/lib.h>

void
print_for_a_while(const char *label)
{
    int i;
    const int max = 300000000;
    for (i = 0; i < max; i++) {
        if (i % 10000000 == 0) {
            cprintf("[%8s] Working on %d/%d\n", label, i, max);
        }
    }
    cprintf("[%6s] =======================================> Finished all work!\n");
}

void
umain(int argc, char **argv)
{
    if (fork() == 0) {
        print_for_a_while("child-high");
    } else {
        print_for_a_while("parent-low");
    }
}
