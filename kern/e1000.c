// LAB 6: Your driver code here
#include <kern/e1000.h>

#include <inc/assert.h>
#include <kern/e1000_hw.h>
#include <kern/pmap.h>

struct tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
};

// Volatility notes
// http://stackoverflow.com/questions/2304729/how-do-i-declare-an-array-created-using-malloc-to-be-volatile-in-c
static uint32_t volatile *bar0;

// Ring of transmit descriptors.
static struct tx_desc volatile tx_desc_list[32] __attribute__((aligned(16)));

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
e1000h_reg(uint32_t offset)
{
    return bar0 + (offset / 4);
}

// Initialize the e1000h for transmission.
// Follows the steps in section 14.5 of the e1000h manual.
static void
e1000h_init_transmit()
{
    // Point TDBAL to the descriptor list. (Should be 16 byte aligned)
    *e1000h_reg(E1000_TDBAL) = PADDR(&tx_desc_list);

    // Set TDLEN to the length of the descriptor list. (must be 128 byte aligned)
    *e1000h_reg(E1000_TDLEN) = sizeof(tx_desc_list);

    // Ensure the head and tail regs are initialized to 0x0b;
    *e1000h_reg(E1000_TDH) = 0;
    *e1000h_reg(E1000_TDT) = 0;

    // Initialize the Transmit Control Register
    uint32_t tctl = 0;
    tctl |= E1000_TCTL_EN;
    tctl |= E1000_TCTL_PSP;
    tctl |= 0x00000100; // Set E1000_TCTL_CT to 10h
    tctl |= 0x00040000; // Set E1000_TCTL_COLD to 40h
    *e1000h_reg(E1000_TCTL) = tctl;

    // See e1000h manual section 13.4.35 table 13-77 
    const uint32_t ipgt = 10;
    const uint32_t ipgr1 = 8;
    const uint32_t ipgr2 = 6;
    uint32_t tipg = 0;
    tipg |= ipgt;
    tipg |= ipgr1 << 10;
    tipg |= ipgr2 << 20;
    *e1000h_reg(E1000_TIPG) = tipg;
}

// Returns 0 on success.
// Returns a negative value on failure.
int
e1000h_enable(struct pci_func *pcif)
{
    cprintf("e1000h_enable start\n");
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
    uint32_t status = *e1000h_reg(E1000_STATUS);
    cprintf("e1000h status: %p\n", status);
    assert(0x80080783 == status);

    e1000h_init_transmit();

    cprintf("e1000h_enable return\n");
    return 0;
}
