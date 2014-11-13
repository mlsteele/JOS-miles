#include <inc/lib.h>

void
loop()
{
    int i = 0;
    printf("looping forever");
    while(1) {
        if (i > 10000) i = 0;
        if (i == 0) cprintf(".");
        sys_yield();
        i++;
    }
}

void
umain(int argc, char **argv)
{
    binaryname = "loop";
    loop();
    exit();
}
