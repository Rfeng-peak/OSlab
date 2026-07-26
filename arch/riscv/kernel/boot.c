/* RISC-V kernel boot stage */
#include <pgtable.h>
#include <asm.h>

#define ARRTIBUTE_BOOTKERNEL __attribute__((section(".bootkernel")))

typedef void (*kernel_entry_t)(unsigned long);

/********* setup memory mapping ***********/
static uintptr_t ARRTIBUTE_BOOTKERNEL alloc_page()
{
    static uintptr_t pg_base = PGDIR_PA;
    pg_base += 0x1000;
    return pg_base;
}

// using 2MB large page
static void ARRTIBUTE_BOOTKERNEL map_page(uint64_t va, uint64_t pa, PTE *pgdir)
{
    va &= VA_MASK;
    uint64_t vpn2 =
        va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^
                    (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    if (pgdir[vpn2] == 0) {
        // alloc a new second-level page directory
        set_pfn(&pgdir[vpn2], alloc_page() >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn2], _PAGE_PRESENT);
        clear_pgdir(get_pa(pgdir[vpn2]));
    }
    PTE *pmd = (PTE *)get_pa(pgdir[vpn2]);
    set_pfn(&pmd[vpn1], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &pmd[vpn1], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                        _PAGE_EXEC | _PAGE_ACCESSED | _PAGE_DIRTY);
}

static void ARRTIBUTE_BOOTKERNEL enable_vm()
{
    /* 1. Ensure all page-table writes are globally visible before TLB flush */
    asm volatile("fence w,w" ::: "memory");

    /* 2. Write satp to enable paging */
    set_satp(SATP_MODE_SV39, 0, PGDIR_PA >> NORMAL_PAGE_SHIFT);
    local_flush_tlb_all();

    /* 3. Pre-touch kernel code+data region (128KB) AND sample the rest
     *    at 2MB granularity (which is what the TLB caches for large pages).
     *    This warms enough of the TLB without overflowing QEMU's TLB cache. */
    for (uintptr_t _va = 0xffffffc050201000;
         _va < 0xffffffc050300000; _va += 0x1000) {
        volatile uint64_t *_p = (volatile uint64_t *)_va;
        (void)*_p;
    }
    /* Also touch the identity-mapped region */
    for (uintptr_t _va2 = 0x50200000; _va2 < 0x50300000; _va2 += 0x1000) {
        volatile uint64_t *_p2 = (volatile uint64_t *)_va2;
        (void)*_p2;
    }


}

/* Sv-39 mode
 * 0x0000_0000_0000_0000-0x0000_003f_ffff_ffff is for user mode
 * 0xffff_ffc0_0000_0000-0xffff_ffff_ffff_ffff is for kernel mode
 */
static void ARRTIBUTE_BOOTKERNEL setup_vm()
{
    clear_pgdir(PGDIR_PA);
    // map kernel virtual address(kva) to kernel physical
    // address(kpa) kva = kpa + 0xffff_ffc0_0000_0000 use 2MB page,
    // map all physical memory
    PTE *early_pgdir = (PTE *)PGDIR_PA;
    for (uint64_t kva = 0xffffffc050000000lu;
         kva < 0xffffffc060000000lu; kva += 0x200000lu) {
        map_page(kva, kva2pa(kva), early_pgdir);
    }
    // map boot address
    for (uint64_t pa = 0x50000000lu; pa < 0x51000000lu;
         pa += 0x200000lu) {
        map_page(pa, pa, early_pgdir);
    }

    // map user code + stack at 0x0 - 0x40000000 (VPN2=0, 1GB)
    // using 2MB large pages with _PAGE_USER, pointing to physical 0x52000000+
    for (uint64_t uva = 0x0lu; uva < 0x40000000lu; uva += 0x200000lu) {
        map_page(uva, 0x52000000lu + uva, early_pgdir);
        // Set _PAGE_USER on the just-created mapping for user access
        uint64_t _vpn2 = uva >> 30;
        uint64_t _vpn1 = (_vpn2 << 9) ^ (uva >> 21);
        PTE *_pmd = (PTE *)get_pa(early_pgdir[_vpn2]);
        set_attribute(&_pmd[_vpn1], _PAGE_USER);
    }

    // map E1000 / PLIC MMIO regions AFTER user space to overwrite
    // any _PAGE_USER flags on overlapping VPN entries (kernel needs
    // to access these without SUM set during early init).
    map_page(0x60000000lu, 0x60000000lu, early_pgdir);  // E1000
    map_page(0xc000000lu, 0xc000000lu, early_pgdir);     // PLIC priority/enable
    map_page(0xc200000lu, 0xc200000lu, early_pgdir);     // PLIC context

    /* Clear _PAGE_USER on IO mappings that overlap user space VPN range.
     * map_page() ORs attributes, so _PAGE_USER from the user loop persists. */
    for (uint64_t iova = 0xc000000lu; iova <= 0xc200000lu; iova += 0x200000lu) {
        uint64_t v2 = iova >> 30;
        uint64_t v1 = (iova >> 21) & (NUM_PTE_ENTRY - 1);
        PTE *pmd = (PTE *)get_pa(early_pgdir[v2]);
        pmd[v1] &= ~_PAGE_USER;
    }
    /* E1000 at 0x60000000 has VPN2=0x180 which is outside user range, safe. */

    enable_vm();
}

extern uintptr_t _start[];

/*********** start here **************/
int ARRTIBUTE_BOOTKERNEL boot_kernel(unsigned long mhartid)
{
    asm volatile("fence.i" ::: "memory");
    if (mhartid == 0) {
        setup_vm();
    } else {
        enable_vm();
    }

    /* enter kernel via its virtual address (VM already enabled) */
    ((kernel_entry_t)_start)(mhartid);

    return 0;
}
