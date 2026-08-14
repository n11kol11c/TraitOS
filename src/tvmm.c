#include "tvmm.h"

#include "tpmm.h"
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

/* The boot page tables live below 1 GiB, and every table entry stores the
 * *physical* address of the next table. Resolve those through the
 * higher-half physmap window instead of the identity map. */
#define TABLE_OF(e) ((uint64_t *)VMM_PHYS_TO_VIRT((e) & ADDR_MASK))

static uint64_t *pml4 = NULL;

static void tlb_flush_page(uintptr_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* Fetch the child table of `table[index]`, allocating and wiring a fresh
 * frame if the slot is empty. Table frames always come from low memory so
 * they stay reachable through the physmap. */
static uint64_t *table_ensure(uint64_t *table, unsigned index, uint32_t flags)
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
    pml4 = TABLE_OF(cr3);
}

int vmm_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags)
{
    uint64_t *pdpt = table_ensure(pml4, PML4_INDEX(virt), flags);
    if (!pdpt)
        return -1;
    uint64_t *pd = table_ensure(pdpt, PDPT_INDEX(virt), flags);
    if (!pd)
        return -1;
    uint64_t *pt = table_ensure(pd, PD_INDEX(virt), flags);
    if (!pt)
        return -1;

    pt[PT_INDEX(virt)] = ((uint64_t)phys & ADDR_MASK) | flags | VMM_PAGE_PRESENT;
    tlb_flush_page(virt);
    return 0;
}

void vmm_unmap_page(uintptr_t virt)
{
    uint64_t *pdpt = table_lookup(pml4, PML4_INDEX(virt));
    if (!pdpt)
        return;
    uint64_t *pd = table_lookup(pdpt, PDPT_INDEX(virt));
    if (!pd)
        return;
    uint64_t *pt = table_lookup(pd, PD_INDEX(virt));
    if (!pt)
        return;

    pt[PT_INDEX(virt)] = 0;
    tlb_flush_page(virt);
}
