#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <os/smp.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
extern volatile uint64_t secondary_alive_flag;
#include <assert.h>
#include <type.h>
#include <csr.h>

extern void ret_from_exception();

// Task info array
task_info_t tasks[TASK_MAXNUM];


static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
    jmptab[SD_WRITE]        = (long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (long (*)())set_timer;
    jmptab[READ_FDT]        = (long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (long (*)())screen_move_cursor;
    jmptab[PRINT]           = (long (*)())printk;
    jmptab[YIELD]           = (long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (long (*)())do_mutex_lock_release;

    // TODO: [p2-task1] (S-core) initialize system call table.

}

static void init_task_info(void)
{
    // Initialize task name mapping according to image build order.
    const char *names[] = {
        "shell", "add", "affinity", "barrier", "condition",
        "consumer", "mbox_client", "mbox_server", "multicore",
        "producer", "ready_to_exit", "test_affinity", "test_barrier",
        "wait_locks", "waitpid"
    };
    int n = sizeof(names) / sizeof(names[0]);
    for (int i = 0; i < TASK_MAXNUM; i++) {
        if (i < n) {
            strncpy(tasks[i].name, names[i], sizeof(tasks[i].name) - 1);
            tasks[i].name[sizeof(tasks[i].name) - 1] = '\0';
            tasks[i].id = i;
        } else {
            tasks[i].name[0] = '\0';
            tasks[i].id = -1;
        }
    }
}

/************************************************************/
static void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
     /* TODO: [p2-task3] initialization of registers on kernel stack
      * HINT: sp, ra, sepc, sstatus
      * NOTE: To run the task in user mode, you should set corresponding bits
      *     of sstatus(SPP, SPIE, etc.).
      */
    regs_context_t *pt_regs =
        (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    {
        volatile char *p = (volatile char *)pt_regs;
        for (int i = 0; i < (int)sizeof(regs_context_t); i++)
            p[i] = 0;
    }
    pt_regs->regs[2] = user_stack;
    pt_regs->regs[4] = (reg_t)pcb;
    pt_regs->sepc = entry_point;
    pt_regs->sstatus = SR_SPIE;


    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto =
        (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    {
        volatile char *p = (volatile char *)pt_switchto;
        for (int i = 0; i < (int)sizeof(switchto_context_t); i++)
            p[i] = 0;
    }
    pt_switchto->regs[0] = (reg_t)ret_from_exception;
    pt_switchto->regs[1] = (reg_t)pt_switchto;

    pcb->kernel_sp = (ptr_t)pt_switchto;
    pcb->user_sp = user_stack;
    init_list_head(&pcb->wait_list);

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    int task_num = 1;
    for (int i = 0; i < task_num; i++) {
        ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
        ptr_t user_stack = allocUserPage(1) + PAGE_SIZE;
        ptr_t entry_point = load_task_img(i);

        pcb[i].pid = process_id++;
        pcb[i].status = TASK_READY;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = i;
        init_pcb_stack(kernel_stack, user_stack, entry_point, &pcb[i]);

        init_list_head(&pcb[i].list);
        list_add_tail(&pcb[i].list, &ready_queue);
    }


    /* TODO: [p2-task1] remember to initialize 'current_running' */
    init_list_head(&pid0_pcb.list);
    pid0_pcb.status = TASK_RUNNING;
    pid0_pcb.cursor_x = 0;
    pid0_pcb.cursor_y = 0;
    current_running = &pid0_pcb;

}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP] = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD] = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE] = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR] = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH] = (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE] = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK] = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT] = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ] = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE] = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_SEMA_INIT] = (long (*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP] = (long (*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN] = (long (*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY] = (long (*)())do_semaphore_destroy;
    syscall[SYSCALL_EXEC] = (long (*)())do_exec;
    syscall[SYSCALL_EXIT] = (long (*)())do_exit;
    syscall[SYSCALL_KILL] = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
    syscall[SYSCALL_PS] = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
    syscall[SYSCALL_READCH] = (long (*)())bios_getchar;
    syscall[SYSCALL_CLEAR] = (long (*)())screen_clear;
    syscall[SYSCALL_SHOW_TASK] = (long (*)())do_process_show;
    syscall[SYSCALL_BARR_INIT] = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT] = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY] = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_COND_INIT] = (long (*)())do_condition_init;
    syscall[SYSCALL_COND_WAIT] = (long (*)())do_condition_wait;
    syscall[SYSCALL_COND_SIGNAL] = (long (*)())do_condition_signal;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())do_condition_broadcast;
    syscall[SYSCALL_COND_DESTROY] = (long (*)())do_condition_destroy;
    syscall[SYSCALL_MBOX_OPEN] = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE] = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND] = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV] = (long (*)())do_mbox_recv;
}
/************************************************************/

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();

    // Prepare idle PCBs for multi-core scheduling
    init_idle_pcbs();

    // Read CPU frequency (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    // Wake up secondary core (P3-task3)
    {
        volatile uint64_t *dbg0 = (uint64_t *)0x50200100;
        volatile uint64_t *dbg1 = (uint64_t *)0x50200108;
        volatile uint64_t *dbg2 = (uint64_t *)0x50200110;
        volatile uint64_t *dbg3 = (uint64_t *)0x50200118;
        volatile uint64_t *dbg4 = (uint64_t *)0x50200120;
        volatile uint64_t *dbg5 = (uint64_t *)0x50200128;
        volatile uint64_t *dbg6 = (uint64_t *)0x50200130;
        volatile uint64_t *dbg7 = (uint64_t *)0x50200140;
        volatile uint64_t *ss = (uint64_t *)0x50200150;
        printk("> [DBG] bb:%lx %lx %lx %lx %lx _s:%lx %lx s2:%lx ss:%lx\n",
               *dbg0, *dbg1, *dbg2, *dbg3, *dbg4, *dbg5, *dbg6, *dbg7, *ss);
    }
    smp_init();
    {
        volatile uint64_t *ss = (uint64_t *)0x50200150;
        volatile uint64_t *sc = (uint64_t *)0x50200158;
        printk("> [INIT] SMP secondary hart awakened (alive=0x%lx, ss=0x%lx, sched=0x%lx).\n",
               secondary_alive_flag, *ss, *sc);
    }

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    printk("> [DBG] about to set timer\n");
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    printk("> [DBG] timer set ok\n");
    enable_interrupt();
    printk("> [DBG] interrupt enabled\n");
    enable_preempt();
    printk("> [DBG] preempt enabled, about to schedule\n");
    do_scheduler();


    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        do_scheduler();
        asm volatile("wfi");
    }

    return 0;
}
