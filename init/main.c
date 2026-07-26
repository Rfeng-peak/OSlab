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
#include <os/ioremap.h>
#include <sys/syscall.h>
#include <os/net.h>
#include <screen.h>
#include <e1000.h>
#include <plic.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>
#include <os/smp.h>

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

#define TASK_INFO_ADDR pa2kva(0x50200000)
#define TASK_INFO_OFFSET 0x100
#define TASK_NAME_LEN 32

static void init_task_info(void)
{
    /* Read task names from boot sector (identity-mapped at 0x50200000) */
    uint8_t count = *(uint8_t *)(TASK_INFO_ADDR + TASK_INFO_OFFSET);
    if (count > TASK_MAXNUM)
        count = TASK_MAXNUM;
    char *ptr = (char *)(TASK_INFO_ADDR + TASK_INFO_OFFSET + 1);
    for (int i = 0; i < (int)count; i++) {
        strncpy(tasks[i].name, ptr, TASK_NAME_LEN - 1);
        tasks[i].name[TASK_NAME_LEN - 1] = '\0';
        tasks[i].id = i;
        ptr += TASK_NAME_LEN;
    }
}

/************************************************************/
void init_pcb_stack(
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
    memset(pt_regs, 0, sizeof(regs_context_t));
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
    memset(pt_switchto, 0, sizeof(switchto_context_t));
    pt_switchto->regs[0] = (reg_t)ret_from_exception;
    pt_switchto->regs[1] = (reg_t)pt_switchto;

    pcb->kernel_sp = (ptr_t)pt_switchto;
    pcb->user_sp = user_stack;

}

static void init_pcb(void)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */

    /* For P4-Task1 we share the kernel page directory.  Later tasks
     * will give each process its own pgdir.
     */
    uintptr_t shared_pgdir = PGDIR_PA;

    int task_num = 1; /* start with just the shell */
    for (int i = 0; i < task_num; i++) {
        ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
        /* user space pre-mapped as 2MB large pages by setup_vm */
        ptr_t user_stack = USER_STACK_ADDR + PAGE_SIZE;
        ptr_t entry_point = load_task_img(i, shared_pgdir);

        pcb[i].pid = process_id++;
        pcb[i].status = TASK_READY;
        pcb[i].cursor_x = 0;
        pcb[i].cursor_y = i;
        pcb[i].pgdir = shared_pgdir;
        init_pcb_stack(kernel_stack, user_stack, entry_point, &pcb[i]);

        init_list_head(&pcb[i].list);
        init_list_head(&pcb[i].wait_list);
        list_add_tail(&pcb[i].list, &ready_queue);
    }


    /* TODO: [p2-task1] remember to initialize 'current_running' */
    init_list_head(&pid0_pcb.list);
    pid0_pcb.status = TASK_RUNNING;
    pid0_pcb.cursor_x = 0;
    pid0_pcb.cursor_y = 0;
    pid0_pcb.pgdir = PGDIR_PA;
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
    syscall[SYSCALL_EXEC] = (long (*)())do_exec;
    syscall[SYSCALL_EXIT] = (long (*)())do_exit;
    syscall[SYSCALL_KILL] = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID] = (long (*)())do_waitpid;
    syscall[SYSCALL_PS] = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID] = (long (*)())do_getpid;
    syscall[SYSCALL_READCH] = (long (*)())bios_getchar;
    syscall[SYSCALL_CLEAR] = (long (*)(long, long, long, long, long))screen_clear;
    syscall[SYSCALL_SHOW_TASK] = (long (*)())do_process_show;
    syscall[SYSCALL_NET_SEND] = (long (*)())do_net_send;
    syscall[SYSCALL_NET_RECV] = (long (*)())do_net_recv;
}
/************************************************************/

static void kernel_brake(void)
{
    disable_interrupt();
    while (1)
        __asm__ volatile("wfi");
}

int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Init Process Control Blocks |•'-'•) ✧
    init_pcb();
    printk("> [INIT] PCB initialization succeeded.\n");

    // Read Flatten Device Tree (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);
    uint64_t e1000_phys = bios_read_fdt(ETHERNET_ADDR);
    uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
    uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
    printk("> [INIT] e1000: phys=0x%lx, plic_addr: 0x%lx, nr_irqs: 0x%x.\n",
           e1000_phys, plic_addr, nr_irqs);

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // [p5-task4] Init PLIC (Platform-Level Interrupt Controller)
    // // plic_init(plic_addr, nr_irqs);
    // printk("> [INIT] PLIC initialized. addr=0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

    // [p5-task2+4] Init E1000 network device
    // NOTE: requires `-device e1000` in QEMU.  Default `make run` has this;
    // for full networking (TAP + host pktRxTx) use `make run-net`.
    e1000 = (volatile uint8_t *)e1000_phys;
    if (e1000_phys != 0 && e1000_phys != (uint64_t)-1) {
        e1000_init();   // needs -device e1000 in QEMU (use `make run-net`)
        printk("> [INIT] E1000 initialized at 0x%lx\n", e1000_phys);
    }

    // Init system call table (0_0)
    init_syscall();
    printk("> [INIT] System call initialized successfully.\n");

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");

    printk("> [INIT] CPU #%u has entered kernel with VM!\n",
        (unsigned int)get_current_cpu_id());
    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
    // [p4-task1 cont.] start user processes under VM
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
    enable_interrupt();
    enable_preempt();
    asm volatile("fence.i" ::: "memory");
    do_scheduler();


    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    while (1) {
        bios_set_timer(get_ticks() + TIMER_INTERVAL);
    enable_interrupt();
    enable_preempt();
    do_scheduler();
        asm volatile("wfi");
    }

    return 0;
}
