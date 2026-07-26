#include <asm/biosdef.h>

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


	// [p1-task2] jump to kernel to start UCAS-OS
	li t0, kernel
	jr t0

	/************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) --> stop here
stop:
	j stop
	nop

.data

msg: .string "It's bootblock!\n\r"

