#include <os/mm.h>
#include <os/string.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void freePage(ptr_t baseAddr)
{
    (void)baseAddr;
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    size = ROUND(size, PAGE_SIZE);
    ptr_t ret = allocPage((int)(size / PAGE_SIZE));
    return (void *)ret;
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    PTE *dest = (PTE *)pa2kva(dest_pgdir);
    PTE *src = (PTE *)pa2kva(src_pgdir);

    for (int i = NUM_PTE_ENTRY / 2; i < NUM_PTE_ENTRY; i++) {
        dest[i] = src[i];
    }
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    va &= VA_MASK;

    PTE *root = (PTE *)pa2kva(pgdir);
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & (NUM_PTE_ENTRY - 1);
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & (NUM_PTE_ENTRY - 1);

    if (!(root[vpn2] & _PAGE_PRESENT)) {
        uintptr_t new_table_kva = allocPage(1);
        memset((void *)new_table_kva, 0, PAGE_SIZE);
        set_pfn(&root[vpn2], kva2pa(new_table_kva) >> NORMAL_PAGE_SHIFT);
        set_attribute(&root[vpn2], _PAGE_PRESENT);
    }

    PTE *l1 = (PTE *)pa2kva(get_pa(root[vpn2]));
    if (!(l1[vpn1] & _PAGE_PRESENT)) {
        uintptr_t new_table_kva = allocPage(1);
        memset((void *)new_table_kva, 0, PAGE_SIZE);
        set_pfn(&l1[vpn1], kva2pa(new_table_kva) >> NORMAL_PAGE_SHIFT);
        set_attribute(&l1[vpn1], _PAGE_PRESENT);
    }

    PTE *l0 = (PTE *)pa2kva(get_pa(l1[vpn1]));
    if (!(l0[vpn0] & _PAGE_PRESENT)) {
        uintptr_t page_kva = allocPage(1);
        memset((void *)page_kva, 0, PAGE_SIZE);
        set_pfn(&l0[vpn0], kva2pa(page_kva) >> NORMAL_PAGE_SHIFT);
        set_attribute(&l0[vpn0],
                      _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE |
                          _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED |
                          _PAGE_DIRTY);
        local_flush_tlb_page(va);
        return page_kva;
    }

    return pa2kva(get_pa(l0[vpn0]));
}

uintptr_t shm_page_get(int key)
{
    (void)key;
    return 0;
}

void shm_page_dt(uintptr_t addr)
{
    (void)addr;
}
