#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H

#include <kern/pci.h>

int e1000h_enable(struct pci_func *pcif);

void e1000h_test(void);

#endif	// JOS_KERN_E1000_H
