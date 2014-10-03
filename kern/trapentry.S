/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(trap_DIVIDE, T_DIVIDE)    // divide error
TRAPHANDLER_NOEC(trap_DEBUG, T_DEBUG)      // debug exception
TRAPHANDLER_NOEC(trap_NMI, T_NMI)          // non-maskable interrupt
TRAPHANDLER_NOEC(trap_BRKPT, T_BRKPT)      // breakpoint
TRAPHANDLER_NOEC(trap_OFLOW, T_OFLOW)      // overflow
TRAPHANDLER_NOEC(trap_BOUND, T_BOUND)      // bounds check
TRAPHANDLER_NOEC(trap_ILLOP, T_ILLOP)      // illegal opcode
TRAPHANDLER_NOEC(trap_DEVICE, T_DEVICE)    // device not available
TRAPHANDLER(trap_DBLFLT, T_DBLFLT)         // double fault
TRAPHANDLER(trap_TSS, T_TSS)               // invalid task switch segment
TRAPHANDLER(trap_SEGNP, T_SEGNP)           // segment not present
TRAPHANDLER(trap_STACK, T_STACK)           // stack exception
TRAPHANDLER(trap_GPFLT, T_GPFLT)           // general protection fault
TRAPHANDLER(trap_PGFLT, T_PGFLT)           // page fault
TRAPHANDLER_NOEC(trap_FPERR, T_FPERR)      // floating point error
TRAPHANDLER(trap_ALIGN, T_ALIGN)           // aligment check
TRAPHANDLER_NOEC(trap_MCHK, T_MCHK)        // machine check
TRAPHANDLER_NOEC(trap_SIMDERR, T_SIMDERR)  // SIMD floating point error


/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
    // construct Trapframe on stack
    // this is the structure of Trapframe
    // 32 reg_edi,   reg_esi,     reg_ebp, reg_oesp
    // 32 reg_ebx,   reg_edx,     reg_ecx, reg_eax
    // 16 tf_es,     tf_padding1, tf_ds,   tf_padding2
    // 32 tf_trapno, tf_err,      tf_eip
    // 16 tf_cs,     tf_padding3
    // 32 tf_eflags, tf_esp
    // 16 tf_ss,     tf_padding4

    // 32 tf_trapno
    // TODO heuh?
    // 16 tf_padding2
    // 16 tf_ds
    pushl %ds
    // 16 tf_padding1
    // 16 tf_es
    pushl %es
    // pusha pushes these: A, C, D, B, S, B, S, D
    pushal

    // put GD_KD (kernel data) into ds and es segment regs
    movw $GD_KD, %ax
    movw %ax, %ds
    movw %ax, %es
    // pass pointer to Trapframe as C arg.
    pushl %esp
    // call trap
    call trap
    // TODO can trap ever return?
