// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Exit from the monitor", mon_backtrace },
	{ "showmappings", "Show memory mappings for virtual range", mon_showmappings },
	{ "sm", "aliased to showmappings", mon_showmappings },
	{ "exit", "Exit from the monitor", mon_exit },
};
#define NCOMMANDS (sizeof(commands)/sizeof(commands[0]))

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < NCOMMANDS; i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_exit(int argc, char **argv, struct Trapframe *tf)
{
	cprintf("Goodbye.\n");
	return -1;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
    uint32_t *base_pointer = (uint32_t*)read_ebp();
    uint32_t instruction_pointer;
    uint32_t pargv[5]; // frame arguments to print
    struct Eipdebuginfo frame_info;
    int i;

    cprintf("Stack backtrace:\n");
    while (base_pointer != 0) {
        instruction_pointer = *(base_pointer + 1);
        // Extract the arguments
        for (i = 0; i < 5; i++) {
            pargv[i] = *(base_pointer + i + 2);
        };

        // Basic info
        cprintf("  ebp %x  eip %x  args %8.0x %8.0x %8.0x %8.0x %8.0x\n",
            base_pointer, instruction_pointer,
            pargv[0], pargv[1], pargv[2], pargv[3], pargv[4]);

        // Symbolic info
        i = debuginfo_eip(instruction_pointer, &frame_info);
        if (i == 0) {
            cprintf("    %s:%d: %.*s+%d\n",
                    frame_info.eip_file,
                    frame_info.eip_line,
                    frame_info.eip_fn_namelen,
                    frame_info.eip_fn_name,
                    instruction_pointer - frame_info.eip_fn_addr);
        }

        // Advance to next frame.
        base_pointer = (uint32_t*)*base_pointer;
    }
	return 0;
}

static void mon_showmappings_entry(void *va) {
    pte_t *pgtable_entry = pgdir_walk(kern_pgdir, va, false);
    if (!pgtable_entry) {
        // VA has no page table.
        cprintf("%08p  -no-table-  U:- W:-\n", va);
    } else {
        physaddr_t pa_wflags = *pgtable_entry;
        if (!(pa_wflags & PTE_P)) {
            // VA has no page.
            cprintf("%08p  -no-page-  U:- W:-\n", va);
        } else {
            // Page mapping found.
            cprintf("%08p  %08p  U:%d W:%d\n",
                va,
                PTE_ADDR(pa_wflags),
                !!(pa_wflags & PTE_U),
                !!(pa_wflags & PTE_W));
        }
    }
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
    // Make sure there are two args.
    if (!(2 <= argc && argc <= 3)) {
        cprintf("Show virtual memory mappings.\n");
        cprintf("Remember that the arguments are virtual addresses, ");
        cprintf("not virtual page numbers.\n\n");
        cprintf("Usage:\n");
        cprintf("showmappings <addr>\n");
        cprintf("showmappings <low_addr> <high_addr>\n");
        return 0;
    }

    // Read arguments
    char *lo_str = argv[1];
    char *hi_str = argv[2];
    // end ptr used as failure indicator for strtol
    char *arg_end;
    uintptr_t va_lo = 0;
    uintptr_t va_hi = 0;
    va_lo = strtol(lo_str, &arg_end, 16);
    // Read first argument.
    if (arg_end == lo_str) {
        cprintf("Error: First argument must be a hex number.\n");
        return 0;
    }
    // Possibly read second argument.
    if (argc > 2) {
        va_hi = strtol(hi_str, &arg_end, 16);
        if (arg_end == hi_str) {
            cprintf("Error: Argument <high_addr> must be a hex number.\n");
            return 0;
        }
        if (va_hi < va_lo) {
            cprintf("Error: Argument <high_addr> must be greater than <low_addr>\n");
            return 0;
        }
    } else {
        // Only one argument supplied.
        va_hi = va_lo;
    }

    // Table header.
    cprintf("VA          PA          PERMS\n");

    // Entries.
    uintptr_t va = ROUNDDOWN(va_lo, PGSIZE);
    for (; va <= va_hi; va += PGSIZE) {
        mon_showmappings_entry((void*)va);
    }

    return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < NCOMMANDS; i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
