#ifndef TPMM_H
#define TPMM_H

#include <stdint.h>
#include <stddef.h>

void      tpmm_init(uintptr_t mbi);
uintptr_t tpmm_alloc(void);
uintptr_t tpmm_alloc_contig(size_t pages);
uintptr_t tpmm_alloc_low(void);
uintptr_t tpmm_alloc_low_contig(size_t pages);
void      tpmm_free(uintptr_t phys);
uint32_t  tpmm_free_frames(void);
uint32_t  tpmm_used_frames(void);
uint64_t  tpmm_total_mem(void);
uint64_t  tpmm_available_mem(void);

/* Frame/region introspection for hardening + RAM scrubbing. */
int  tpmm_frame_used(uintptr_t phys);
void tpmm_reserve_range(uintptr_t base, size_t size);
void tpmm_release_range(uintptr_t base, size_t size);

#endif
