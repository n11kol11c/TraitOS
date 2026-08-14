#ifndef TVMM_H
#define TVMM_H

#include <stdint.h>

#define VMM_PAGE_PRESENT (1u << 0)
#define VMM_PAGE_WRITE   (1u << 1)
#define VMM_PAGE_USER    (1u << 2)

/* Higher-half physmap: boot.asm maps VIRT 0xFFFFFFFF80000000 + p onto
 * physical p for the whole first 1 GiB (2 MiB pages). */
#define VMM_VIRT_BASE     0xFFFFFFFF80000000ULL
#define VMM_PHYS_TO_VIRT(p) ((uintptr_t)(p) + VMM_VIRT_BASE)

void vmm_init(void);
int  vmm_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags);
void vmm_unmap_page(uintptr_t virt);

#endif
