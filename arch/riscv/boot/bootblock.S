#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001fc

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence
    // a0 is mhartid
	bnez a0, secondary

	// set up a temporary stack for BIOS calls in boot stage
	li sp, 0x50501000

	// [p1-task1] print bootloader message through BIOS
	la a0, msg
	li a1, 0
	li a2, 0
	li a3, 0
	li a4, 0
	li a5, 0
	li a6, 0
	li a7, BIOS_PUTSTR
	li t0, bios_func_entry
	jalr ra, 0(t0)


	// [p1-task2] read kernel sectors from SD card into memory
	li t0, os_size_loc
	lhu t0, 0(t0)
	li a0, kernel
	mv a1, t0
	li a2, 1
	li a3, 0
	li a4, 0
	li a5, 0
	li a6, 0
	li a7, BIOS_SDREAD
	li t1, bios_func_entry
	jalr ra, 0(t1)


	// TODO: [p1-task4] load task-related arguments and pass them to kernel

	// mask all interrupts before jumping to kernel
	// NOTE: U-Boot's enter_supervisor_mode already left sstatus.SIE=0,
	// and csrw sie can be illegal here due to a dual-hart ugp race.
	// Just ensure SIP is clear with a NOP chain.
	nop
	nop

	// ensure instruction cache coherence after loading kernel
	fence.i

	// [p1-task2] jump to kernel to start UCAS-OS
	li t0, kernel
	jr t0

	/************************************************************/
secondary:

	/* P3-task3: initialize secondary hart
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to exception_handler_entry
	 *    (hardcoded kernel+0x6c8; must match kernel's exception_handler_entry)
	 * 3. enable supervisor software interrupt for ipi
	 */
	/* mask all interrupts */
	csrw CSR_SIE, zero

	/* clear any stale pending interrupts (e.g. leftover IPI from previous boot) */
	csrw CSR_SIP, zero

	/* DEBUG: step1 — SIE/SIP masked */
	li t0, 0x50200108
	li t1, 0xBEE1
	sd t1, 0(t0)

	/* set stvec to exception_handler_entry (kernel + 0x6c8) */
	li t0, kernel + 0x6c8
	csrw CSR_STVEC, t0

	/* DEBUG: step2 — stvec set to exception_handler_entry */
	li t0, 0x50200110
	li t1, 0xBEE2
	sd t1, 0(t0)

	/* enable supervisor software interrupt (SSIE) to receive IPIs */
	li t0, SIE_SSIE
	csrs CSR_SIE, t0

	/* DEBUG: step3 — SSIE enabled */
	li t0, 0x50200118
	li t1, 0xBEE3
	sd t1, 0(t0)


	/* DEBUG: step4 — entering busy-wait loop (SSTATUS.SIE not set) */
	li t0, 0x50200120
	li t1, 0xBEE4
	sd t1, 0(t0)

	// Busy-wait on wake flag (hardcoded addr 0x50200100)
	li t0, 0x50200100
wait_for_wakeup:
	ld t1, 0(t0)
	beqz t1, wait_for_wakeup
	// flag is set - jump to kernel entry
	la t0, kernel
	fence.i
	jr t0

	/************************************************************/
stop:
	j stop
	nop

.data

msg: .string "It's bootblock!\n\r"

.align 3
secondary_wake_flag:
	.dword 0

