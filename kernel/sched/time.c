#include <os/list.h>
#include <os/sched.h>
#include <type.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    list_node_t *node, *next;
    uint64_t now = get_timer();

    list_for_each_safe(node, next, &sleep_queue)
    {
        pcb_t *task = list_entry(node, pcb_t, list);
        if (task->wakeup_time <= now) {
            /* inline do_unblock to avoid re-entering kernel lock */
            list_del(node);
            task->status = TASK_READY;
            list_add_tail(node, &ready_queue);
        }
    }
}