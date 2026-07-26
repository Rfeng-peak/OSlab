#include <os/list.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/sched.h>
#include <os/task.h>
#include <os/time.h>
#include <os/mm.h>
#include <os/loader.h>
#include <screen.h>
#include <os/smp.h>
#include <os/string.h>
#include <printk.h>
#include <assert.h>

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

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    // check_sleeping(); // P4: skip pending investigation of sleep queue issue

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.
    if (list_empty(&ready_queue)) {
        return;
    }

    pcb_t *prev_running = current_running;
    if (prev_running != &pid0_pcb && prev_running->status == TASK_RUNNING) {
        prev_running->status = TASK_READY;
        list_add_tail(&prev_running->list, &ready_queue);
    }

    pcb_t *next_running = list_entry(ready_queue.next, pcb_t, list);
    list_del(&next_running->list);
    next_running->status = TASK_RUNNING;
    current_running = next_running;

    unlock_kernel();
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
}

/* P3-TASK1: process management helpers */
pid_t do_exec(long id_or_ptr, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    int id;

    if (id_or_ptr >= 0 && id_or_ptr < TASK_MAXNUM) {
        id = (int)id_or_ptr;
    } else if (id_or_ptr != 0) {
        /* treat as user-space pointer to task name */
        char buf[32];
        const char *src = (const char *)id_or_ptr;
        int i;
        for (i = 0; i < (int)sizeof(buf) - 1; i++) {
            buf[i] = src[i];
            if (buf[i] == '\0') break;
        }
        buf[i] = '\0';
        id = -1;
        for (i = 0; i < TASK_MAXNUM; i++) {
            if (tasks[i].name[0] && strcmp(tasks[i].name, buf) == 0) {
                id = tasks[i].id;
                break;
            }
        }
        if (id < 0) return -1;
    } else {
        return -1;
    }

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

    /* Share kernel page table (user space pre-mapped by setup_vm) */
    uintptr_t pgdir = PGDIR_PA;

    ptr_t kernel_stack = allocKernelPage(1) + PAGE_SIZE;
    uint64_t ustack_va = USER_STACK_ADDR + idx * PAGE_SIZE;
    alloc_page_helper(ustack_va, pgdir);
    ptr_t user_stack = ustack_va + PAGE_SIZE;
    ptr_t entry_point = load_task_img(id, pgdir);

    pcb[idx].pid = process_id++;
    pcb[idx].status = TASK_READY;
    pcb[idx].cursor_x = 0;
    pcb[idx].cursor_y = idx;
    pcb[idx].pgdir = pgdir;
    init_list_head(&pcb[idx].list);
    init_list_head(&pcb[idx].wait_list);
    init_pcb_stack(kernel_stack, user_stack, entry_point, &pcb[idx]);
    /* Provide minimal default argv: argc=1, argv[0]="", argv[1]=NULL.
     * P4 special-cased id==3 (rw); P5 programs don't need that. */
    {
        regs_context_t *ur = (regs_context_t *)(pcb[idx].kernel_sp + sizeof(switchto_context_t));
        uintptr_t usp = ur->regs[2];
        usp = ROUNDDOWN(usp - 64, 16);
        char *stk = (char *)usp;
        stk[0] = '\0';  /* empty program name */
        uintptr_t *argv_ptrs = (uintptr_t *)(stk + 8);
        argv_ptrs[0] = (uintptr_t)stk;  /* argv[0] = "" */
        argv_ptrs[1] = 0;               /* argv[1] = NULL */
        ur->regs[10] = 1;  /* argc = 1 */
        ur->regs[11] = (reg_t)argv_ptrs;
        ur->regs[2]  = usp;
    }
    lock_kernel();
    list_add_tail(&pcb[idx].list, &ready_queue);
    unlock_kernel();
    asm volatile("fence.i" ::: "memory");
    bios_putchar('X');

    return pcb[idx].pid;
}

void do_exit(void)
{
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

    do_scheduler();
}

int do_kill(pid_t pid)
{
    if (pid <= 0)
        return -1;
    for (int i = 0; i < NUM_MAX_TASK; i++) {
        if (pcb[i].pid == pid) {
            if (pcb[i].status == TASK_EXITED)
                return -1;
            pcb[i].status = TASK_EXITED;
            /* remove from any queue (list_del on self-linked node is a no-op) */
            list_del(&pcb[i].list);
            /* wake up waiters */
            while (!list_empty(&pcb[i].wait_list)) {
                list_node_t *node = pcb[i].wait_list.next;
                list_del(node);
                pcb_t *p = list_entry(node, pcb_t, list);
                p->status = TASK_READY;
                list_add_tail(&p->list, &ready_queue);
            }
            /* if killing self, yield immediately */
            if (&pcb[i] == current_running) {
                do_scheduler();
            }
            return 0;
        }
    }
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
    printk("[PS] PID\tSTATUS\n");
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
            printk("[PS] %d\t%s\n", pcb[i].pid, s);
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
