#ifndef TSS_H
#define TSS_H

#include <stdint.h>

void      tss_init(void);
void      tss_set_rsp0(uint64_t rsp0);
uintptr_t tss_base(void);
uint32_t  tss_limit(void);

#endif
