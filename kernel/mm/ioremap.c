#include <os/ioremap.h>
#include <os/mm.h>
#include <os/string.h>
#include <pgtable.h>
#include <type.h>
#include <printk.h>

/* IO virtual address allocator — start from IO_ADDR_START */
static uintptr_t io_base = IO_ADDR_START;

void *ioremap(unsigned long phys_addr, unsigned long size)
{
    uintptr_t pa = ROUNDDOWN(phys_addr, PAGE_SIZE);
    uintptr_t end = ROUND(phys_addr + size, PAGE_SIZE);
    uintptr_t va = io_base;
    uintptr_t pgdir_kva = pa2kva(PGDIR_PA);
    PTE *pgdir = (PTE *)pgdir_kva;

    printk("> [IOREMAP] pa=0x%lx size=%lu -> va=0x%lx\n", phys_addr, size, va);

    for (uintptr_t offset = 0; pa + offset < end; offset += PAGE_SIZE) {
        uintptr_t v = va + offset;
        uintptr_t p = pa + offset;

        uint64_t vpn2 = v >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        uint64_t vpn1 = (v >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & (NUM_PTE_ENTRY - 1);
        uint64_t vpn0 = (v >> NORMAL_PAGE_SHIFT) & (NUM_PTE_ENTRY - 1);

        /* Allocate intermediate page directory (level-1) if needed */
        if (!(pgdir[vpn2] & _PAGE_PRESENT)) {
            uintptr_t t = allocPage(1);
            memset((void *)t, 0, PAGE_SIZE);
            set_pfn(&pgdir[vpn2], kva2pa(t) >> NORMAL_PAGE_SHIFT);
            set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
        }

        /* Access level-1 (PMD) page table */
        uintptr_t l1_pa = get_pa(pgdir[vpn2]);
        PTE *l1 = (PTE *)pa2kva(l1_pa);

        /* Allocate level-0 page table if needed */
        if (!(l1[vpn1] & _PAGE_PRESENT)) {
            uintptr_t t = allocPage(1);
            memset((void *)t, 0, PAGE_SIZE);
            set_pfn(&l1[vpn1], kva2pa(t) >> NORMAL_PAGE_SHIFT);
            set_attribute(&l1[vpn1], _PAGE_PRESENT);
        }

        /* Map IO physical page to the level-0 PTE */
        uintptr_t l0_pa = get_pa(l1[vpn1]);
        PTE *l0 = (PTE *)pa2kva(l0_pa);
        set_pfn(&l0[vpn0], p >> NORMAL_PAGE_SHIFT);
        set_attribute(&l0[vpn0],
                      _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                      _PAGE_ACCESSED | _PAGE_DIRTY);
        local_flush_tlb_page(v);
    }

    io_base = va + (end - pa);
    return (void *)(va + (phys_addr & (PAGE_SIZE - 1)));
}

void iounmap(void *io_addr)
{
    /* For S-core with shared page table, IO mappings persist. */
    printk("> [IOUNMAP] va=0x%lx (no-op for shared pgdir)\n", (uintptr_t)io_addr);
    (void)io_addr;
}
