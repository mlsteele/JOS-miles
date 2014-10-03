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



/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:

    ; construct Trapframe on stack
	; 32 reg_edi
	; 32 reg_esi
	; 32 reg_ebp
	; 32 reg_oesp
	; 32 reg_ebx
	; 32 reg_edx
	; 32 reg_ecx
	; 32 reg_eax
	; 16 tf_es
	; 16 tf_padding1
	; 16 tf_ds
	; 16 tf_padding2
	; 32 tf_trapno
	; 32 tf_err
	; 32 tf_eip
	; 16 tf_cs
	; 16 tf_padding3
	; 32 tf_eflags
	; 32 tf_esp
	; 16 tf_ss
	; 16 tf_padding4
    ; put GD_KD (kernel data) into ds and es segment regs
    GD_KD %ds
    GD_KD %es
    ; pass pointer to Trapframe as C arg.
    pushl %esp
    ; call trap
    call trap
    ; TODO can trap ever return?
