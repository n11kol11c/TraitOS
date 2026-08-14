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

/* One address space = one independent PML4 tree. Every space clones the
 * kernel view (identity map of low memory + the higher half), so kernel
 * code and data remain visible no matter which space is active. User
 * mappings live in the untouched PML4 slots 1..255. */
typedef struct vmm_aspace {
    uintptr_t pml4_phys;      /* physical address of the PML4 */
    uint64_t *pml4;           /* kernel-visible virtual alias */
} vmm_aspace_t;

void          vmm_init(void);

vmm_aspace_t *vmm_aspace_create(void);
void          vmm_aspace_destroy(vmm_aspace_t *as);
void          vmm_aspace_switch(vmm_aspace_t *as);
vmm_aspace_t *vmm_aspace_current(void);
int           vmm_aspace_map(vmm_aspace_t *as, uintptr_t virt,
                             uintptr_t phys, uint32_t flags);
void          vmm_aspace_unmap(vmm_aspace_t *as, uintptr_t virt);

/* Map/unmap in the currently active address space. */
int  vmm_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags);
void vmm_unmap_page(uintptr_t virt);

#endif
