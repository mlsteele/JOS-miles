#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

int e1000_enable(struct pci_func *pcif);
int e1000_send(void *packet, size_t size);
void e1000_test_send(void);

#endif	// JOS_KERN_E1000_H
