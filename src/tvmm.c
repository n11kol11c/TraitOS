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

static uint64_t *pml4 = NULL;

void vmm_init(void)
{
    __asm__ volatile("mov %%cr3, %0" : "=r"(pml4));
}

int vmm_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags)
{
    uint64_t entry = ((uint64_t)phys & ADDR_MASK) | flags | VMM_PAGE_PRESENT;
    uint64_t *pdpt, *pd, *pt;

    if (!(pml4[PML4_INDEX(virt)] & 1)) {
        pdpt = (uint64_t *)tpmm_alloc();
        if (!pdpt)
            return -1;
        tmemset(pdpt, 0, 4096);
        pml4[PML4_INDEX(virt)] = (uint64_t)pdpt | 3;
    } else {
        pdpt = (uint64_t *)(pml4[PML4_INDEX(virt)] & ADDR_MASK);
    }

    if (!(pdpt[PDPT_INDEX(virt)] & 1)) {
        pd = (uint64_t *)tpmm_alloc();
        if (!pd)
            return -1;
        tmemset(pd, 0, 4096);
        pdpt[PDPT_INDEX(virt)] = (uint64_t)pd | 3;
    } else {
        pd = (uint64_t *)(pdpt[PDPT_INDEX(virt)] & ADDR_MASK);
    }

    if (!(pd[PD_INDEX(virt)] & 1)) {
        pt = (uint64_t *)tpmm_alloc();
        if (!pt)
            return -1;
        tmemset(pt, 0, 4096);
        pd[PD_INDEX(virt)] = (uint64_t)pt | 3;
    } else {
        pt = (uint64_t *)(pd[PD_INDEX(virt)] & ADDR_MASK);
    }

    pt[PT_INDEX(virt)] = entry;
    __asm__ volatile("invlpg (%0)" : : "r"(virt));
    return 0;
}

void vmm_unmap_page(uintptr_t virt)
{
    uint64_t *pdpt = (uint64_t *)(pml4[PML4_INDEX(virt)] & ADDR_MASK);
    uint64_t *pd   = (uint64_t *)(pdpt[PDPT_INDEX(virt)] & ADDR_MASK);
    uint64_t *pt   = (uint64_t *)(pd[PD_INDEX(virt)] & ADDR_MASK);
    pt[PT_INDEX(virt)] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(virt));
}
