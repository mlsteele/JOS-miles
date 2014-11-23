// LAB 6: Your driver code here
#include <kern/e1000.h>

#include <inc/assert.h>
#include <inc/string.h>
#include <inc/error.h>
#include <kern/e1000_hw.h>
#include <kern/pmap.h>

// Maximum size from manual section 3.3.3
#define TX_RING_SIZE 32
#define TX_MAX_PACKET_SIZE 16288 // in bytes

struct tx_desc {
    uint64_t desc_addr;
    uint16_t desc_length;
    uint8_t desc_cso;
    uint8_t desc_cmd;
    uint8_t desc_status;
    uint8_t desc_css;
    uint16_t desc_special;
};

// Volatility notes
// http://stackoverflow.com/questions/2304729/how-do-i-declare-an-array-created-using-malloc-to-be-volatile-in-c
static uint32_t volatile *bar0;

// Ring of transmit descriptors.
static struct tx_desc volatile tx_desc_list[TX_RING_SIZE] __attribute__((aligned(16)));

// Packet buffers
static uint8_t tx_buffers[TX_RING_SIZE][TX_MAX_PACKET_SIZE];

// Convert a register offset into a pointer to virtual memory.
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

// offset - offset in bytes, usually one of E1000_Xs from e1000_hw.h
static volatile uint32_t*
e1000_reg(uint32_t offset)
{
    return bar0 + (offset / 4);
}

// Initialize the e1000 for transmission.
// Follows the steps in section 14.5 of the e1000 manual.
static void
e1000_init_transmit()
{
    int i;
    struct PageInfo * pp;
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

    // Ensure the head and tail regs are initialized to 0x0b;
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

    cprintf("e1000_enable return\n");
    return 0;
}

// Transmit a packet located at `packet` of `size` byts long.
// Returns:
//   size  if the packet was added to the tx queue.
//   0  if the packet was discarded due to overload.
//      This is considered success, but the user should attempt a resend later on.
//   -E_INVAL  on invalid parameters.
int
e1000_transmit(void *packet, size_t size)
{
    cprintf("e1000_transmit size: %d\n", size);

    volatile struct tx_desc *desc;
    // note: `packet` is the user supplied buffer,
    //       `buf` is the driver-internal tx buffer.
    uint8_t *buf;
    uint32_t tail;
    uint32_t head;
    bool slot_available;

    if (size <= 0) return -E_INVAL;
    if (size > TX_MAX_PACKET_SIZE) return -E_INVAL;

    // Find a spot in the queue.
    head = *e1000_reg(E1000_TDH);
    tail = *e1000_reg(E1000_TDT);
    cprintf("e1000 head: %d, tail: %d\n", head, tail);
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

void
e1000_test_transmit(void)
{
    int r;
    uint8_t buf[16];

    memset(buf, 0, 16);

    buf[0] = 0xA;
    buf[1] = 0xB;
    buf[7] = 0xC;
    buf[8] = 0xD;

    if ((r = e1000_transmit(&buf, 8)) < 0) {
        panic("transmit failed: %d\n", r);
    }
}
