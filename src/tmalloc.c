#include "tmalloc.h"

/* Heap lands with the MM milestone (after PMM + paging). */
void *tmalloc(size_t size)
{
    (void)size;
    return 0;
}

void tfree(void *ptr)
{
    (void)ptr;
}
