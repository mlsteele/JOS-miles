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
    void *bottom = (void*)(pn * PGSIZE);
    pte_t pte = uvpt[pn];
    int perms = pte ^ PTE_ADDR(pte);
    bool fault_for_cow = (err & FEC_WR) && (perms & PTE_COW);
    if (!fault_for_cow) {
        panic("pgfault is not a copy on write effect @%p", bottom);
    }

    // Allocate a new page, map it at a temporary location (PFTEMP),
    // copy the data from the old page to the new page, then move the new
    // page to the old page's address.
    // Hint:
    //   You should make three system calls.

    // LAB 4: Your code here.
    // Allocate a new page for our use.
    sys_page_alloc(0, PFTEMP, PTE_P | PTE_U | PTE_W);
    // Copy from the read only page into our new page.
    memcpy(PFTEMP, bottom, PGSIZE);
    // Unmap the read only page. Is this optional or extra?
    sys_page_unmap(0, bottom);
    // Map in our new page.
    sys_page_map(0, PFTEMP, 0, bottom, PTE_P | PTE_U | PTE_W);
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
    void *va = (void*)(pn * PGSIZE);

    // Abort if page table is not present.
    pde_t pde = uvpd[pn / (PGSIZE / sizeof(pte_t))];
    if (!(pde & PTE_P)) {
        return 0;
    }

    // Analyze pte
    pte_t pte = uvpt[pn];
    int perms = pte ^ PTE_ADDR(pte);
    bool present = 0 != (perms & PTE_P);
    bool mark_cow = 0 != (perms & PTE_W || perms & PTE_COW);

    if (present && mark_cow) {
        // Register writeable or COW pages as COW.
        // cprintf("duppage cow @%p\n");
        sys_page_map(0, va, envid, va, PTE_P | PTE_U | PTE_COW); // for the child
        sys_page_map(0, va, 0, va, PTE_P | PTE_U | PTE_COW); // and for us, the parent
        // cprintf("duppage almost done\n");
    } else if (present) {
        // Copy read only mapping as read only.
        // cprintf("duppage map\n");
        sys_page_map(0, va, envid, va, PTE_P | PTE_U );
        // cprintf("duppage almost done\n");
    } else {
        // cprintf("duppage nop\n");
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

        cprintf("fork-return child\n");
        return 0;
    }

    // Set up child mappings.
    cprintf("setting up child mappings\n");
    unsigned pn;
    for (pn = 0; pn < UTOP / PGSIZE; pn++) {
        // cprintf("considering pn:%d va:%p\n", pn, pn * PGSIZE);
        if (pn == UXSTACKTOP/PGSIZE - 1) {
            // User exception stack gets a new page no matter what.
            cprintf("allocating UX stack\n");
            sys_page_alloc(child, (void*)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
        } else {
            // Lazy-duplicate all other mappings
            duppage(child, pn);
        }
    }


    // Install the child's page handler.
    sys_env_set_pgfault_upcall(child, thisenv->env_pgfault_upcall);

    // Mark child as runnable.
    cprintf("mark child as runnable\n");
    sys_env_set_status(child, ENV_RUNNABLE);
    cprintf("fork-return parent\n");
    return child;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
