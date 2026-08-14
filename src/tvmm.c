#include "tvmm.h"

#include "tpmm.h"
#include "tmalloc.h"
#include "tstring.h"

#include <stdint.h>

#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT   21
#define PT_SHIFT   12

#define PML4_INDEX(v) (((v) >> PML4_SHIFT) & 0x1FF)
#define PDPT_INDEX(v) (((v) >> PDPT_SHIFT) & 0x1FF)
#define PD_INDEX(v)   (((v) >> PD_SHIFT) & 0x1FF)
#define PT_INDEX(v)   (((v) >> PT_SHIFT) & 0x1FF)

#define ADDR_MASK 0x000FFFFFFFFFF000ULL

/* PML4 slots 0 (identity map of low memory) and 256..511 (higher half)
 * form the kernel view, shared read-only by every address space. */
#define KERNEL_PML4_START 256

/* The boot page tables live below 1 GiB, and every table entry stores the
 * *physical* address of the next table. Resolve those through the
 * higher-half physmap window instead of the identity map. */
#define TABLE_OF(e) ((uint64_t *)VMM_PHYS_TO_VIRT((e) & ADDR_MASK))

static vmm_aspace_t kernel_aspace;
static vmm_aspace_t *current = &kernel_aspace;

static void tlb_flush_page(uintptr_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* Fetch the child table of `table[index]`, allocating and wiring a fresh
 * frame if the slot is empty. Table frames always come from low memory so
 * they stay reachable through the physmap. */
static uint64_t *table_ensure(uint64_t *table, unsigned index, uint64_t flags)
{
    uint64_t e = table[index];
    if (e & VMM_PAGE_PRESENT)
        return TABLE_OF(e);

    uintptr_t frame = tpmm_alloc_low();
    if (!frame)
        return NULL;

    uint64_t *next = TABLE_OF(frame);
    tmemset(next, 0, 4096);
    table[index] = frame | VMM_PAGE_PRESENT | VMM_PAGE_WRITE |
                   (flags & VMM_PAGE_USER);
    return next;
}

/* Read-only variant: no allocation, NULL if the slot is empty. */
static uint64_t *table_lookup(uint64_t *table, unsigned index)
{
    uint64_t e = table[index];
    if (!(e & VMM_PAGE_PRESENT))
        return NULL;
    return TABLE_OF(e);
}

void vmm_init(void)
{
    uintptr_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    kernel_aspace.pml4_phys = cr3;
    kernel_aspace.pml4 = TABLE_OF(cr3);
    current = &kernel_aspace;
}

vmm_aspace_t *vmm_aspace_create(void)
{
    uintptr_t frame = tpmm_alloc_low();
    if (!frame)
        return NULL;

    uint64_t *pml4 = TABLE_OF(frame);
    tmemset(pml4, 0, 4096);

    /* share the kernel view: identity map (slot 0) + higher half (256..511) */
    for (int i = 0; i < 512; i++)
        if (i == 0 || i >= KERNEL_PML4_START)
            pml4[i] = kernel_aspace.pml4[i];

    vmm_aspace_t *as = (vmm_aspace_t *)tmalloc(sizeof(vmm_aspace_t));
    if (!as) {
        tpmm_free(frame);
        return NULL;
    }
    as->pml4_phys = frame;
    as->pml4 = pml4;
    return as;
}

/* Free a page-table frame, descending into children unless the entry is a
 * huge page (PS bit) or a leaf. Only used for user-space (non-shared) tables. */
static void free_table(uint64_t *table, int level)
{
    for (int i = 0; i < 512; i++) {
        uint64_t e = table[i];
        if (!(e & VMM_PAGE_PRESENT))
            continue;
        if (level > 0 && !(e & (1ull << 7)))
            free_table(TABLE_OF(e), level - 1);
        tpmm_free(e & ADDR_MASK);
    }
}

void vmm_aspace_destroy(vmm_aspace_t *as)
{
    if (!as || as == current)
        return;

    /* the kernel half (slot 0 + 256..511) is shared, not owned; free only
     * the user slots 1..255 that this space mapped on its own */
    for (int i = 1; i < KERNEL_PML4_START; i++) {
        uint64_t e = as->pml4[i];
        if (e & VMM_PAGE_PRESENT)
            free_table(TABLE_OF(e), 2);
    }
    tpmm_free(as->pml4_phys);
    tfree(as);
}

void vmm_aspace_switch(vmm_aspace_t *as)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(as->pml4_phys) : "memory");
    current = as;
}

vmm_aspace_t *vmm_aspace_current(void)
{
    return current;
}

vmm_aspace_t *vmm_aspace_kernel(void)
{
    return &kernel_aspace;
}

/* Is the page covering `virt` mapped in the current address space with the
 * required permission bits set? Walks the current PML4 by hand so it works
 * on any space (the shared kernel slots included). */
static int page_has(uintptr_t virt, uint64_t need)
{
    uint64_t *pdpt = table_lookup(current->pml4, PML4_INDEX(virt));
    if (!pdpt)
        return 0;
    uint64_t *pd = table_lookup(pdpt, PDPT_INDEX(virt));
    if (!pd)
        return 0;
    uint64_t pe = pd[PD_INDEX(virt)];
    if (pe & (1ull << 7))   /* 2 MiB huge page */
        return (pe & need) == need;
    uint64_t *pt = table_lookup(pd, PD_INDEX(virt));
    if (!pt)
        return 0;
    return (pt[PT_INDEX(virt)] & need) == need;
}

int vmm_range_user(uintptr_t addr, size_t len, int write)
{
    uint64_t need = VMM_PAGE_PRESENT | VMM_PAGE_USER;
    if (write)
        need |= VMM_PAGE_WRITE;

    if (len == 0)
        return 1;
    if (addr < VMM_USER_BASE)
        return 0;
    if (addr > VMM_USER_END || len > VMM_USER_END - addr)
        return 0;

    uintptr_t start = addr & ~(uintptr_t)0xFFF;
    uintptr_t end = addr + len;
    for (uintptr_t p = start; p < end; p += 0x1000)
        if (!page_has(p, need))
            return 0;
    return 1;
}

/* Only the user slots (1..255) may be mapped/unmapped through an address
 * space. Slots 0 and 256..511 are the shared kernel view (identity map +
 * higher half); touching them here would corrupt the kernel's page tables
 * for *every* address space. */
static int kernel_slot(unsigned virt_index)
{
    return virt_index == 0 || virt_index >= KERNEL_PML4_START;
}

int vmm_aspace_map(vmm_aspace_t *as, uintptr_t virt, uintptr_t phys,
                   uint64_t flags)
{
    if (kernel_slot(PML4_INDEX(virt)))
        return -1;

    uint64_t *pdpt = table_ensure(as->pml4, PML4_INDEX(virt), flags);
    if (!pdpt)
        return -1;
    uint64_t *pd = table_ensure(pdpt, PDPT_INDEX(virt), flags);
    if (!pd)
        return -1;
    uint64_t *pt = table_ensure(pd, PD_INDEX(virt), flags);
    if (!pt)
        return -1;

    pt[PT_INDEX(virt)] = ((uint64_t)phys & ADDR_MASK) | flags | VMM_PAGE_PRESENT;
    if (as == current)
        tlb_flush_page(virt);
    return 0;
}

void vmm_aspace_unmap(vmm_aspace_t *as, uintptr_t virt)
{
    if (kernel_slot(PML4_INDEX(virt)))
        return;

    uint64_t *pdpt = table_lookup(as->pml4, PML4_INDEX(virt));
    if (!pdpt)
        return;
    uint64_t *pd = table_lookup(pdpt, PDPT_INDEX(virt));
    if (!pd)
        return;
    uint64_t *pt = table_lookup(pd, PD_INDEX(virt));
    if (!pt)
        return;

    pt[PT_INDEX(virt)] = 0;
    if (as == current)
        tlb_flush_page(virt);
}

int vmm_map_page(uintptr_t virt, uintptr_t phys, uint64_t flags)
{
    return vmm_aspace_map(current, virt, phys, flags);
}

void vmm_unmap_page(uintptr_t virt)
{
    vmm_aspace_unmap(current, virt);
}
