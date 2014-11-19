#include <kern/e1000.h>

#include <inc/assert.h>
#include <kern/e1000_hw.h>
#include <kern/pmap.h>

volatile uint32_t *bar0;

// LAB 6: Your driver code here
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
e1000h_reg(uint32_t offset)
{
    return bar0 + (offset / 4);
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

    cprintf("e1000h_enable return\n");
    return 0;
}
