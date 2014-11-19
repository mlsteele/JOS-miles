#include <kern/e1000.h>

#include <inc/assert.h>

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

// Returns 0 on success.
// Returns a negative value on failure.
int
e1000h_enable(struct pci_func *pcif)
{
    debug_pci_func(pcif);
    pci_func_enable(pcif);
    debug_pci_func(pcif);
    return 0;
}
