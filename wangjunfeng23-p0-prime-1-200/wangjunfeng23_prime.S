.section .text
.global main

#define PRIME_RESULT_BASE 0x50002000

main:
    li s0, 1
    li s1, 200

    la s2, prime_result_base_ptr
    ld s2, 0(s2)

main_loop:
    mv a0, s0
    jal ra, is_prime

    sb a0, 0(s2)

    addi s0, s0, 1
    addi s2, s2, 1
    ble s0, s1, main_loop

stop:
    j stop 

# a0 = n, return a0 = 1 (prime) or 0 (not prime)
is_prime:
    li t0, 2
    blt a0, t0, not_prime

    li t1, 2

check_divisor:
    mul t2, t1, t1
    bgt t2, a0, prime

    rem t3, a0, t1
    beqz t3, not_prime

    addi t1, t1, 1
    j check_divisor

prime:
    li a0, 1
    ret

not_prime:
    li a0, 0
    ret

.section .rodata
.align 3
prime_result_base_ptr:
    .dword PRIME_RESULT_BASE
