#include <os/task.h>
#include <os/loader.h>
#include <os/string.h>
#include <os/kernel.h>
#include <os/mm.h>
#include <pgtable.h>
#include <type.h>

#define OS_SIZE_LOC pa2kva(0x502001fc)
#define BOOT_SECTOR 1

uint64_t load_task_img(int taskid, uintptr_t pgdir)
{
    uint32_t kernel_secs;
    uint32_t task_secs;
    uint32_t task_start;
    uint64_t phys_load_addr;
    uint64_t user_va;

    if (taskid < 0 || taskid >= TASK_MAXNUM)
        return 0;

    kernel_secs = (uint32_t)(*(uint16_t *)OS_SIZE_LOC);
    task_secs = kernel_secs;
    task_start = BOOT_SECTOR + task_secs + (uint32_t)taskid * task_secs;
    phys_load_addr = TASK_MEM_BASE + (uint64_t)taskid * TASK_SIZE;

    /* Load task image from SD card to temporary physical memory */
    bios_sd_read((uint32_t)phys_load_addr, task_secs, task_start);

    user_va = USER_ENTRYPOINT + (uint64_t)taskid * TASK_SIZE;
    uintptr_t src_kva  = pa2kva(phys_load_addr);

    if (pgdir == PGDIR_PA) {
        /* Shared page directory — use the pre-mapped 2MB large pages */
        uintptr_t dest_kva = pa2kva(0x52000000lu + user_va);
        memcpy((void *)dest_kva, (const void *)src_kva, TASK_SIZE);
    } else {
        /* Per-process page table — allocate private pages via alloc_page_helper */
        for (uint64_t offset = 0; offset < TASK_SIZE; offset += NORMAL_PAGE_SIZE) {
            uintptr_t page_kva = alloc_page_helper(user_va + offset, pgdir);
            memcpy((void *)page_kva, (void *)(src_kva + offset), NORMAL_PAGE_SIZE);
        }
    }
    asm volatile("fence.i" ::: "memory");

    return user_va;
}
