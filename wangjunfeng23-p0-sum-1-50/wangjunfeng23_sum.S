.section .text
.global main

#define RESULT_ADDR 0x50001000

main:
    li t0, 1
    li t1, 50
    li t2, 0

sum_loop:
    add t2, t2, t0
    addi t0, t0, 1
    ble t0, t1, sum_loop

    la t3, result_addr_ptr
    ld t3, 0(t3)
    sd t2, 0(t3)

stop:
    j stop 

.section .rodata
.align 3
result_addr_ptr:
    .dword RESULT_ADDR
