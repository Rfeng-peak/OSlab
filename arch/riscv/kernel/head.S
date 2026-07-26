/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000
#define SECONDARY_STACK		0x50502000

.section ".entry_function","ax"
ENTRY(_start)
  /* Identify this hart first — csrr mhartid traps to M-mode but
   * is safely emulated. Secondary skips CSR writes and BSS clear.
   */
  csrr t0, CSR_MHARTID
  bnez t0, secondary_start

  /* Primary: setup stvec and sie (CSR writes work on primary) */
  la t0, exception_handler_entry
  csrw CSR_STVEC, t0
  li t0, SIE_STIE
  csrs CSR_SIE, t0

  /* DEBUG: mark entry */
  li t0, 0x50200128
  li t1, 0xC001
  sd t1, 0(t0)
  csrr t1, CSR_MHARTID
  slli t1, t1, 3
  add t0, t0, t1
  li t1, 0xC002
  sd t1, 0(t0)

  /* [p1-task2] clear BSS for flat non-ELF images */
  la t0, __bss_start
  la t1, __BSS_END__

1:
  bgeu t0, t1, 2f
  sd zero, 0(t0)
  addi t0, t0, 8
  j 1b


2:
  /* [p1-task2] setup C environment */
  li sp, KERNEL_STACK
  call main


  /* never reaches here */

secondary_start:
  /* stvec and sie already set in bootblock secondary path.
   * Skip CSR writes here — they trap as illegal instruction
   * on the secondary for an as-yet-unknown reason.
   */
  /* DEBUG: secondary reached here */
  li t0, 0x50200140
  li t1, 0xC003
  sd t1, 0(t0)

  /* Secondary hart: do NOT clear BSS (primary already did) */
  /* separate stack to avoid corrupting primary */
  li sp, SECONDARY_STACK

  call smp_secondary_main
  /* never returns */

loop:
  wfi
  j loop

END(_start)
