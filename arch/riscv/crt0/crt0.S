#include <asm.h>

#define USER_STACK 0x52010000

.section ".entry_function","ax"
ENTRY(_start)

    /* [p1-task3] setup C runtime environment for the user program */
    mv s0, ra
    mv s1, sp
    li sp, USER_STACK

    la t0, __bss_start
    la t1, __BSS_END__

1:
    bgeu t0, t1, 2f
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b


2:
    /* [p1-task3] enter main function */
    call main


    /* [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    mv ra, s0
    mv sp, s1
    ret

    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
