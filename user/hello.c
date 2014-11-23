// hello, world
#include <inc/lib.h>

void
packet_send_test()
{
    int r;
    uint8_t buf[16];

    memset(buf, 0, 16);

    buf[0] = 0xA;
    buf[1] = 0xB;
    buf[7] = 0xC;
    buf[8] = 0xD;

    if ((r = sys_packet_transmit(&buf, 8)) < 0) {
        panic("send failed: %d\n", r);
    }
}

void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
    packet_send_test();
	cprintf("i am environment %08x\n", thisenv->env_id);
}
