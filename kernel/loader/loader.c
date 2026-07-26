#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

#define OS_SIZE_LOC 0x502001fc
#define BOOT_SECTOR 1

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    uint16_t kernel_secs;
    uint32_t task_secs;
    uint32_t task_start;

    if (taskid < 0 || taskid >= TASK_MAXNUM) {
        return 0;
    }

    kernel_secs = *(uint16_t *)OS_SIZE_LOC;
    task_secs = (uint32_t)kernel_secs;
    task_start = BOOT_SECTOR + task_secs + taskid * task_secs;

    bios_sd_read(TASK_MEM_BASE, task_secs, task_start);

    /* The task image is always loaded to the same address.
     * Flush the instruction cache so the CPU does not execute stale code
     * from the previous task image.
     */
    asm volatile("fence.i" ::: "memory");

    return TASK_MEM_BASE;
}