// Fork a binary tree of processes and display their structure.

#include <inc/lib.h>

void
print_for_a_while(const char *label)
{
    // Do lots of looping, print sometimes, print when done.
    // Used to gauge scheduling tendencies.
    int i;
    const int max = 300000000;
    for (i = 0; i < max; i++) {
        if (i % 10000000 == 0) {
            cprintf("[%9s] Working on %d/%d\n", label, i, max);
        }
    }
    cprintf("[%9s] =======================================> Finished all work!\n");
}

void
umain(int argc, char **argv)
{
    // Set the parent to lowest priority.
    sys_renice(0, ENV_PRI_LOW);

    envid_t child;
    if ((child = fork()) == 0) {
        print_for_a_while("child-high0");
    } else {
        // Boost the child to high priority.
        sys_renice(child, ENV_PRI_MAX);
        print_for_a_while("parent-low");
    }
}
