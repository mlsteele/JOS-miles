#include "ns.h"

extern union Nsipc nsipcbuf;

// jif_pkt and its PACKET_BUF_SIZE must fit on a page.
#define PACKET_BUF_SIZE 1518 // in bytes

void
input(envid_t ns_envid)
{
    binaryname = "ns_input";

    // LAB 6: Your code here:
    //  - read a packet from the device driver
    //  - send it to the network server
    // Hint: When you IPC a page to the network server, it will be
    // reading from it for a while, so don't immediately receive
    // another packet in to the same physical page.
    int r;

    if ((r = sys_page_alloc(0, &nsipcbuf, PTE_P | PTE_U | PTE_W)) < 0)
        panic("could not allocate page: %e");

    while (true) {
        struct jif_pkt *pkt;

        // Get a new page so the ns server can use it even when another packet is received.
        if ((r = sys_page_unmap(0, (void*)&nsipcbuf)) < 0)
            panic("unmap failed: %e", r);
        if ((r = sys_page_alloc(0, (void*)&nsipcbuf, PTE_P | PTE_U | PTE_W)) < 0)
            panic("alloc failed: %e", r);

        pkt = (struct jif_pkt*)&nsipcbuf;
        if ((r = sys_packet_receive(&(pkt->jp_data), PACKET_BUF_SIZE)) < 0)
            panic("packet receive failed: %d", r);
        if (r == 0)
            continue;
        pkt->jp_len = r;

        cprintf("RECV RECV RECV\n");
        if ((r = sys_ipc_try_send(ns_envid, NSREQ_INPUT, pkt, PTE_P | PTE_U)))
            panic("packet receive relay failed: %e", r);
    }
}
