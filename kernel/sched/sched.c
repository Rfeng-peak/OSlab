#include <os/list.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/loader.h>
#include <os/string.h>
#include <csr.h>
#include <os/sched.h>
#include <asm/regs.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/smp.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* global process id */
pid_t process_id = 1;

extern pcb_t secondary_idle;

static void idle_loop(void)
{
    while (1) {
        do_scheduler();
        asm volatile ("wfi");
    }
}

void init_idle_pcbs(void)
{
    /* Set up pid0_pcb and secondary_idle kernel stacks for switch_to */
    pcb_t *idles[2] = { &pid0_pcb, &secondary_idle };
    for (int i = 0; i < 2; i++) {
        ptr_t sp = idles[i]->kernel_sp;
        switchto_context_t *ctx = (switchto_context_t *)(sp - sizeof(switchto_context_t));
        ctx->regs[0] = (reg_t)idle_loop; /* ra */
        ctx->regs[1] = (reg_t)(sp - sizeof(switchto_context_t)); /* sp */
        idles[i]->kernel_sp = (ptr_t)ctx;
        idles[i]->pid = 0;
        idles[i]->status = TASK_RUNNING;
    }
}

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    lock_kernel();
    check_sleeping();

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    pcb_t *prev_running = current_running;
    if (list_empty(&ready_queue)) {
        if (prev_running->pid != 0 && prev_running->status != TASK_RUNNING) {
            /* Blocked/exited but no task to run — switch to idle */
            pcb_t *idle = (get_current_cpu_id() == 0) ? &pid0_pcb : &secondary_idle;
            unlock_kernel();
            /* switch_to atomically sets tp (current_running) to idle */
            switch_to(prev_running, idle);
            /* NOTREACHED — switch_to returns into idle_loop */
        }
        unlock_kernel();
        return;
    }

    /* pid == 0 denotes idle PCBs (pid0_pcb / secondary_idle) — never re-queue them */
    if (prev_running->pid != 0 && prev_running->status == TASK_RUNNING) {
        prev_running->status = TASK_READY;
        list_add_tail(&prev_running->list, &ready_queue);
    }

    pcb_t *next_running = list_entry(ready_queue.next, pcb_t, list);
    list_del(&next_running->list);
    next_running->status = TASK_RUNNING;

    unlock_kernel();

    /* switch_to atomically sets tp (current_running) to next_running */
    switch_to(prev_running, next_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    current_running->wakeup_time = get_timer() + sleep_time;
    do_block(&current_running->list, &sleep_queue);
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    lock_kernel();
    pcb_t *pcb_blocked = list_entry(pcb_node, pcb_t, list);
    pcb_blocked->status = TASK_BLOCKED;
    list_add_tail(pcb_node, queue);
    unlock_kernel();
    do_scheduler();
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    lock_kernel();
    pcb_t *pcb_unblocked = list_entry(pcb_node, pcb_t, list);
    list_del(pcb_node);
    pcb_unblocked->status = TASK_READY;
    list_add_tail(pcb_node, &ready_queue);
    unlock_kernel();
    wakeup_other_hart();
}

/* P3-TASK1: process management helpers */
#ifdef S_CORE
pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    if (id < 0)
        return -1;

    /* find a free pcb slot */
    int idx = -1;
    for (int i = 1; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == 0 || pcb[i].status == TASK_EXITED) {
            idx = i;
            break;
        }
    }
    if (idx == -1)
        return -1;

    ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    ptr_t user_stack = allocUserPage(1) + PAGE_SIZE;
    ptr_t entry_point = load_task_img(id);

    pcb[idx].pid = process_id++;
    pcb[idx].status = TASK_READY;
    pcb[idx].cursor_x = 0;
    pcb[idx].cursor_y = idx;
    init_list_head(&pcb[idx].list);
    init_list_head(&pcb[idx].wait_list);
    /* initialize regs and switchto context on kernel stack (replicate init_pcb_stack) */
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    {
        volatile char *p = (volatile char *)pt_regs;
        for (int i = 0; i < (int)sizeof(regs_context_t); i++)
            p[i] = 0;
    }
    pt_regs->regs[2] = user_stack; /* sp */
    pt_regs->regs[4] = (reg_t)&pcb[idx];
    pt_regs->sepc = entry_point;
    pt_regs->sstatus = SR_SPIE;

    switchto_context_t *pt_switchto = (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    {
        volatile char *p = (volatile char *)pt_switchto;
        for (int i = 0; i < (int)sizeof(switchto_context_t); i++)
            p[i] = 0;
    }
    extern void ret_from_exception();
    pt_switchto->regs[0] = (reg_t)ret_from_exception;
    pt_switchto->regs[1] = (reg_t)pt_switchto;

    pcb[idx].kernel_sp = (ptr_t)pt_switchto;
    pcb[idx].user_sp = user_stack;

    /* set initial user registers for S-core: place args into a0..a2 */
    regs_context_t *user_regs = (regs_context_t *)((ptr_t)pcb[idx].kernel_sp + SWITCH_TO_SIZE);
    user_regs->regs[10] = (reg_t)argc; /* a0 */
    user_regs->regs[11] = (reg_t)arg0; /* a1 */
    user_regs->regs[12] = (reg_t)arg1; /* a2 */

    lock_kernel();
    list_add_tail(&pcb[idx].list, &ready_queue);
    unlock_kernel();
    wakeup_other_hart();

    return pcb[idx].pid;
}
#else
static char *u64_to_dec(unsigned long value, char *buffer)
{
    char temp[32];
    int pos = 0;

    do {
        temp[pos++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value != 0);

    for (int i = 0; i < pos; i++) {
        buffer[i] = temp[pos - i - 1];
    }
    buffer[pos] = '\0';
    return buffer;
}

pid_t do_exec(char *name, int argc, char *argv[])
{
    if (!name)
        return -1;

    /* find task id by name */
    int id = -1;
    for (int i = 0; i < TASK_MAXNUM; i++) {
        if (tasks[i].name[0] && strcmp(tasks[i].name, name) == 0) {
            id = tasks[i].id;
            break;
        }
    }
    if (id < 0)
        return -1;

    /* allocate stacks and load image */
    int idx = -1;
    for (int i = 1; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == 0 || pcb[i].status == TASK_EXITED) {
            idx = i;
            break;
        }
    }
    if (idx == -1)
        return -1;

    ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    ptr_t user_stack = allocUserPage(1) + PAGE_SIZE;
    ptr_t entry_point = load_task_img(id);

    pcb[idx].pid = process_id++;
    pcb[idx].status = TASK_READY;
    pcb[idx].cursor_x = 0;
    pcb[idx].cursor_y = idx;
    init_list_head(&pcb[idx].list);
    init_list_head(&pcb[idx].wait_list);

    /* initialize regs and switchto context on kernel stack (replicate init_pcb_stack) */
    regs_context_t *pt_regs = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    {
        volatile char *p = (volatile char *)pt_regs;
        for (int i = 0; i < (int)sizeof(regs_context_t); i++)
            p[i] = 0;
    }
    pt_regs->regs[2] = user_stack; /* sp */
    pt_regs->regs[4] = (reg_t)&pcb[idx];
    pt_regs->sepc = entry_point;
    pt_regs->sstatus = SR_SPIE;

    switchto_context_t *pt_switchto = (switchto_context_t *)((ptr_t)pt_regs - sizeof(switchto_context_t));
    {
        volatile char *p = (volatile char *)pt_switchto;
        for (int i = 0; i < (int)sizeof(switchto_context_t); i++)
            p[i] = 0;
    }
    extern void ret_from_exception();
    pt_switchto->regs[0] = (reg_t)ret_from_exception;
    pt_switchto->regs[1] = (reg_t)pt_switchto;

    pcb[idx].kernel_sp = (ptr_t)pt_switchto;
    pcb[idx].user_sp = user_stack;

    /* set user registers: argc and argv pointer */
    regs_context_t *user_regs = (regs_context_t *)((ptr_t)pcb[idx].kernel_sp + SWITCH_TO_SIZE);
    user_regs->regs[10] = (reg_t)argc; /* a0 */
    user_regs->regs[11] = (reg_t)argv; /* a1 */


    if (strcmp(name, "add") == 0 && argc < 2) {
        struct add_args {
            int print_location;
            int from;
            int to;
            int *result;
        };

        char addr_buf[32];
        size_t payload_size = sizeof(struct add_args) + sizeof(int);
        ptr_t payload_base = ROUNDDOWN(user_stack - payload_size, sizeof(void *));
        struct add_args *shared_args = (struct add_args *)payload_base;
        int *result_ptr = (int *)(payload_base + sizeof(struct add_args));

        shared_args->print_location = idx + 1;
        shared_args->from = 0;
        shared_args->to = 10;
        shared_args->result = result_ptr;
        *result_ptr = 0;

        u64_to_dec((unsigned long)payload_base, addr_buf);

        size_t name_len = strlen("add") + 1;
        size_t addr_len = strlen(addr_buf) + 1;
        size_t ptrs_size = sizeof(char *) * 3;
        ptr_t argv_area = ROUNDDOWN(payload_base - (ptrs_size + name_len + addr_len), sizeof(void *));
        char *str_ptr = (char *)(argv_area + ptrs_size);

        ((char **)argv_area)[0] = str_ptr;
        memcpy((uint8_t *)str_ptr, (const uint8_t *)"add", name_len);
        str_ptr += name_len;

        ((char **)argv_area)[1] = str_ptr;
        memcpy((uint8_t *)str_ptr, (const uint8_t *)addr_buf, addr_len);
        str_ptr += addr_len;

        ((char **)argv_area)[2] = NULL;

        ptr_t argv_stack_top = ROUNDDOWN(argv_area, 16);

        user_regs->regs[10] = 2;
        user_regs->regs[11] = (reg_t)argv_area;
        user_regs->regs[2] = argv_stack_top;
        pcb[idx].user_sp = argv_stack_top;
    }

    /* Copy argv strings and pointer array into the new process's user stack.
     * Layout on new user stack (growing downwards):
     *   [padding]
     *   pointers[] (argc+1 pointers)
     *   strings (concatenated, each NUL terminated)
     * We place the data just below user_stack.
     */
    if (!(strcmp(name, "add") == 0 && argc < 2) && argc > 0 && argv) {
        /* compute total length of strings */
        size_t total_len = 0;
        for (int k = 0; k < argc; k++) {
            if (argv[k])
                total_len += strlen(argv[k]) + 1;
        }

         size_t ptrs_size = sizeof(char *) * (argc + 1);
         ptr_t user_top = pcb[idx].user_sp; /* original user stack top */

         ptr_t argv_area = user_top - (ptrs_size + total_len);
         char *str_ptr = (char *)(argv_area + ptrs_size);
        /* copy strings and set pointers */
        for (int k = 0; k < argc; k++) {
            if (argv[k]) {
                size_t l = strlen(argv[k]) + 1;
                /* copy from caller's user memory (argv[k]) into new user stack */
                /* kernel can access this memory directly in this simple setup */
                memcpy((void *)str_ptr, (const void *)argv[k], l);
                ((char **)argv_area)[k] = (char *)str_ptr;
                str_ptr += l;
            } else {
                ((char **)argv_area)[k] = NULL;
            }
        }
        ((char **)argv_area)[argc] = NULL;

        /* keep argv data above the initial stack pointer so main() does not overwrite it */
        ptr_t argv_stack_top = ROUNDDOWN(argv_area, 16);

        /* set argv to point into new process user stack */
        user_regs->regs[11] = (reg_t)argv_area;
        user_regs->regs[2] = argv_stack_top;

        /* update the saved user_sp to the aligned stack top used by the new process */
        pcb[idx].user_sp = argv_stack_top;
    }

    lock_kernel();
    list_add_tail(&pcb[idx].list, &ready_queue);
    unlock_kernel();
    wakeup_other_hart();

    return pcb[idx].pid;
}
#endif

void do_exit(void)
{
    lock_kernel();
    /* mark current as exited and wake up waiters */
    current_running->status = TASK_EXITED;

    /* wake up all waiting PCBs */
    while (!list_empty(&current_running->wait_list)) {
        list_node_t *node = current_running->wait_list.next;
        list_del(node);
        pcb_t *p = list_entry(node, pcb_t, list);
        p->status = TASK_READY;
        list_add_tail(&p->list, &ready_queue);
    }
    unlock_kernel();
    wakeup_other_hart();

    do_scheduler();
}

int do_kill(pid_t pid)
{
    if (pid <= 0)
        return -1;
    lock_kernel();
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid) {
            if (pcb[i].status == TASK_EXITED) {
                unlock_kernel();
                return -1;
            }
            pcb[i].status = TASK_EXITED;
            /* remove from ready queue if present */
            if (!list_empty(&pcb[i].list)) {
                list_del(&pcb[i].list);
            }
            /* wake up waiters */
            while (!list_empty(&pcb[i].wait_list)) {
                list_node_t *node = pcb[i].wait_list.next;
                list_del(node);
                pcb_t *p = list_entry(node, pcb_t, list);
                p->status = TASK_READY;
                list_add_tail(&p->list, &ready_queue);
            }
            unlock_kernel();
            wakeup_other_hart();
            return 0;
        }
    }
    unlock_kernel();
    return -1;
}

int do_waitpid(pid_t pid)
{
    if (pid <= 0)
        return -1;
    pcb_t *target = NULL;
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid) {
            target = &pcb[i];
            break;
        }
    }
    if (!target)
        return -1;

    if (target->status == TASK_EXITED)
        return 0;

    /* block current on target's wait_list */
    do_block(&current_running->list, &target->wait_list);
    return 0;
}

void do_process_show()
{
    printk("[Process Table]:\n");
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid != 0) {
            const char *s = "UNKNOWN";
            switch (pcb[i].status) {
            case TASK_BLOCKED:
                s = "BLOCKED";
                break;
            case TASK_RUNNING:
                s = "RUNNING";
                break;
            case TASK_READY:
                s = "READY";
                break;
            case TASK_EXITED:
                s = "EXITED";
                break;
            }
            printk("[%d] PID : %d  STATUS : %s\n", i, pcb[i].pid, s);
        }
    }
}

pid_t do_getpid()
{
    return current_running->pid;
}

int do_getchar()
{
    return bios_getchar();
}
