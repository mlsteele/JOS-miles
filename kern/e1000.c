// LAB 6: Your driver code here
#include <kern/e1000.h>

#include <inc/assert.h>
#include <inc/string.h>
#include <inc/error.h>
#include <kern/e1000_hw.h>
#include <kern/pmap.h>

// Maximum size from manual section 3.3.3
#define TX_RING_SIZE 32
#define TX_MAX_PACKET_SIZE 1518 // in bytes

#define RX_RING_SIZE 128
#define RX_MAX_PACKET_SIZE 2048 // in bytes

struct tx_desc {
    uint64_t desc_addr;
    uint16_t desc_length;
    uint8_t desc_cso;
    uint8_t desc_cmd;
    uint8_t desc_status;
    uint8_t desc_css;
    uint16_t desc_special;
};

struct rx_desc {
    uint64_t desc_addr;
    uint16_t desc_length;
    uint16_t desc_checksum;
    uint8_t desc_status;
    uint8_t desc_errors;
    uint16_t desc_special;
};

// Volatility notes
// http://stackoverflow.com/questions/2304729/how-do-i-declare-an-array-created-using-malloc-to-be-volatile-in-c
static uint32_t volatile *bar0;

// Ring of transmit descriptors.
static struct tx_desc volatile tx_desc_list[TX_RING_SIZE] __attribute__((aligned(16)));

// Packet buffers for transmit.
static uint8_t tx_buffers[TX_RING_SIZE][TX_MAX_PACKET_SIZE];

// Ring of receive descriptors.
static struct rx_desc volatile rx_desc_list[RX_RING_SIZE] __attribute__((aligned(16)));

// Packet buffers for receiving.
static uint8_t rx_buffers[RX_RING_SIZE][RX_MAX_PACKET_SIZE] __attribute__((aligned(16)));

void
debug_pci_func(struct pci_func *pcif)
{
    cprintf("pci_func header\n");
    cprintf("  bus: %p\n", pcif->bus);
    cprintf("  dev: %d\n", pcif->dev);
    cprintf("  func: %d\n", pcif->func);
    cprintf("  dev_id: %p\n", pcif->dev_id);
    cprintf("  dev_class: %p\n", pcif->dev_class);
    cprintf("  reg_base: %p %p %p %p %p %p\n",
        pcif->reg_base[0],
        pcif->reg_base[1],
        pcif->reg_base[2],
        pcif->reg_base[3],
        pcif->reg_base[4],
        pcif->reg_base[5],
        pcif->reg_base[6]);
    cprintf("  reg_size: %p %p %p %p %p %p\n",
        pcif->reg_size[0],
        pcif->reg_size[1],
        pcif->reg_size[2],
        pcif->reg_size[3],
        pcif->reg_size[4],
        pcif->reg_size[5],
        pcif->reg_size[6]);
    cprintf("  irq_line: %d\n", pcif->irq_line);
}

// Convert a register offset into a pointer to virtual memory.
// offset - offset in bytes, usually one of E1000_Xs from e1000_hw.h
static volatile uint32_t*
e1000_reg(uint32_t offset)
{
    return bar0 + (offset / 4);
}

void
debug_rx_regs(void)
{
    cprintf("debug_rx_regs\n");
    cprintf("  RA: %p\n", *e1000_reg(E1000_RA));
    cprintf("  RA + 4: %d\n", *e1000_reg(E1000_RA + 4));
    cprintf("  MTA: %d\n", *e1000_reg(E1000_MTA));
    cprintf("  RDBAL: %d\n", *e1000_reg(E1000_RDBAL));
    cprintf("  RDLEN: %d\n", *e1000_reg(E1000_RDLEN));
    cprintf("  RDH: %d\n", *e1000_reg(E1000_RDH));
    cprintf("  RDT: %d\n", *e1000_reg(E1000_RDT));
    cprintf("  RCTL: %d\n", *e1000_reg(E1000_RCTL));
}

// Initialize the e1000 for transmission.
// Follows the steps in section 14.5 of the e1000 manual.
static void
e1000_init_transmit()
{
    int i;
    struct tx_desc desc_template = {
        .desc_addr = 0,
        .desc_length = 0,
        .desc_cso = 0,
        .desc_cmd = 0,
        .desc_status = E1000_TXD_STAT_DD,
        .desc_css = 0,
        .desc_special = 0,
    };

    // Initialize the descriptors.
    // memset(&tx_desc_list, 0, sizeof(tx_desc_list));
    for (i = 0; i < TX_RING_SIZE; i++) {
        tx_desc_list[i] = desc_template;
    }

    // Zero the buffers.
    memset(&tx_buffers, 0, sizeof(tx_buffers));

    // Point TDBAL to the descriptor list. (Should be 16 byte aligned)
    *e1000_reg(E1000_TDBAL) = PADDR(&tx_desc_list);

    // Set TDLEN to the length of the descriptor list. (must be 128 byte aligned)
    *e1000_reg(E1000_TDLEN) = sizeof(tx_desc_list);

    // Ensure the head and tail regs are initialized to 0b;
    *e1000_reg(E1000_TDH) = 0;
    *e1000_reg(E1000_TDT) = 0;

    // Initialize the Transmit Control Register
    uint32_t tctl = 0;
    tctl |= E1000_TCTL_EN;
    tctl |= E1000_TCTL_PSP;
    tctl |= 0x00000100; // Set E1000_TCTL_CT to 10h
    tctl |= 0x00040000; // Set E1000_TCTL_COLD to 40h
    *e1000_reg(E1000_TCTL) = tctl;

    // See e1000 manual section 13.4.35 table 13-77
    const uint32_t ipgt = 10;
    const uint32_t ipgr1 = 8;
    const uint32_t ipgr2 = 6;
    uint32_t tipg = 0;
    tipg |= ipgt;
    tipg |= ipgr1 << 10;
    tipg |= ipgr2 << 20;
    *e1000_reg(E1000_TIPG) = tipg;
}

// Initialize the e1000 for receiving.
// Follows the steps in section 14.4 of the e1000 manual.
static void
e1000_init_receive()
{
    int i;
    uint32_t mac_stage;
    struct rx_desc desc_template = {
        .desc_addr = 0,
        .desc_length = 0,
        .desc_checksum = 0,
        .desc_status = 0,
        .desc_errors = 0,
        .desc_special = 0,
    };

    // Initialize the descriptors.
    for (i = 0; i < RX_RING_SIZE; i++) {
        rx_desc_list[i] = desc_template;
        rx_desc_list[i].desc_addr = PADDR(&rx_buffers[i][0]);
    }

    // Fill all the buffers with identifiable junk.
    memset(&rx_buffers, 0xbe, sizeof(rx_buffers));

    // Program the Receive Address Register(s) (RAL/RAH) with the desired Ethernet addresses.
    // When writing to this register, always write low-to-high. When clearing this register, always clear high-to-low.
    // RAL[0]/RAH[0] should always be used to store the Individual Ethernet MAC address of the Ethernet controller.
    // RAL0 contains the lower 32-bit of the 48-bit Ethernet address.
    // RAH0 contains the high 32-bit of the 48-bit Ethernet address.
    // hard-coded MAC 52:54:00:12:34:56
    *e1000_reg(E1000_RA) = 0x12005452;
    *e1000_reg(E1000_RA + 4) = E1000_RAH_AV | 0x5634;

    // Initialize the MTA (Multicast Table Array) to 0b.
    *e1000_reg(E1000_MTA) = 0;

    // Point Receive Descriptor Base Address (RDBAL) to the descriptor list. (Should be 16 byte aligned)
    *e1000_reg(E1000_RDBAL) = PADDR(&rx_desc_list);

    // Set the Receive Descriptor Length (RDLEN) to the length of the descriptor list. (must be 128 byte aligned)
    *e1000_reg(E1000_RDLEN) = sizeof(rx_desc_list);

    // The Receive Descriptor Head and Tail registers are initialized (by hardware) to 0b after a power-on
    // or a software-initiated Ethernet controller reset. Receive buffers of appropriate size should be
    // allocated and pointers to these buffers should be stored in the receive descriptor ring. Software
    // initializes the Receive Descriptor Head (RDH) register and Receive Descriptor Tail (RDT) with the
    // appropriate head and tail addresses. Head should point to the first valid receive descriptor in the
    // descriptor ring and tail should point to one descriptor beyond the last valid descriptor in the
    // descriptor ring.
    // Initialize the head and tail regs are initialized to 0b;
    *e1000_reg(E1000_RDH) = 0;
    *e1000_reg(E1000_RDT) = RX_RING_SIZE - 1;

    // Initialize the Receive Control Register
    uint32_t rctl = 0;
    rctl |= E1000_RCTL_EN;
    rctl |= E1000_RCTL_BAM;
    // BSIZE = 00b indicating 2048 size buffers.
    rctl |= E1000_RCTL_SECRC;
    *e1000_reg(E1000_RCTL) = rctl;

    debug_rx_regs();
}

// Returns 0 on success.
// Returns a negative value on failure.
int
e1000_enable(struct pci_func *pcif)
{
    cprintf("e1000_enable start\n");
    debug_pci_func(pcif);
    // Fill in the BAR entries and irq_line of pcif.
    pci_func_enable(pcif);
    debug_pci_func(pcif);

    // Map some memory for the BAR.
    bar0 = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);

    // Make sure the mapping is correct.
    // Check the device status register (section 13.4.2)
    // 4 byte register that starts at byte 8 of bar0.
    // 0x80080783 indicates a full duplex link is up
    // at 1000 MB/s, among other things.
    uint32_t status = *e1000_reg(E1000_STATUS);
    cprintf("e1000 status: %p\n", status);
    assert(0x80080783 == status);

    e1000_init_transmit();
    e1000_init_receive();

    cprintf("e1000_enable return\n");
    return 0;
}

// Transmit a packet located at `packet` of `size` bytes long.
// Returns:
//   size  if the packet was added to the tx queue.
//   0  if the packet was discarded due to overload.
//      This is considered success, but the user should attempt a resend later on.
//   -E_INVAL  on invalid parameters.
int
e1000_transmit(void *packet, size_t size)
{
    // note: `packet` is the user supplied buffer,
    //       `buf` is the driver-internal tx buffer.
    uint8_t *buf;
    volatile struct tx_desc *desc;
    uint32_t tail;
    uint32_t head;
    bool slot_available;

    // TODO(miles): minimum size is 48? section 3.3.3
    if (size <= 0) return -E_INVAL;
    if (size > TX_MAX_PACKET_SIZE) return -E_INVAL;

    // Find a spot in the queue.
    head = *e1000_reg(E1000_TDH);
    tail = *e1000_reg(E1000_TDT);
    desc = &tx_desc_list[tail];
    slot_available = (desc->desc_status & E1000_TXD_STAT_DD);
    if (!slot_available) {
        cprintf("e1000: Out of descriptors. Dropping packet.\n");
        return 0;
    }

    // Copy payload.
    buf = &tx_buffers[tail][0];
    memcpy(buf, packet, size);
    // Check that the copy worked a little.
    assert(buf[0] == ((uint8_t*)packet)[0]);
    // Check that the buffer is still right (had a bug once).
    assert(&tx_buffers[tail][0] == buf);

    // Write descriptor.
    desc->desc_addr = PADDR(buf);
    assert(desc->desc_addr != ((uint32_t)NULL));
    desc->desc_length = size;
    assert(desc->desc_length != 0);
    desc->desc_cso = 0;
    desc->desc_cmd = (E1000_TXD_CMD_RS >> 24) | (E1000_TXD_CMD_EOP >> 24);
    desc->desc_status = 0;
    desc->desc_css = 0;
    desc->desc_special = 0;

    // Increment tail.
    tail += 1;
    tail %= TX_RING_SIZE;
    *e1000_reg(E1000_TDT) = tail;

    return size;
}

// Receive a packet to va `dest` that is <= `max_size` bytes long.
// max_size's above RX_MAX_PACKET_SIZE are not yet supported.
// Returns:
//   size  of the packet if one was successfully received and written to `dest`.
//         Will always be < `max_size`
//   0  if there was no packet available.
//   -E_INVAL  on invalid parameters.
int
e1000_receive(void *dst, size_t max_size)
{
    // note: `dst` is the user supplied buffer,
    //       `buf` is the driver-internal rx buffer.
    uint8_t *buf;
    size_t length;
    volatile struct rx_desc *desc;
    uint32_t tail;

    if (max_size <= 0) return -E_INVAL;
    if (max_size > RX_MAX_PACKET_SIZE) return -E_INVAL;

    // Increment local tail. (not written to reg)
    tail = *e1000_reg(E1000_RDT);
    tail += 1;
    tail %= RX_RING_SIZE;

    // Check DD bit of tail+1
    desc = &rx_desc_list[tail];
    if (!(desc->desc_status & E1000_RXD_STAT_DD))
        return 0;
    cprintf("DRIVER RECV\n");
    // TODO(miles): handle multi-buffer packets.
    assert(desc->desc_status & E1000_RXD_STAT_EOP);
    length = desc->desc_length;

    // Copy data from buffer into user's `dst`.
    buf = &rx_buffers[tail][0];
    assert(desc->desc_addr == PADDR(buf));
    memcpy(dst, buf, length);
    // Check that the copy worked a little.
    assert(((uint8_t*)dst)[0] == buf[0]);
    // Check that the buffer is still right (had a bug once).
    assert(&rx_buffers[tail][0] == buf);

    // Re-initialize consumed descriptor
    desc->desc_addr = PADDR(&rx_buffers[tail][0]);
    desc->desc_length = 0;
    desc->desc_checksum = 0;
    desc->desc_status = 0;
    desc->desc_errors = 0;
    desc->desc_special = 0;

    // Write incremented tail.
    *e1000_reg(E1000_RDT) = tail;
    cprintf("E1000 tail after succ read: %d\n", *e1000_reg(E1000_RDT));

    cprintf("e1000_receive return %d\n", length);
    return length;
}
