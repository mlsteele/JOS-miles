#include "ns.h"

void
output(envid_t ns_envid)
{
    binaryname = "ns_output";

    // LAB 6: Your code here:
    // 	- read a packet from the network server
    //	- send the packet to the device driver
    envid_t ns_env;

    // Wait for the network server env to show up.
    // TODO(miles): Only accept from the net (doesn't work in tests)
    // cprintf("ns output waiting for ns env\n");
    // while ((ns_env = ipc_find_env(ENV_TYPE_NS)) == 0) sys_yield();
    // cprintf("ns output discovered ns env at %p\n", ns_env);
    ns_env = 0;

    while (true) {
        int r;
        envid_t from_env;
        uint32_t ipc_type;
        struct jif_pkt *pkt;
        void *data;
        size_t len;

        // Read packet from network server.
        ipc_type = ipc_recv(&from_env, (void*)REQVA, NULL);
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
        if ((r = sys_packet_transmit(data, len)) < 0)
            panic("packet transmission failed: %e", r);
    }
}
