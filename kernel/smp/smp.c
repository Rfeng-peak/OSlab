#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/irq.h>
#include <os/time.h>
#include <printk.h>
#include <os/mm.h>
#include <common.h>

static uint64_t kernel_lock_var = 0;

volatile uint64_t secondary_alive_flag = 0;
volatile uint64_t secondary_trap_count = 0;

void lock_kernel()
{
    while (atomic_swap_d(1, (ptr_t)&kernel_lock_var) != 0) {
        asm volatile ("" : : : "memory");
    }
}

void unlock_kernel()
{
    atomic_swap_d(0, (ptr_t)&kernel_lock_var);
}

/* Separate idle PCB for secondary hart so it does not share pid0_pcb.kernel_sp */
#define SECONDARY_STACK 0x50502000
pcb_t secondary_idle = {
    .pid = 0,
    .kernel_sp = SECONDARY_STACK + PAGE_SIZE,
    .user_sp = SECONDARY_STACK + PAGE_SIZE,
};

static void sbi_clear_ipi()
{
    /* Clear supervisor software interrupt pending bit directly.
     * The M-mode handler already cleared MSIP at the CLINT level;
     * we just need to clear SSIP so it doesn't re-fire after enable_interrupt.
     */
    asm volatile ("csrc sip, %0" : : "r"(0x2UL));
}

void wakeup_other_hart()
{
    lock_kernel();
    unsigned long hart_mask = (get_current_cpu_id() == 0) ? 2 : 1;
    send_ipi(&hart_mask);
    unlock_kernel();
}

void smp_init()
{
    /* Set secondary wake flag in bootblock's .data section.
     * The secondary hart polls this flag in a wfi loop and jumps
     * to kernel entry when it becomes non-zero.
     * We do NOT send IPI because the M-mode trap handler corrupts
     * the secondary's GP via the shared ugp variable. */
    volatile uint64_t *wake_flag = (uint64_t *)0x50200100;
    *wake_flag = 1;
    printk("> [SMP] wrote wake_flag=0x%lx, readback=0x%lx\n", (uint64_t)1, *wake_flag);
}

void smp_secondary_main()
{
    secondary_alive_flag = 0xDEAD;
    current_running = &secondary_idle;
    secondary_alive_flag = 0xBEEF;
    volatile uint64_t *ss = (uint64_t *)0x50200150;
    *ss = 0x5500;  // entered smp_secondary_main

    setup_exception();
    *ss = 0x5501;  // setup_exception done

    /* skip bios_read_fdt — time_base already set by primary */
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    *ss = 0x5502;  // timer set

    sbi_clear_ipi();
    *ss = 0x5503;

    enable_interrupt();
    *ss = 0x5504;

    enable_preempt();
    *ss = 0x5505;

    volatile uint64_t *sched_cnt = (uint64_t *)0x50200158;
    while (1) {
        do_scheduler();
        (*sched_cnt)++;
        asm volatile("wfi");
    }
}
