/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


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
#define TRAPHANDLER(name, num) ;\
  .text ;\
    .globl name ;\
    /* symbol type is function */ ;\
    .type name, @function ;\
    /* align function definition */ ;\
    .align 2 ;\
    /* function starts here */ ;\
    name: ;\
    pushl $(num) ;\
    jmp _alltraps ;\
  .data ;\
    .long name

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num) ;\
  .text ;\
    .globl name ;\
    .type name, @function ;\
    .align 2 ;\
    name: ;\
    pushl $0 ;\
    pushl $(num) ;\
    jmp _alltraps ;\
  .data ;\
    .long name

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 * See http://pdosnew.csail.mit.edu/6.828/2014/readings/ia32/IA32-3A.pdf
 * page 185 for a table of which interrupts have error code.
 */
.data
.globl trap_handlers
trap_handlers:
TRAPHANDLER_NOEC(trap_DIVIDE, T_DIVIDE)    // 0 divide error
TRAPHANDLER_NOEC(trap_DEBUG, T_DEBUG)      // 1 debug exception
TRAPHANDLER_NOEC(trap_NMI, T_NMI)          // 2 non-maskable interrupt
TRAPHANDLER_NOEC(trap_BRKPT, T_BRKPT)      // 3 breakpoint
TRAPHANDLER_NOEC(trap_OFLOW, T_OFLOW)      // 4 overflow
TRAPHANDLER_NOEC(trap_BOUND, T_BOUND)      // 5 bounds check
TRAPHANDLER_NOEC(trap_ILLOP, T_ILLOP)      // 6 illegal opcode
TRAPHANDLER_NOEC(trap_DEVICE, T_DEVICE)    // 7 device not available
TRAPHANDLER(trap_DBLFLT, T_DBLFLT)         // 8 double fault
.long 0 // 9
TRAPHANDLER(trap_TSS, T_TSS)               // 10 invalid task switch segment
TRAPHANDLER(trap_SEGNP, T_SEGNP)           // 11 segment not present
TRAPHANDLER(trap_STACK, T_STACK)           // 12 stack exception
TRAPHANDLER(trap_GPFLT, T_GPFLT)           // 13 general protection fault
TRAPHANDLER(trap_PGFLT, T_PGFLT)           // 14 page fault
TRAPHANDLER(trap_NOOP, 15)
TRAPHANDLER_NOEC(trap_FPERR, T_FPERR)      // 16 floating point error
TRAPHANDLER(trap_ALIGN, T_ALIGN)           // 17 aligment check
TRAPHANDLER_NOEC(trap_MCHK, T_MCHK)        // 18 machine check
TRAPHANDLER_NOEC(trap_SIMDERR, T_SIMDERR)  // 19 SIMD floating point error
// Oh boy, do I regret doing this challenge. What a mess.
.space (20 - 31)*-4+4 // 20 through 31
TRAPHANDLER_NOEC(trap_IRQ_TIMER, IRQ_OFFSET+IRQ_TIMER)       // 32 IRQ_TIMER
TRAPHANDLER_NOEC(trap_IRQ_KBD, IRQ_OFFSET+IRQ_KBD)           // 33 IRQ_KBD
.space (34 - 35)*-4+4 // 34 through 35
TRAPHANDLER_NOEC(trap_IRQ_SERIAL, IRQ_OFFSET+IRQ_SERIAL)     // 36 IRQ_SERIAL
.space (37 - 38)*-4+4 // 37 through 38
TRAPHANDLER_NOEC(trap_IRQ_SPURIOUS, IRQ_OFFSET+IRQ_SPURIOUS) // 39 IRQ_SPURIOUS
.space (40 - 45)*-4+4 // 40 through 45
TRAPHANDLER_NOEC(trap_IRQ_IDE, IRQ_OFFSET+IRQ_IDE)           // 46 IRQ_IDE
.space (47 - 47)*-4+4 // 47 through 47
TRAPHANDLER_NOEC(trap_SYSCALL, T_SYSCALL)                     // 48 JOS system call
.space (49 - 50)*-4+4 // 49 through 50
TRAPHANDLER_NOEC(trap_IRQ_ERROR, IRQ_OFFSET+IRQ_ERROR)       // 51 IRQ_ERROR

/*
 * Lab 3: Your code here for _alltraps
 */
.text
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
