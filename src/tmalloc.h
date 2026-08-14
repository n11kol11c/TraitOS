#ifndef TMALLOC_H
#define TMALLOC_H

#include <stddef.h>
#include <stdint.h>

void     *tmalloc(size_t size);
void      tfree(void *ptr);
uint64_t  tmalloc_total(void);
uint64_t  tmalloc_used(void);
uint32_t  tmalloc_blocks(void);

#endif
