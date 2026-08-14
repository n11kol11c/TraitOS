#ifndef TPMM_H
#define TPMM_H

#include <stdint.h>
#include <stddef.h>

void      tpmm_init(uintptr_t mbi);
uintptr_t tpmm_alloc(void);
void      tpmm_free(uintptr_t phys);
uint32_t  tpmm_free_frames(void);
uint32_t  tpmm_used_frames(void);
uint64_t  tpmm_total_mem(void);
uint64_t  tpmm_available_mem(void);

#endif
