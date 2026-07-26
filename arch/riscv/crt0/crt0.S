#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* [p1-task3] setup C runtime environment for the user program */
    mv s0, ra
    mv s1, sp

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


    /* [P3] return from main by exiting the task instead of falling through */
    call sys_exit

    /************************************************************/
    /* TODO: [P3] call sys_exit */
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
