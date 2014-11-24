#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

int e1000_enable(struct pci_func *pcif);
int e1000_transmit(void *packet, size_t size);
int e1000_receive(void *dst, size_t max_size);

#endif	// JOS_KERN_E1000_H
