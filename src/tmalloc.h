#ifndef TMALLOC_H
#define TMALLOC_H

#include <stddef.h>

void *tmalloc(size_t size);
void  tfree(void *ptr);

#endif
