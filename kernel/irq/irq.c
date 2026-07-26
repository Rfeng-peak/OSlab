#include <os/irq.h>
#include <os/time.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/kernel.h>
#include <printk.h>
#include <assert.h>
#include <screen.h>
#include <csr.h>

handler_t irq_table[IRQC_COUNT];
handler_t exc_table[EXCC_COUNT];

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    uint64_t code = scause & ~SCAUSE_IRQ_FLAG;

    if (scause & SCAUSE_IRQ_FLAG)
    {
        if (code < IRQC_COUNT && irq_table[code])
            irq_table[code](regs, stval, scause);
        else
            handle_other(regs, stval, scause);
    }
    else
    {
        if (code < EXCC_COUNT && exc_table[code])
            exc_table[code](regs, stval, scause);
        else
            handle_other(regs, stval, scause);
    }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // bios_set_timer uses hardcoded CLINT addresses (no GP needed), safe for both harts
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    do_scheduler();

    (void)regs;
    (void)stval;
    (void)scause;
}

void handle_irq_ssoft(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    /* Acknowledge supervisor software interrupt by clearing SSIP. */
    asm volatile ("csrc sip, %0" : : "r"(0x2UL));
    (void)stval;
    (void)scause;

    /* IPI means another hart put work on ready_queue.
     * If we are idle, schedule immediately so we don't sleep through the wakeup. */
    if (current_running->pid == 0) {
        do_scheduler();
    }
    (void)regs;
}

void init_exception()
{
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    for (int i = 0; i < EXCC_COUNT; i++)
        exc_table[i] = handle_other;
    exc_table[EXCC_SYSCALL] = handle_syscall;

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for (int i = 0; i < IRQC_COUNT; i++)
        irq_table[i] = handle_other;
    irq_table[IRQC_S_TIMER] = handle_irq_timer;
    irq_table[IRQC_S_SOFT] = handle_irq_ssoft;

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
