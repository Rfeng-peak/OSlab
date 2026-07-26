/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0xffffffc050500000

.section ".entry_function","ax"
ENTRY(_start)
  /* If we are secondary (mhartid != 0), skip BSS clear
   * and jump directly to secondary entry.
   */
  csrr t0, CSR_MHARTID
  bnez t0, secondary_start

  /* Mask all interrupts */
  csrw CSR_SIE, zero
  csrw CSR_SIP, zero

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
  /* Secondary hart: BSS already cleared by primary.
   * Just set up its own stack and call main.
   */
  li sp, KERNEL_STACK + 0x2000  /* separate stack for secondary */
  call main


loop:
  wfi
  j loop

END(_start)
