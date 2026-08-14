#include "kmalloc.h"

/* Heap lands with the MM milestone (after PMM + paging). */
void *kmalloc(size_t size)
{
    (void)size;
    return 0;
}

void kfree(void *ptr)
{
    (void)ptr;
}
