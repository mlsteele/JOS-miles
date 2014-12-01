#include "ns.h"

void
output(envid_t ns_envid)
{
    binaryname = "ns_output";

    // LAB 6: Your code here:
    // 	- read a packet from the network server
    //	- send the packet to the device driver

    while (true) {
        int r;
        envid_t from_env;
        uint32_t ipc_type;
        struct jif_pkt *pkt;
        void *data;
        size_t len;

        // Read packet from network server.
        ipc_type = ipc_recv(&from_env, (void*)REQVA, NULL);
        if (from_env != ns_envid)
            continue;
        if (ipc_type != NSREQ_OUTPUT)
            continue;
        pkt = (struct jif_pkt*)REQVA;

        // Send packet to device driver.
        data = pkt->jp_data;
        len = pkt->jp_len;
        if (len <= 0) {
            cprintf("*SKIP*\n");
            continue;
        }
        cprintf("SENDING SENDING SENDING\n");
        if ((r = sys_packet_transmit(data, len)) < 0)
            panic("packet transmission failed: %e", r);
    }
}
