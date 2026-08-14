#ifndef TVMM_H
#define TVMM_H

#include <stdint.h>

#define VMM_PAGE_PRESENT (1u << 0)
#define VMM_PAGE_WRITE   (1u << 1)
#define VMM_PAGE_USER    (1u << 2)

void vmm_init(void);
int  vmm_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags);
void vmm_unmap_page(uintptr_t virt);

#endif
