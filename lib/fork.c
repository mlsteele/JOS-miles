// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
    cprintf("pgfault user handler\n");
    void *addr = (void *) utf->utf_fault_va;
    uint32_t err = utf->utf_err;

    // Check that the faulting access was (1) a write, and (2) to a
    // copy-on-write page.  If not, panic.
    // Hint:
    //   Use the read-only page table mappings at uvpt
    //   (see <inc/memlayout.h>).

    // LAB 4: Your code here.
    unsigned pn = (uintptr_t)addr / PGSIZE;
    pte_t pte = uvpt[pn];
    int perms = pte ^ PTE_ADDR(pte);
    bool fault_for_cow = (err & FEC_WR) && (perms & PTE_COW);
    if (!fault_for_cow) {
        panic("pgfault is not a copy on write effect");
    }

    // Allocate a new page, map it at a temporary location (PFTEMP),
    // copy the data from the old page to the new page, then move the new
    // page to the old page's address.
    // Hint:
    //   You should make three system calls.

    // LAB 4: Your code here.
    void *bottom = (void*)(pn * PGSIZE);
    // Allocate a new page for our use.
    sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W);
    // Copy from the read only page into our new page.
    memcpy(PFTEMP, bottom, PGSIZE);
    // Unmap the read only page.
    sys_page_unmap(0, bottom);
    // Map in our new page.
    sys_page_map(0, PFTEMP, 0, bottom, (perms ^ ~PTE_COW) | PTE_W);
    // Unmap the temporary location.
    sys_page_unmap(0, PFTEMP);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
    // LAB 4: Your code here.
    pte_t pte = uvpt[pn];
    int perms = pte ^ PTE_ADDR(pte);
    bool present = 0 != (perms & PTE_P);
    bool mark_cow = 0 != (perms & PTE_W || perms & PTE_COW);
    void *va = (void*)(pn * PGSIZE);

    if (present && mark_cow) {
        // Register writeable or COW pages as COW.
        cprintf("duppage cow\n");
        sys_page_map(0, va, envid, va, (perms & ~PTE_W) | PTE_COW);
        cprintf("duppage almost done\n");
    } else if (present) {
        // Copy mapping only.
        cprintf("duppage map\n");
        sys_page_map(0, va, envid, va, perms);
        cprintf("duppage almost done\n");
    } else {
        // Don't map anything for non-present pages.
    }
    return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
    set_pgfault_handler(pgfault);

    cprintf("fork\n");
    envid_t child;
    if ((child = sys_exofork()) < 0) {
        panic("sys_exofork: %e", child);
    }
    if (child == 0) {
        // We're the child.
        // The copied value of the global variable 'thisenv'
        // is no longer valid (it refers to the parent!).
        // Fix it and return 0.
        thisenv = &envs[ENVX(sys_getenvid())];
        // I'm not sure if this is necessary, but it shouldn't hurt.
        set_pgfault_handler(pgfault);
        cprintf("fork-return child\n");
        return 0;
    }

    // Set up child mappings.
    int pn;
    for (pn = 0; pn < UTOP / PGSIZE; pn++) {
        if (pn == UXSTACKTOP/PGSIZE - 1) {
            // User exception stack gets a new page no matter what.
            sys_page_alloc(0, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
        } else {
            // Lazy-duplicate all other mappings
            duppage(child, pn);
        }
    }

    // Finished setting up child mappings, mark child as runnable.
    sys_env_set_status(child, ENV_RUNNABLE);
    return child;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
