#ifndef TVMM_H
#define TVMM_H

#include <stdint.h>
#include <stddef.h>

#define VMM_PAGE_PRESENT (1ull << 0)
#define VMM_PAGE_WRITE   (1ull << 1)
#define VMM_PAGE_USER    (1ull << 2)
#define VMM_PAGE_NX      (1ull << 63)

/* Higher-half physmap: boot.asm maps VIRT 0xFFFFFFFF80000000 + p onto
 * physical p for the whole first 1 GiB (2 MiB pages). */
#define VMM_VIRT_BASE     0xFFFFFFFF80000000ULL
#define VMM_PHYS_TO_VIRT(p) ((uintptr_t)(p) + VMM_VIRT_BASE)

/* User-program region: PML4 slot 1, which every address space leaves
 * unmapped for user code to own (slots 0 and 256..511 are the shared
 * kernel view). */
#define VMM_USER_BASE 0x0000008000000000ULL
#define VMM_USER_END  0x0000010000000000ULL

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
vmm_aspace_t *vmm_aspace_kernel(void);
int           vmm_aspace_map(vmm_aspace_t *as, uintptr_t virt,
                             uintptr_t phys, uint64_t flags);
void          vmm_aspace_unmap(vmm_aspace_t *as, uintptr_t virt);

/* Return 1 and the mapped physical frame if `virt` is present in `as`
 * (user slots only); 0 otherwise. */
int vmm_aspace_phys(vmm_aspace_t *as, uintptr_t virt, uintptr_t *phys);

/* Validate that [addr, addr+len) lies inside the user region and that every
 * page it touches is mapped present + user (and writable when `write` is
 * set) in the currently active address space. Safe to call on untrusted
 * ring-3 pointers before any stac/clac copy. */
int vmm_range_user(uintptr_t addr, size_t len, int write);

/* Map/unmap in the currently active address space. */
int  vmm_map_page(uintptr_t virt, uintptr_t phys, uint64_t flags);
void vmm_unmap_page(uintptr_t virt);

#endif
