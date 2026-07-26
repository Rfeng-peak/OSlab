#include <atomic.h>
#include <os/sched.h>
#include <os/smp.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/irq.h>

void lock_kernel() { /* SMP-only, no-op for single-CPU */ }
void unlock_kernel() { /* SMP-only, no-op for single-CPU */ }
void wakeup_other_hart() { /* TODO: P3-TASK3 */ }
void smp_init() { /* TODO: P3-TASK3 */ }
