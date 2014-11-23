#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

int e1000h_enable(struct pci_func *pcif);
int e1000h_send(void *packet, size_t size);
void e1000h_test_send(void);

#endif	// JOS_KERN_E1000_H
